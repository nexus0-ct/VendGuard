#include "impact.h"
#include <string.h>
static int64_t distance_squared(const int32_t a[3], const int32_t b[3])
{
  int64_t sum = 0;
  for (size_t i = 0; i < 3; ++i) { int64_t delta = (int64_t)a[i] - b[i]; sum += delta * delta; }
  return sum;
}
void impact_reset(impact_detector_t *d) { memset(d, 0, sizeof(*d)); }
void impact_sample(impact_detector_t *d, event_log_t *log, uint32_t now, const int32_t mg[3])
{
  if (!d->ready) {
    const int32_t zero[3] = {0};
    int64_t norm = distance_squared(mg, zero);
    if (norm < 810000 || norm > 1210000) { impact_reset(d); return; }
    bool moving = false;
    for (size_t i = 0; i < 3; ++i) {
      if (!d->samples) d->minimum[i] = d->maximum[i] = mg[i];
      if (mg[i] < d->minimum[i]) d->minimum[i] = mg[i];
      if (mg[i] > d->maximum[i]) d->maximum[i] = mg[i];
      if (d->maximum[i] - d->minimum[i] > 100) moving = true;
    }
    if (moving) { impact_reset(d); return; }
    for (size_t i = 0; i < 3; ++i) d->sums[i] += mg[i];
    if (++d->samples < 100) return;
    for (size_t i = 0; i < 3; ++i) d->background[i] = (int32_t)(d->sums[i] / 100);
    d->ready = true;
    log_event(log, now, BACKGROUND_READY, VG_MPU, 100);
    memcpy(d->previous, mg, sizeof(d->previous));
    d->previous_valid = true;
    return;
  }
  int64_t deviation = distance_squared(mg, d->background);
  int64_t step = d->previous_valid ? distance_squared(mg, d->previous) : 0;
  memcpy(d->previous, mg, sizeof(d->previous));
  d->previous_valid = true;
  if (!d->latched && deviation >= 650LL*650 && step >= 250LL*250) {
    d->latched = true;
    d->quiet = false;
    log_event(log, now, IMPACT_ALARM, VG_MPU, (int32_t)deviation);
  }
  if (d->latched) {
    if (deviation <= 150LL*150) {
      if (!d->quiet) { d->quiet = true; d->quiet_since = now; }
      if ((uint32_t)(now - d->quiet_since) >= 300U) { d->latched = false; d->quiet = false; }
    } else d->quiet = false;
  }
}

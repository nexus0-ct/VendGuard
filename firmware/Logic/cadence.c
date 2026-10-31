#include "cadence.h"
#include <string.h>
void vg_span_add(vg_span_t *s, uint32_t us)
{
  if (!s->n || us < s->min) s->min = us;
  if (us > s->max) s->max = us;
  s->sum += us;
  ++s->n;
}
void vg_cadence_init(vg_cadence_t *t, uint32_t now, uint32_t period, uint32_t deadline)
{
  memset(t, 0, sizeof(*t));
  t->period = period; t->deadline = deadline; t->next = now + period;
}
bool vg_cadence_due(vg_cadence_t *t, uint32_t now)
{
  if ((int32_t)(now - t->next) < 0) return false;
  uint32_t delay = now - t->next;
  uint32_t omitted = delay / t->period;
  t->skipped += omitted;
  t->release = t->next + omitted * t->period;
  t->next = t->release + t->period;
  if (delay > t->lateness_max) t->lateness_max = delay;
  if (t->has_previous) {
    int32_t j = (int32_t)(now - t->previous) - (int32_t)t->period;
    if (!t->jitter_n || j < t->jitter_min) t->jitter_min = j;
    if (!t->jitter_n || j > t->jitter_max) t->jitter_max = j;
    ++t->jitter_n;
    t->jitter_sum += j;
    uint32_t magnitude = j < 0 ? (uint32_t)(-(int64_t)j) : (uint32_t)j;
    ++t->jitter_bins[magnitude<=10 ? 0 : magnitude<=100 ? 1 : magnitude<=1000 ? 2 : 3];
  }
  t->previous = now; t->has_previous = true;
  return true;
}
void vg_cadence_finish(vg_cadence_t *t, uint32_t start, uint32_t end)
{
  vg_span_add(&t->execution, end - start);
  if ((uint32_t)(end - t->release) > t->deadline) ++t->late;
}

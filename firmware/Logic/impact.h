#ifndef VG_IMPACT_H
#define VG_IMPACT_H
#include "event_log.h"
#include <stdbool.h>
typedef struct {
  int32_t background[3], minimum[3], maximum[3], previous[3];
  int64_t sums[3];
  uint16_t samples;
  bool ready, latched, quiet, previous_valid;
  uint32_t quiet_since;
} impact_detector_t;
void impact_reset(impact_detector_t *detector);
void impact_sample(impact_detector_t *detector, event_log_t *log, uint32_t now, const int32_t mg[3]);
#endif

#ifndef VG_CADENCE_H
#define VG_CADENCE_H
#include <stdbool.h>
#include <stdint.h>
typedef struct { uint32_t n, min, max; uint64_t sum; } vg_span_t;
typedef struct {
  uint32_t period, deadline, next, release, previous;
  uint32_t skipped, late, lateness_max, jitter_n;
  int32_t jitter_min, jitter_max;
  int64_t jitter_sum;
  uint32_t jitter_bins[4];
  bool has_previous;
  vg_span_t execution;
} vg_cadence_t;
void vg_span_add(vg_span_t *s, uint32_t us);
void vg_cadence_init(vg_cadence_t *t, uint32_t now, uint32_t period, uint32_t deadline);
bool vg_cadence_due(vg_cadence_t *t, uint32_t now);
void vg_cadence_finish(vg_cadence_t *t, uint32_t start, uint32_t end);
#endif

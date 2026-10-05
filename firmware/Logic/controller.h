#ifndef VG_CONTROLLER_H
#define VG_CONTROLLER_H
#include "impact.h"
typedef enum { GUARD_MODE, SERVICE_MODE } guard_mode_t;
typedef struct {
  event_log_t journal;
  impact_detector_t impact;
  guard_mode_t mode;
  bool pir_high, mpu_fault, adc_fault;
  bool light_ready, light_open, light_candidate, dark_candidate;
  uint16_t light_samples;
  uint32_t light_sum, candidate_since, dark_since;
  int32_t filtered_q4, light_background;
} controller_t;
void controller_init(controller_t *controller, void (*sink)(void *, const event_t *), void *context);
void controller_mode(controller_t *controller, uint32_t now, guard_mode_t mode);
void controller_pir(controller_t *controller, uint32_t now, bool high);
void controller_acceleration(controller_t *controller, uint32_t now, bool valid, const int32_t mg[3], int32_t failure);
void controller_light(controller_t *controller, uint32_t now, bool valid, uint16_t counts);
#endif

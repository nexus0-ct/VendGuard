#include "controller.h"
#include <string.h>
static void access_event(controller_t *c, uint32_t now)
{
  log_event(&c->journal, now, c->mode == GUARD_MODE ? SERVICE_OPEN_SUSPECTED : SERVICE_ACCESS,
            VG_LDR, c->light_background - c->filtered_q4 / 4);
}
void controller_init(controller_t *c, void (*sink)(void *, const event_t *), void *context)
{
  memset(c, 0, sizeof(*c));
  log_init(&c->journal, sink, context);
  c->mode = GUARD_MODE;
}
void controller_mode(controller_t *c, uint32_t now, guard_mode_t mode)
{
  if (mode == c->mode) return;
  c->mode = mode;
  log_event(&c->journal, now, MODE_CHANGED, VG_OPERATOR, mode);
  if (c->light_open && !c->adc_fault) access_event(c, now);
}
void controller_pir(controller_t *c, uint32_t now, bool high)
{
  if (high && !c->pir_high) log_event(&c->journal, now, CUSTOMER_DETECTED, VG_PIR, 1);
  c->pir_high = high;
}
void controller_acceleration(controller_t *c, uint32_t now, bool valid, const int32_t mg[3], int32_t failure)
{
  if (!valid) {
    if (!c->mpu_fault) log_event(&c->journal, now, MPU_FAILED, VG_MPU, failure);
    c->mpu_fault = true;
    impact_reset(&c->impact);
    return;
  }
  if (c->mpu_fault) {
    c->mpu_fault = false;
    log_event(&c->journal, now, MPU_RECOVERED, VG_MPU, 0);
  }
  impact_sample(&c->impact, &c->journal, now, mg);
}
void controller_light(controller_t *c, uint32_t now, bool valid, uint16_t counts)
{
  if (!valid) {
    if (!c->adc_fault) log_event(&c->journal, now, ADC_FAILED, VG_LDR, 0);
    c->adc_fault = true;
    c->light_candidate = c->dark_candidate = false;
    if (!c->light_ready) { c->light_samples = 0; c->light_sum = 0; }
    return;
  }
  if (c->adc_fault) { c->adc_fault = false; log_event(&c->journal, now, ADC_RECOVERED, VG_LDR, counts); }
  if (!c->light_ready) {
    c->light_sum += counts;
    if (++c->light_samples == 50) {
      c->light_background = (int32_t)(c->light_sum / 50);
      c->filtered_q4 = c->light_background * 4;
      c->light_ready = true;
      log_event(&c->journal, now, LIGHT_READY, VG_LDR, c->light_background);
    }
    return;
  }
  /* Quarter-count units preserve precision during the low-pass update. */
  c->filtered_q4 += ((int32_t)counts * 4 - c->filtered_q4) / 4;
  int32_t delta = c->light_background - c->filtered_q4 / 4;
  if (!c->light_open) {
    if (delta >= 600 && c->light_background - (int32_t)counts >= 600) {
      if (!c->light_candidate) { c->light_candidate = true; c->candidate_since = now; }
      if ((uint32_t)(now - c->candidate_since) >= 400U) {
        c->light_open = true; c->light_candidate = false; access_event(c, now);
      }
    } else c->light_candidate = false;
  } else {
    if (delta <= 300 && c->light_background - (int32_t)counts <= 300) {
      if (!c->dark_candidate) { c->dark_candidate = true; c->dark_since = now; }
      if ((uint32_t)(now - c->dark_since) >= 400U) { c->light_open = false; c->dark_candidate = false; }
    } else c->dark_candidate = false;
  }
}

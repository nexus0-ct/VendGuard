#ifndef VG_EVENT_LOG_H
#define VG_EVENT_LOG_H
#include <stdint.h>
#include <stddef.h>
#define VG_LOG_CAPACITY 64U
typedef enum { VG_INFO, VG_ALARM, VG_FAULT } event_category_t;
typedef enum { VG_SYSTEM, VG_PIR, VG_MPU, VG_LDR, VG_OPERATOR } event_source_t;
typedef enum { CUSTOMER_DETECTED, IMPACT_ALARM, SERVICE_OPEN_SUSPECTED, SERVICE_ACCESS,
               MODE_CHANGED, MPU_FAILED, MPU_RECOVERED, BACKGROUND_READY, LIGHT_READY, ADC_FAILED, ADC_RECOVERED } event_kind_t;
typedef struct {
  uint32_t sequence, ms;
  event_kind_t kind;
  event_category_t category;
  event_source_t source;
  int32_t value;
} event_t;
typedef struct {
  event_t records[VG_LOG_CAPACITY];
  uint32_t next_sequence, alarm_count, overwritten;
  size_t count, head;
  void (*sink)(void *, const event_t *);
  void *context;
} event_log_t;
void log_init(event_log_t *log, void (*sink)(void *, const event_t *), void *context);
void log_event(event_log_t *log, uint32_t ms, event_kind_t kind, event_source_t source, int32_t value);
const event_t *log_at(const event_log_t *log, size_t chronological_index);
const char *event_name(event_kind_t kind);
const char *category_name(event_category_t category);
const char *source_name(event_source_t source);
#endif

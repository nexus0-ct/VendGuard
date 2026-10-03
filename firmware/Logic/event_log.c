#include "event_log.h"
#include <string.h>
void log_init(event_log_t *log, void (*sink)(void *, const event_t *), void *context)
{
  memset(log, 0, sizeof(*log));
  log->sink = sink;
  log->context = context;
}
void log_event(event_log_t *log, uint32_t ms, event_kind_t kind, event_source_t source, int32_t value)
{
  event_category_t category = VG_INFO;
  if (kind == IMPACT_ALARM || kind == SERVICE_OPEN_SUSPECTED) category = VG_ALARM;
  if (kind == MPU_FAILED || kind == ADC_FAILED) category = VG_FAULT;
  event_t record = {++log->next_sequence, ms, kind, category, source, value};
  log->records[log->head] = record;
  log->head = (log->head + 1U) % VG_LOG_CAPACITY;
  if (log->count < VG_LOG_CAPACITY) ++log->count;
  else ++log->overwritten;
  if (category == VG_ALARM) ++log->alarm_count;
  if (log->sink) log->sink(log->context, &record);
}
const event_t *log_at(const event_log_t *log, size_t index)
{
  if (index >= log->count) return NULL;
  size_t oldest = (log->head + VG_LOG_CAPACITY - log->count) % VG_LOG_CAPACITY;
  return &log->records[(oldest + index) % VG_LOG_CAPACITY];
}
const char *event_name(event_kind_t kind)
{
  static const char *const names[] = {"CUSTOMER_DETECTED", "IMPACT_ALARM", "SERVICE_OPEN_SUSPECTED", "SERVICE_ACCESS",
    "MODE_CHANGED", "MPU_FAILED", "MPU_RECOVERED", "BACKGROUND_READY", "LIGHT_READY", "ADC_FAILED", "ADC_RECOVERED"};
  return names[kind];
}
const char *category_name(event_category_t category)
{
  static const char *const names[] = {"INFO", "ALARM", "FAULT"};
  return names[category];
}
const char *source_name(event_source_t source)
{
  static const char *const names[] = {"SYSTEM", "PIR", "MPU6050", "LDR_ADC", "OPERATOR"};
  return names[source];
}

#include "board.h"
#include "main.h"
#include "controller.h"
#include "mpu6050.h"
#include <stdio.h>
#include <string.h>
static controller_t controller;
static mpu6050_t mpu;
static acceleration_t acceleration;
static mpu_result_t imu_status;
static uint16_t adc_counts;
static bool mpu_ready, adc_valid;
static uint32_t heartbeat, pir_time, imu_time, light_time, data_time, retry_time;
static char command[48];
static size_t command_length;
static bool command_overflow;
static void print_event(const char *tag, const event_t *e)
{
  printf("VG %s seq=%lu ms=%lu category=%s source=%s event=%s value=%ld\r\n",
    tag, (unsigned long)e->sequence, (unsigned long)e->ms, category_name(e->category),
    source_name(e->source), event_name(e->kind), (long)e->value);
}
static void event_sink(void *context, const event_t *e) { (void)context; print_event("EVENT", e); }
static void status(uint32_t now)
{
  printf("VG STATUS ms=%lu mode=%s pir=%u adc=%u adc_ok=%u imu=%s ax=%ld ay=%ld az=%ld calibrated=%u alarms=%lu records=%u overwritten=%lu\r\n",
    (unsigned long)now, controller.mode == GUARD_MODE ? "guard" : "service", controller.pir_high,
    adc_counts, adc_valid, mpu_result_name(imu_status), (long)acceleration.mg[0],
    (long)acceleration.mg[1], (long)acceleration.mg[2], controller.impact.ready,
    (unsigned long)controller.journal.alarm_count, (unsigned)controller.journal.count,
    (unsigned long)controller.journal.overwritten);
}
static void identify(uint32_t now)
{
  uint8_t id = 0;
  imu_status = mpu_begin(&mpu, &id);
  mpu_ready = imu_status == MPU_OK;
  retry_time = now;
  printf("VG CHECK device=MPU6050 addr=0x%02x id=0x%02x result=%s\r\n", mpu.address, id, mpu_result_name(imu_status));
  if (!mpu_ready) controller_acceleration(&controller, now, false, NULL, imu_status);
}
static void execute(uint32_t now)
{
  if (!strcmp(command, "guard")) controller_mode(&controller, now, GUARD_MODE);
  else if (!strcmp(command, "service")) controller_mode(&controller, now, SERVICE_MODE);
  else if (!strcmp(command, "status")) status(now);
  else if (!strcmp(command, "probe")) {
    uint8_t id = 0, bytes[1];
    register_bus_t bus = board_bus();
    bus_result_t read = bus.read(bus.context, mpu.address, 0x75, bytes, 1);
    if (read == BUS_OK) id = bytes[0];
    acceleration_t frame = {0};
    mpu_result_t result = mpu_read(&mpu, &frame);
    printf("VG PROBE addr=0x%02x id=0x%02x result=%s raw=", mpu.address, id, mpu_result_name(result));
    for (size_t i = 0; i < 6; ++i) printf("%02x", frame.raw[i]);
    printf(" ax=%ld ay=%ld az=%ld\r\n", (long)frame.mg[0], (long)frame.mg[1], (long)frame.mg[2]);
  } else if (!strcmp(command, "mpu 69") || !strcmp(command, "mpu 68")) {
    mpu.address = command[5] == '9' ? 0x69 : 0x68;
    mpu_ready = false;
    identify(now);
  } else if (!strcmp(command, "calibrate")) {
    impact_reset(&controller.impact);
    controller.light_ready = controller.light_open = false;
    controller.light_candidate = controller.dark_candidate = false;
    controller.light_samples = 0;
    controller.light_sum = 0;
    printf("VG COMMAND op=calibrate result=OK\r\n");
  } else if (!strcmp(command, "journal")) {
    for (size_t i = 0; i < controller.journal.count; ++i) print_event("RECORD", log_at(&controller.journal, i));
  } else printf("VG COMMAND result=UNKNOWN\r\n");
}
static void console(uint32_t now)
{
  char ch;
  unsigned budget = 8;
  while (budget-- && board_console_byte(&ch)) {
    if (ch == '\r' || ch == '\n') {
      if (command_overflow) printf("VG COMMAND result=TOO_LONG\r\n");
      else if (command_length) { command[command_length] = '\0'; execute(now); }
      command_length = 0;
      command_overflow = false;
    } else if (ch == '\b' && command_length) --command_length;
    else if (ch >= ' ' && ch <= '~') {
      if (command_length + 1 < sizeof(command)) command[command_length++] = ch;
      else command_overflow = true;
    }
  }
}
void vendguard_start(void)
{
  controller_init(&controller, event_sink, NULL);
  board_console_start();
  mpu.bus = board_bus();
  mpu.address = 0x68;
  HAL_Delay(100);
  uint32_t now = HAL_GetTick();
  printf("VG BOOT version=2 mode=guard\r\n");
  identify(now);
  adc_valid = board_light(&adc_counts);
  printf("VG CHECK device=LDR_ADC result=%s counts=%u\r\n", adc_valid ? "READABLE" : "ERROR", adc_counts);
  printf("VG CHECK device=PIR result=READABLE level=%u\r\n", board_customer());
  printf("VG READY imu_hz=100 light_hz=50 pir_hz=200 log=64 commands=guard,service,status,probe,mpu68,mpu69,calibrate,journal\r\n");
  heartbeat = pir_time = imu_time = light_time = data_time = now;
}
void vendguard_poll(void)
{
  uint32_t now = HAL_GetTick();
  console(now);
  if ((uint32_t)(now - pir_time) >= 5U) {
    pir_time = now;
    controller_pir(&controller, now, board_customer());
  }
  if (!mpu_ready && (uint32_t)(now - retry_time) >= 1000U) identify(now);
  if ((uint32_t)(now - imu_time) >= 10U) {
    imu_time = now;
    if (mpu_ready) {
      imu_status = mpu_read(&mpu, &acceleration);
      if (imu_status != MPU_OK) { mpu_ready = false; retry_time = now; }
    }
    controller_acceleration(&controller, now, mpu_ready, acceleration.mg, imu_status);
  }
  if ((uint32_t)(now - light_time) >= 20U) {
    light_time = now;
    adc_valid = board_light(&adc_counts);
    controller_light(&controller, now, adc_valid, adc_counts);
  }
  if ((uint32_t)(now - data_time) >= 500U) { data_time = now; status(now); }
  if ((uint32_t)(now - heartbeat) >= 1000U) {
    heartbeat = now;
    board_heartbeat();
    printf("VG HEARTBEAT ms=%lu\r\n", (unsigned long)now);
  }
}

#include "board.h"
#include "controller.h"
#include "mpu6050.h"
#include "instrument.h"
#include "cadence.h"
#include <stdio.h>
#include <string.h>
static controller_t controller;
static mpu6050_t mpu;
static acceleration_t acceleration;
static mpu_result_t imu_status;
static uint16_t adc_counts;
static bool mpu_ready, adc_valid;
enum { INPUT, CUSTOMER, MECHANICAL, LIGHT, RECOVERY, AUDIT, DATA, HEART, TASK_COUNT };
static const char *const task_names[] = {"console","customer","mechanical","light","recovery","audit","telemetry","heartbeat"};
static const uint32_t periods[] = {1000,5000,10000,20000,1000000,1000,500000,1000000};
static vg_cadence_t tasks[TASK_COUNT];
static vg_span_t loop_execution, loop_interval, pir_latency, command_latency;
static vg_span_t clock_cost;
static vg_span_t command_complete_latency;
static uint32_t measured_since, previous_loop;
static uint32_t measure_generation;
static uint64_t occupied;
static bool measuring, telemetry;
static bool first_mechanical;
static vg_audit_t audit;
static event_t pending[8];
static unsigned queued, qread, qwrite;
static uint32_t audit_dropped, audit_failed;
#ifndef VG_BUILD_ID
#define VG_BUILD_ID "unversioned"
#endif
static char command[48];
static size_t command_length;
static bool command_overflow;
static void print_event(const char *tag, const event_t *e)
{
  printf("VG %s seq=%lu ms=%lu category=%s source=%s event=%s value=%ld\r\n",
    tag, (unsigned long)e->sequence, (unsigned long)e->ms, category_name(e->category),
    source_name(e->source), event_name(e->kind), (long)e->value);
}
static void event_sink(void *context, const event_t *e)
{
  (void)context;
  if (queued == 8) ++audit_dropped;
  else { pending[qwrite] = *e; qwrite = (qwrite+1U)%8U; ++queued; }
  print_event("EVENT",e);
}
static void measure_start(void)
{
  ++measure_generation;
  uint32_t now = vg_now();
  for (unsigned i = 0; i < TASK_COUNT; ++i) vg_cadence_init(&tasks[i],now,periods[i],periods[i]);
  memset(&loop_execution,0,sizeof(loop_execution)); memset(&loop_interval,0,sizeof(loop_interval));
  memset(&pir_latency,0,sizeof(pir_latency)); memset(&command_latency,0,sizeof(command_latency));
  memset(&command_complete_latency,0,sizeof(command_complete_latency));
  occupied = 0; previous_loop = 0; measured_since = now; measuring = true;
}
static void span_print(const char *name, const vg_span_t *s)
{
  printf("STAT name=%s n=%lu min_us=%lu avg_us=%lu max_us=%lu\r\n",name,
    (unsigned long)s->n,(unsigned long)s->min,(unsigned long)(s->n ? s->sum/s->n : 0),(unsigned long)s->max);
}
static void memory_report(void)
{
  unsigned stack = vg_stack_used(); vg_heap_t heap = vg_heap_read();
  printf("MEM stack_used=%u stack_reserve=2048 heap_arena=%u heap_used=%u heap_free=%u\r\n",
    stack,heap.arena,heap.used,heap.free);
}
static void stats(void)
{
  uint32_t elapsed = vg_now()-measured_since;
  measuring = false;
  printf("VG METRICS build=%s elapsed_us=%lu task_load_permille=%lu busy_spin=1 clock=TIM3_1MHz\r\n",
    VG_BUILD_ID,(unsigned long)elapsed,(unsigned long)(elapsed ? occupied*1000U/elapsed : 0));
  for (unsigned i = 0; i < TASK_COUNT; ++i) {
    span_print(task_names[i],&tasks[i].execution);
    printf("SCHED name=%s skipped=%lu deadline_miss=%lu jitter_min_us=%ld jitter_max_us=%ld release_late_max_us=%lu\r\n",
      task_names[i],(unsigned long)tasks[i].skipped,(unsigned long)tasks[i].late,
      (long)tasks[i].jitter_min,(long)tasks[i].jitter_max,(unsigned long)tasks[i].lateness_max);
    printf("JITTER name=%s n=%lu avg_us=%ld abs_le10=%lu abs_le100=%lu abs_le1000=%lu abs_gt1000=%lu\r\n",
      task_names[i],(unsigned long)tasks[i].jitter_n,
      (long)(tasks[i].jitter_n ? tasks[i].jitter_sum/tasks[i].jitter_n : 0),
      (unsigned long)tasks[i].jitter_bins[0],(unsigned long)tasks[i].jitter_bins[1],
      (unsigned long)tasks[i].jitter_bins[2],(unsigned long)tasks[i].jitter_bins[3]);
  }
  span_print("loop",&loop_execution); span_print("loop_interval",&loop_interval);
  span_print("pir_latency",&pir_latency); span_print("command_latency",&command_latency);
  span_print("command_complete_latency",&command_complete_latency);
  span_print("clock_read",&clock_cost);
  uint32_t lost, overrun, edges; board_rx_stats(&lost,&overrun,&edges);
  uint32_t tx_timeout, tx_error; board_tx_stats(&tx_timeout,&tx_error);
  printf("COUNTERS rx_dropped=%lu rx_overrun=%lu pir_dropped=%lu audit_dropped=%lu audit_failed=%lu audit_queued=%u tx_timeouts=%lu tx_errors=%lu\r\n",
    (unsigned long)lost,(unsigned long)overrun,(unsigned long)edges,(unsigned long)audit_dropped,(unsigned long)audit_failed,queued,
    (unsigned long)tx_timeout,(unsigned long)tx_error);
  memory_report();
}
static void durable_dump(void)
{
  uint8_t b[VG_AUDIT_RECORD]; vg_nvm_t io = vg_internal_flash();
  for (unsigned off = 0; off < 4096; off += sizeof(b)) {
    event_t e; uint32_t seq, boot;
    if (io.read(io.context,off,b,sizeof(b)) && vg_audit_decode(b,&e,&seq,&boot)) {
      printf("VG DURABLE serial=%lu boot=%lu ",(unsigned long)seq,(unsigned long)boot);
      print_event("PERSISTED",&e);
    }
  }
}
static void status(uint32_t now)
{
  printf("VG STATUS ms=%lu mode=%s pir=%u adc=%u adc_ok=%u imu=%s ax=%ld ay=%ld az=%ld calibrated=%u light_ready=%u alarms=%lu records=%u overwritten=%lu\r\n",
    (unsigned long)now, controller.mode == GUARD_MODE ? "guard" : "service", board_customer(),
    adc_counts, adc_valid, mpu_result_name(imu_status), (long)acceleration.mg[0],
    (long)acceleration.mg[1], (long)acceleration.mg[2], controller.impact.ready, controller.light_ready,
    (unsigned long)controller.journal.alarm_count, (unsigned)controller.journal.count,
    (unsigned long)controller.journal.overwritten);
}
static void identify(uint32_t now)
{
  uint8_t id = 0;
  imu_status = mpu_begin(&mpu, &id);
  mpu_ready = imu_status == MPU_OK;
  printf("VG CHECK device=MPU6050 addr=0x%02x id=0x%02x result=%s\r\n", mpu.address, id, mpu_result_name(imu_status));
  if (!mpu_ready) controller_acceleration(&controller, now, false, NULL, imu_status);
}
static void execute(uint32_t now)
{
  bool known = true;
  if (!strcmp(command,"version")) printf("VG VERSION build=%s\r\n",VG_BUILD_ID);
  else if (!strcmp(command,"stats")) stats();
  else if (!strcmp(command,"measure on")) { printf("VG MEASURE start\r\n"); measure_start(); }
  else if (!strcmp(command,"measure off")) stats();
  else if (!strcmp(command,"memory")) memory_report();
  else if (!strcmp(command,"telemetry 1")) telemetry = true;
  else if (!strcmp(command,"telemetry 0")) telemetry = false;
  else if (!strcmp(command,"durable")) durable_dump();
  else if (!strcmp(command,"store pause")) audit.available = false;
  else if (!strcmp(command,"store retry")) {
    uint32_t boot = audit.boot;
    vg_audit_open(&audit,vg_internal_flash());
    if (boot) audit.boot = boot;
    printf("VG STORE available=%u valid=%lu damaged=%lu serial=%lu boot=%lu\r\n",audit.available,
      (unsigned long)audit.valid,(unsigned long)audit.damaged,(unsigned long)audit.serial,(unsigned long)audit.boot);
  }
  else if (!strcmp(command,"verify")) {
    vg_audit_t scan; bool ok = vg_audit_open(&scan,vg_internal_flash());
    printf("VG VERIFY readable=%u valid=%lu damaged=%lu latest=%lu\r\n",ok,
      (unsigned long)scan.valid,(unsigned long)scan.damaged,(unsigned long)scan.serial);
  }
  else if (!strcmp(command, "guard")) controller_mode(&controller, now, GUARD_MODE);
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
  } else { known = false; printf("VG COMMAND result=UNKNOWN\r\n"); }
  if (known && strcmp(command,"measure on") && strcmp(command,"measure off") && strcmp(command,"stats"))
    printf("VG DONE op=%s t_us=%lu\r\n",command,(unsigned long)vg_now());
}
static bool rx_gap(void)
{
  static uint32_t previously_lost, previous_overrun;
  uint32_t lost, overruns, edges; board_rx_stats(&lost,&overruns,&edges);
  if (lost == previously_lost && overruns == previous_overrun) return false;
  previously_lost = lost; previous_overrun = overruns;
  board_console_discard();
  command_overflow = true; command_length = 0;
  return true;
}
static bool console(uint32_t now)
{
  char ch; uint32_t arrived;
  bool consumed = false;
  (void)rx_gap();
  unsigned budget = 8;
  while (budget-- && board_console_timed(&ch,&arrived)) {
    consumed = true;
    if (rx_gap()) continue;
    if (ch == '\r' || ch == '\n') {
      if (command_overflow) printf("VG COMMAND result=DISCARDED reason=overflow_or_rx_gap\r\n");
      else if (command_length) {
        command[command_length] = '\0';
        bool count_command = measuring && strcmp(command,"measure on") && strcmp(command,"measure off") && strcmp(command,"stats");
        if (count_command) vg_span_add(&command_latency,vg_now()-arrived);
        execute(now);
        if (count_command && measuring) vg_span_add(&command_complete_latency,vg_now()-arrived);
      }
      command_length = 0;
      command_overflow = false;
    } else if (ch == '\b' && command_length) --command_length;
    else if (ch >= ' ' && ch <= '~') {
      if (command_length + 1 < sizeof(command)) command[command_length++] = ch;
      else command_overflow = true;
    }
  }
  return consumed;
}
void vendguard_start(void)
{
  controller_init(&controller, event_sink, NULL);
  board_console_start();
  mpu.bus = board_bus();
  mpu.address = 0x68;
  uint32_t now = vg_millis();
  vg_heap_t before_output = vg_heap_read();
  printf("VG BOOT version=3 build=%s mode=guard timer=TIM3_1MHz\r\n",VG_BUILD_ID);
  printf("MEM_STAGE stage=before_uart heap_arena=%u heap_used=%u\r\n",
    before_output.arena,before_output.used);
  for (unsigned i = 0; i < 256; ++i) {
    uint32_t t = vg_now(); vg_span_add(&clock_cost,vg_now()-t);
  }
  vg_audit_open(&audit,vg_internal_flash());
  printf("VG STORE available=%u valid=%lu damaged=%lu serial=%lu boot=%lu\r\n",audit.available,
    (unsigned long)audit.valid,(unsigned long)audit.damaged,(unsigned long)audit.serial,(unsigned long)audit.boot);
  identify(now);
  adc_valid = board_light(&adc_counts);
  printf("VG CHECK device=LDR_ADC result=%s counts=%u t_us=%lu\r\n", adc_valid ? "READABLE" : "ERROR", adc_counts,(unsigned long)vg_now());
  printf("VG CHECK device=PIR result=READABLE level=%u t_us=%lu\r\n", board_customer(),(unsigned long)vg_now());
  printf("VG READY imu_hz=100 light_hz=50 pir_hz=200 log=64 commands=guard,service,status,probe,mpu68,mpu69,calibrate,journal\r\n");
  memory_report();
  measure_start();
}
void vendguard_poll(void)
{
  uint32_t loop_start = vg_now(); bool account = measuring;
  uint32_t generation = measure_generation;
  if (account && previous_loop) vg_span_add(&loop_interval,loop_start-previous_loop);
  previous_loop = loop_start;
  for (unsigned id = 0; id < TASK_COUNT; ++id) {
    uint32_t start = vg_now();
    if (!vg_cadence_due(&tasks[id],start)) continue;
    uint32_t now = vg_millis(); bool worked = true;
    if (id == INPUT) worked = console(now);
    else if (id == CUSTOMER) {
      uint32_t edge;
      if (board_customer_event(&edge)) {
        vg_probe(8,true);
        if (measuring) vg_span_add(&pir_latency,vg_now()-edge);
        controller_pir(&controller,now,false); controller_pir(&controller,now,true);
        vg_probe(8,false);
      } else worked = false;
    } else if (id == MECHANICAL) {
      vg_probe(6,true);
    if (mpu_ready) {
      imu_status = mpu_read(&mpu, &acceleration);
      if (imu_status != MPU_OK) mpu_ready = false;
      else if (!first_mechanical) {
        first_mechanical = true;
        printf("VG FIRST device=MPU6050 t_us=%lu\r\n",(unsigned long)vg_now());
      }
    }
    controller_acceleration(&controller, now, mpu_ready, acceleration.mg, imu_status);
      vg_probe(6,false);
    } else if (id == LIGHT) {
    adc_valid = board_light(&adc_counts);
    controller_light(&controller, now, adc_valid, adc_counts);
    } else if (id == RECOVERY) {
      if (!mpu_ready) identify(now);
      else worked = false;
    } else if (id == AUDIT) {
      if (queued && audit.available) {
        vg_probe(7,true);
        bool ok = vg_audit_append(&audit,&pending[qread]);
        vg_probe(7,false);
        if (ok) { qread = (qread+1U)%8U; --queued; }
        else { ++audit_failed; printf("VG STORE result=FAILED readback_or_hal=1\r\n"); }
      } else worked = false;
    } else if (id == DATA) { if (telemetry) status(now); else worked = false; }
    else if (id == HEART) board_heartbeat();
    uint32_t end = vg_now();
    if (measuring && account && generation == measure_generation) {
      vg_cadence_finish(&tasks[id],start,end);
      if (worked) occupied += end-start;
    }
  }
  if (measuring && account && generation == measure_generation) vg_span_add(&loop_execution,vg_now()-loop_start);
}

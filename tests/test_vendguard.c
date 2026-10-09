#include "controller.h"
#include "mpu6050.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
typedef struct {
  uint8_t registers[256];
  bus_result_t failure;
  unsigned operations, fail_at, writes;
  uint8_t address, written_registers[8], written_values[8];
} fake_bus_t;
static bus_result_t fake_read(void *context, uint8_t address, uint8_t reg, uint8_t *bytes, size_t length)
{
  fake_bus_t *f = context;
  assert(address == f->address);
  if (++f->operations == f->fail_at) return f->failure;
  memcpy(bytes, &f->registers[reg], length);
  return BUS_OK;
}
static bus_result_t fake_write(void *context, uint8_t address, uint8_t reg, const uint8_t *bytes, size_t length)
{
  fake_bus_t *f = context;
  assert(address == f->address && length == 1);
  if (++f->operations == f->fail_at) return f->failure;
  f->written_registers[f->writes] = reg;
  f->written_values[f->writes++] = bytes[0];
  memcpy(&f->registers[reg], bytes, length);
  return BUS_OK;
}
static mpu6050_t device(fake_bus_t *f)
{
  memset(f, 0, sizeof(*f)); f->address = 0x68; f->registers[0x75] = 0x68;
  return (mpu6050_t){{f, fake_read, fake_write}, 0x68};
}
static unsigned events(const controller_t *c, event_kind_t kind)
{
  unsigned count = 0;
  for (size_t i = 0; i < c->journal.count; ++i) if (log_at(&c->journal, i)->kind == kind) ++count;
  return count;
}
static const int32_t rest[3] = {0,0,1000};
static void initialize(controller_t *c)
{
  controller_init(c, NULL, NULL);
  for (unsigned i=0;i<100;++i) controller_acceleration(c,i*10,true,rest,0);
  for (unsigned i=0;i<50;++i) controller_light(c,i*20,true,3900);
  assert(c->impact.ready && c->light_ready && !c->journal.alarm_count);
}
static void light_for(controller_t *c, uint32_t now, uint16_t level, unsigned samples)
{
  for (unsigned i=0;i<samples;++i) controller_light(c,now+i*20,true,level);
}
static void test_driver(void)
{
  fake_bus_t f; mpu6050_t d=device(&f); uint8_t id=0;
  assert(mpu_begin(&d,&id)==MPU_OK && id==0x68 && f.writes==5);
  const uint8_t regs[]={0x6b,0x1a,0x19,0x1c,0x6c}, vals[]={1,3,9,0,7};
  assert(!memcmp(regs,f.written_registers,5) && !memcmp(vals,f.written_values,5));
  d=device(&f);f.registers[0x75]=0x42; assert(mpu_begin(&d,&id)==MPU_BAD_ID && f.writes==0);
  for (unsigned fail=1;fail<=6;++fail) {
    for (unsigned result=BUS_NACK;result<=BUS_ERROR;++result) {
      d=device(&f);f.fail_at=fail;f.failure=(bus_result_t)result;
      mpu_result_t expected=result==BUS_NACK?MPU_NACK:result==BUS_TIMEOUT?MPU_TIMEOUT:MPU_IO_ERROR;
      assert(mpu_begin(&d,&id)==expected && f.operations==fail);
    }
  }
  for (unsigned i=0;i<=MPU_BAD_ID;++i) assert(strlen(mpu_result_name((mpu_result_t)i)));
}
static void test_raw_units(void)
{
  fake_bus_t f;mpu6050_t d=device(&f);acceleration_t sample={{0},{0}};
  /* Six bytes from the recorded Wokwi burst; provenance in fixtures/captured-rest.md. */
  const uint8_t captured[]={0,0,0,0,0x40,0};
  memcpy(&f.registers[0x3b],captured,6);assert(mpu_read(&d,&sample)==MPU_OK);
  assert(sample.mg[0]==0 && sample.mg[1]==0 && sample.mg[2]==1000 && !memcmp(sample.raw,captured,6));
  const uint8_t signed_extremes[]={0x80,0,0x7f,0xff,0xff,0xff};
  memcpy(&f.registers[0x3b],signed_extremes,6);assert(mpu_read(&d,&sample)==MPU_OK);
  assert(sample.mg[0]==-2000 && sample.mg[1]==1999 && sample.mg[2]==0);
  acceleration_t saved=sample;
  for (unsigned r=BUS_NACK;r<=BUS_ERROR;++r) {
    f.fail_at=f.operations+1;f.failure=(bus_result_t)r;
    assert(mpu_read(&d,&sample)!=MPU_OK && !memcmp(&sample,&saved,sizeof(sample)));
  }
}
static void test_calibration(void)
{
  controller_t c;controller_init(&c,NULL,NULL);
  const int32_t zero[]={0,0,0}, excessive[]={0,0,1101}, low[]={0,0,899};
  controller_acceleration(&c,0,true,zero,0);controller_acceleration(&c,1,true,excessive,0);
  controller_acceleration(&c,2,true,low,0);assert(!c.impact.samples);
  for (unsigned i=0;i<99;++i) controller_acceleration(&c,i*10,true,rest,0);
  assert(!c.impact.ready);
  int32_t moving[]={101,0,1000};controller_acceleration(&c,990,true,moving,0);assert(!c.impact.samples);
  for (unsigned i=0;i<100;++i) {
    int32_t sample[]={i%2?50:-50,0,1000};controller_acceleration(&c,1000+i*10,true,sample,0);
  }
  assert(c.impact.ready && c.impact.background[0]==0 && events(&c,BACKGROUND_READY)==1 && !c.journal.alarm_count);
}
static void test_rest_noise(void)
{
  controller_t c;initialize(&c);
  for(unsigned i=0;i<500;++i) { int32_t noise[]={i%2?30:-30,20,1000};controller_acceleration(&c,2000+i*10,true,noise,0); }
  assert(!c.journal.alarm_count);
  for(unsigned i=0;i<800;++i) {int32_t drift[]={(int32_t)i,0,1000};controller_acceleration(&c,8000+i*10,true,drift,0);}
  assert(!c.journal.alarm_count);
}
static void test_impact_boundaries(void)
{
  controller_t c;initialize(&c);
  int32_t near[]={649,0,1000}, exact[]={650,0,1000};
  controller_acceleration(&c,2000,true,near,0);assert(!c.journal.alarm_count);
  controller_acceleration(&c,2010,true,rest,0);controller_acceleration(&c,2020,true,exact,0);
  assert(events(&c,IMPACT_ALARM)==1);
  for(unsigned i=0;i<100;++i) controller_acceleration(&c,2030+i*10,true,exact,0);
  assert(events(&c,IMPACT_ALARM)==1);
  const int32_t quiet_edge[]={150,0,1000}, outside[]={151,0,1000};
  controller_acceleration(&c,4000,true,quiet_edge,0);controller_acceleration(&c,4299,true,quiet_edge,0);assert(c.impact.latched);
  controller_acceleration(&c,4300,true,outside,0);assert(!c.impact.quiet);
  controller_acceleration(&c,4400,true,rest,0);controller_acceleration(&c,4700,true,rest,0);assert(!c.impact.latched);
  controller_acceleration(&c,4710,true,exact,0);assert(events(&c,IMPACT_ALARM)==2);
  controller_init(&c,NULL,NULL);for(unsigned i=0;i<100;++i){int32_t oriented[]={0,1000,0};controller_acceleration(&c,i*10,true,oriented,0);}
  int32_t strike[]={0,1000,-650};controller_acceleration(&c,2000,true,strike,0);assert(c.journal.alarm_count==1);
}
static void test_step_threshold(void)
{
  controller_t c;initialize(&c);int32_t slow[]={451,0,1000},below[]={700,0,1000},above[]={950,0,1000};
  controller_acceleration(&c,2000,true,slow,0);controller_acceleration(&c,2010,true,below,0);assert(!c.journal.alarm_count);
  controller_acceleration(&c,2020,true,above,0);assert(c.journal.alarm_count==1);
}
static void test_pir(void)
{
  controller_t c;initialize(&c);
  for(unsigned i=0;i<1000;++i)controller_pir(&c,2000+i*5,true);
  assert(events(&c,CUSTOMER_DETECTED)==1 && c.journal.alarm_count==0);
  controller_pir(&c,8000,false);controller_pir(&c,8100,true);
  assert(events(&c,CUSTOMER_DETECTED)==2 && !c.journal.alarm_count);
}
static void test_light_guard(void)
{
  controller_t c;initialize(&c);
  light_for(&c,2000,3900,100);assert(!c.journal.alarm_count);
  for(unsigned i=0;i<100;++i)controller_light(&c,4000+i*20,true,i%2?3280:3320);
  assert(!c.journal.alarm_count);
  light_for(&c,6000,1000,15);assert(!c.journal.alarm_count);
  light_for(&c,6300,3900,40);assert(!c.journal.alarm_count);
  light_for(&c,7200,1000,60);assert(events(&c,SERVICE_OPEN_SUSPECTED)==1);
  light_for(&c,8500,1000,100);assert(events(&c,SERVICE_OPEN_SUSPECTED)==1);
  light_for(&c,11000,3900,60);assert(!c.light_open);
  light_for(&c,13000,1000,60);assert(events(&c,SERVICE_OPEN_SUSPECTED)==2);
}
static void test_light_exact_timing(void)
{
  controller_t c;initialize(&c);c.filtered_q4=3300*4;
  controller_light(&c,2000,true,3300);controller_light(&c,2399,true,3300);assert(!c.light_open);
  controller_light(&c,2400,true,3300);assert(c.light_open);
  c.filtered_q4=3600*4;
  controller_light(&c,2500,true,3600);controller_light(&c,2899,true,3600);assert(c.light_open);
  controller_light(&c,2900,true,3600);assert(!c.light_open);
  c.filtered_q4=3299*4;controller_light(&c,3000,true,3299);c.filtered_q4=3301*4;
  controller_light(&c,3200,true,3301);assert(!c.light_candidate);
}
static void test_modes(void)
{
  controller_t c;initialize(&c);controller_mode(&c,2000,SERVICE_MODE);
  controller_mode(&c,2001,SERVICE_MODE);assert(events(&c,MODE_CHANGED)==1);
  light_for(&c,2100,1000,60);assert(events(&c,SERVICE_ACCESS)==1 && !c.journal.alarm_count);
  int32_t hit[]={700,0,1000};controller_acceleration(&c,3400,true,hit,0);assert(c.journal.alarm_count==1);
  controller_mode(&c,3500,GUARD_MODE);assert(events(&c,SERVICE_OPEN_SUSPECTED)==1 && c.journal.alarm_count==2);
  controller_mode(&c,3600,SERVICE_MODE);assert(events(&c,SERVICE_ACCESS)==2);
}
static void test_simultaneous(void)
{
  controller_t c;initialize(&c);c.filtered_q4=1000*4;
  controller_light(&c,2000,true,1000);
  controller_pir(&c,2400,true);int32_t hit[]={700,0,1000};controller_acceleration(&c,2400,true,hit,0);
  controller_light(&c,2400,true,1000);
  assert(events(&c,CUSTOMER_DETECTED)==1 && events(&c,IMPACT_ALARM)==1 && events(&c,SERVICE_OPEN_SUSPECTED)==1 && c.journal.alarm_count==2);
  for(size_t i=2;i<c.journal.count;++i)assert(log_at(&c.journal,i)->ms==2400);
}
static void test_fault_recovery(void)
{
  controller_t c;initialize(&c);controller_acceleration(&c,2000,false,NULL,MPU_NACK);
  for(unsigned i=0;i<10;++i)controller_acceleration(&c,2010+i*10,false,NULL,MPU_TIMEOUT);
  controller_pir(&c,2200,true);light_for(&c,2200,1000,60);
  assert(events(&c,MPU_FAILED)==1 && events(&c,IMPACT_ALARM)==0 && events(&c,CUSTOMER_DETECTED)==1 && events(&c,SERVICE_OPEN_SUSPECTED)==1);
  for(unsigned i=0;i<100;++i)controller_acceleration(&c,4000+i*10,true,rest,0);
  assert(events(&c,MPU_RECOVERED)==1 && c.impact.ready);
  int32_t hit[]={700,0,1000};controller_acceleration(&c,5100,true,hit,0);assert(events(&c,IMPACT_ALARM)==1);
}
static void test_adc_fault(void)
{
  controller_t c;controller_init(&c,NULL,NULL);controller_light(&c,0,true,3900);
  controller_light(&c,20,false,0);controller_light(&c,40,false,0);assert(c.light_samples==0 && events(&c,ADC_FAILED)==1);
  light_for(&c,100,3900,50);assert(c.light_ready && events(&c,ADC_RECOVERED)==1);
  light_for(&c,1200,1000,10);controller_light(&c,1500,false,0);assert(!c.light_candidate);
  light_for(&c,1600,1000,60);assert(c.light_open);
  controller_light(&c,3000,false,0);controller_mode(&c,3010,SERVICE_MODE);assert(!events(&c,SERVICE_ACCESS));
}
static void test_wrap(void)
{
  controller_t c;initialize(&c);int32_t hit[]={700,0,1000};controller_acceleration(&c,UINT32_MAX-400,true,hit,0);
  controller_acceleration(&c,UINT32_MAX-100,true,rest,0);controller_acceleration(&c,199,true,rest,0);
  assert(!c.impact.latched);
  c.filtered_q4=1000*4;controller_light(&c,UINT32_MAX-200,true,1000);controller_light(&c,199,true,1000);assert(c.light_open);
}
static unsigned delivered;
static void sink(void *ctx,const event_t *e){assert(ctx==&delivered && e->sequence);++delivered;}
static void test_journal(void)
{
  event_log_t log;log_init(&log,sink,&delivered);
  for(unsigned i=0;i<70;++i)log_event(&log,i,i%2?IMPACT_ALARM:CUSTOMER_DETECTED,i%2?VG_MPU:VG_PIR,1);
  assert(log.count==64 && log.overwritten==6 && log.alarm_count==35 && delivered==70);
  assert(log_at(&log,0)->sequence==7 && log_at(&log,63)->sequence==70 && !log_at(&log,64));
  log_event(&log,100,MPU_FAILED,VG_MPU,1);assert(log_at(&log,63)->category==VG_FAULT);
  for(unsigned i=0;i<=ADC_RECOVERED;++i)assert(strlen(event_name((event_kind_t)i)));
  for(unsigned i=0;i<=VG_OPERATOR;++i)assert(strlen(source_name((event_source_t)i)));
  for(unsigned i=0;i<=VG_FAULT;++i)assert(strlen(category_name((event_category_t)i)));
}
#define RUN(test) do { test(); puts("PASS: " #test); } while(0)
int main(void)
{
  RUN(test_driver);RUN(test_raw_units);RUN(test_calibration);RUN(test_rest_noise);RUN(test_impact_boundaries);
  RUN(test_step_threshold);RUN(test_pir);RUN(test_light_guard);RUN(test_light_exact_timing);RUN(test_modes);
  RUN(test_simultaneous);RUN(test_fault_recovery);RUN(test_adc_fault);RUN(test_wrap);RUN(test_journal);
  puts("PASS: 15 groups cover FR-02 through FR-09, driver units and bus failures");
  return 0;
}

#include "audit.h"
#include "cadence.h"
#include "controller.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
typedef struct {
  uint8_t bytes[4096]; int budget, erase_budget;
  bool no_read, corrupt_verify; unsigned writes, erases;
} flash_model_t;
static flash_model_t flash;
static bool rd(void *ctx, uint32_t off, uint8_t *b, size_t n)
{
  flash_model_t *m = ctx;
  if (m->no_read || off+n>sizeof(m->bytes)) return false;
  memcpy(b,m->bytes+off,n);
  if (m->corrupt_verify && m->writes) b[0]^=1;
  return true;
}
static bool er(void *ctx, unsigned page)
{
  flash_model_t *m = ctx; assert(page<2); ++m->erases;
  unsigned n = m->erase_budget<0 ? 2048U : (unsigned)m->erase_budget;
  memset(m->bytes+page*2048,0xff,n); return n==2048;
}
static bool wr(void *ctx, uint32_t off, const uint8_t *b, size_t n)
{
  flash_model_t *m = ctx; assert(n==8 && off%8==0); ++m->writes;
  for (size_t i=0;i<n;++i) {
    if (!m->budget) return false;
    assert(m->bytes[off+i]==0xff); m->bytes[off+i] &= b[i];
    if (m->budget>0) --m->budget;
  }
  return true;
}
static vg_nvm_t io(void) { return (vg_nvm_t){&flash,rd,er,wr}; }
static void fresh(void)
{ memset(&flash,0,sizeof(flash)); memset(flash.bytes,0xff,sizeof(flash.bytes)); flash.budget=-1; flash.erase_budget=-1; }
static event_t access_record(unsigned value)
{ return (event_t){0,1000+value,SERVICE_ACCESS,VG_INFO,VG_LDR,(int32_t)value}; }
static void append(vg_audit_t *a,unsigned v)
{ event_t e=access_record(v); assert(vg_audit_append(a,&e)); }
static void audit_power_cuts(void)
{
  uint8_t saved[4096]; vg_audit_t a;
  fresh(); assert(vg_audit_open(&a,io())); append(&a,1); memcpy(saved,flash.bytes,sizeof(saved));
  for (int cut=0;cut<=32;++cut) {
    memcpy(flash.bytes,saved,sizeof(saved)); flash.budget=cut;
    assert(vg_audit_open(&a,io())); event_t e=access_record(2);
    bool complete=vg_audit_append(&a,&e); assert(complete==(cut==32));
    assert(vg_audit_open(&a,io())); assert(a.serial==(cut==32?2U:1U));
    flash.budget=-1; append(&a,3);
    assert(vg_audit_open(&a,io())); assert(a.serial==(cut==32?3U:2U));
  }
  puts("PASS audit: all 33 byte cut positions reject torn append and resume");
}
static void audit_erase_cuts(void)
{
  fresh(); vg_audit_t a; assert(vg_audit_open(&a,io()));
  for (unsigned i=0;i<128;++i) append(&a,i);
  uint8_t saved[4096]; memcpy(saved,flash.bytes,sizeof(saved));
  for (int cut=0;cut<=2048;++cut) {
    memcpy(flash.bytes,saved,sizeof(saved)); flash.erase_budget=cut; flash.budget=-1;
    assert(vg_audit_open(&a,io())); event_t e=access_record(129);
    bool complete=vg_audit_append(&a,&e); assert(complete==(cut==2048));
    assert(vg_audit_open(&a,io())); assert(a.serial==(complete?129U:128U));
    flash.erase_budget=-1; append(&a,130);
    assert(vg_audit_open(&a,io())); assert(a.serial==(complete?130U:129U));
  }
  puts("PASS audit: all 2049 partial erase positions preserve latest committed page");
}
static void audit_corruption(void)
{
  fresh(); vg_audit_t a; assert(vg_audit_open(&a,io())); append(&a,1); append(&a,2);
  uint8_t good[32]; memcpy(good,flash.bytes+32,32);
  for (unsigned bit=0;bit<256;++bit) {
    memcpy(flash.bytes+32,good,32); flash.bytes[32+bit/8]^=(uint8_t)(1U<<(bit%8));
    assert(vg_audit_open(&a,io())); assert(a.serial==1 && a.damaged==1);
  }
  flash.no_read=true; assert(!vg_audit_open(&a,io())); assert(!a.available);
  event_t e=access_record(5); assert(!vg_audit_append(&a,&e));
  assert(vg_crc32((const uint8_t *)"123456789",9)==0xcbf43926U);
  puts("PASS audit: 256 single-bit corruptions, read failure and CRC-32 reference");
}
static void cadence_gaps(void)
{
  vg_cadence_t t; vg_cadence_init(&t,UINT32_MAX-3999,5000,5000);
  assert(!vg_cadence_due(&t,999)); assert(vg_cadence_due(&t,1000));
  vg_cadence_finish(&t,1000,1200); assert(!t.late && t.execution.max==200);
  assert(vg_cadence_due(&t,22000)); assert(t.skipped==3);
  vg_cadence_finish(&t,22000,28000); assert(t.late==1 && t.jitter_max==16000);
  assert(t.jitter_min==16000 && t.jitter_n==1 && t.jitter_bins[3]==1);
  assert(t.next==26000); assert(!vg_cadence_due(&t,25000));
  assert(vg_cadence_due(&t,26000)); vg_cadence_finish(&t,26000,26100);
  assert(t.jitter_min==-1000 && t.jitter_bins[2]==1);
  assert(vg_cadence_due(&t,31000)); vg_cadence_finish(&t,31000,31100);
  assert(vg_cadence_due(&t,36050)); vg_cadence_finish(&t,36050,36150);
  assert(t.jitter_bins[0]==1 && t.jitter_bins[1]==1);
  puts("PASS cadence: wrap, phase retention, skipped releases, completion deadline");
}
static void audit_unknown_ack(void)
{
  fresh(); vg_audit_t a; assert(vg_audit_open(&a,io()));
  flash.corrupt_verify=true; event_t e=access_record(7);
  assert(!vg_audit_append(&a,&e)); assert(!a.available);
  flash.corrupt_verify=false; assert(vg_audit_open(&a,io()));
  assert(a.serial==1); append(&a,7);
  assert(vg_audit_open(&a,io())); assert(a.serial==2 && a.valid==2);
  /* A lost verification acknowledgement allows a duplicate, never silent loss. */
  uint8_t *record=flash.bytes;
  const unsigned fields[]={20,21,22,23};
  for (unsigned i=0;i<4;++i) {
    uint8_t saved=record[fields[i]]; record[fields[i]]=255;
    uint32_t crc=vg_crc32(record,24);
    for(unsigned j=0;j<4;++j)record[24+j]=(uint8_t)(crc>>(8*j));
    uint32_t serial,boot;event_t out;
    assert(!vg_audit_decode(record,&out,&serial,&boot));
    record[fields[i]]=saved;
  }
  puts("PASS audit: readback uncertainty is explicit, retries may duplicate; invalid typed records rejected");
}
static unsigned kind_count(const controller_t *c,event_kind_t k)
{
  unsigned n=0; for(size_t i=0;i<c->journal.count;++i) if(log_at(&c->journal,i)->kind==k)++n;
  return n;
}
static void vending_shift(void)
{
  controller_t c; controller_init(&c,NULL,NULL); vg_cadence_t sample;
  vg_cadence_init(&sample,0,10000,10000);
  int32_t rest[3]={0,0,1000}; unsigned launches=0,customers=0,services=0;
  /* One logical hour, not a real-time MCU/Wokwi endurance measurement. */
  for(uint32_t ms=0;ms<=3600000;ms+=5) {
    if(vg_cadence_due(&sample,ms*1000U)) {
      controller_acceleration(&c,ms,true,rest,0); vg_cadence_finish(&sample,ms*1000U,ms*1000U+100U); ++launches;
    }
    bool customer=ms%30000U==5000U;
    uint32_t before=c.journal.next_sequence;
    controller_pir(&c,ms,customer);
    if(customer) { assert(c.journal.next_sequence==before+1); ++customers; }
    if(ms%60000U==10000U)controller_mode(&c,ms,SERVICE_MODE);
    if(ms%60000U==15000U)controller_mode(&c,ms,GUARD_MODE);
    if(ms%20U==0)controller_light(&c,ms,true,(ms%60000U>=11000U && ms%60000U<13000U)?1000:3900);
    if(c.journal.next_sequence!=before && c.journal.count) {
      const event_t *last=log_at(&c.journal,c.journal.count-1);
      if(last->kind==SERVICE_ACCESS)++services;
    }
  }
  assert(launches==360000 && customers==120 && services==60);
  assert(!c.journal.alarm_count && !sample.skipped && !sample.late);
  assert(kind_count(&c,SERVICE_OPEN_SUSPECTED)==0);
  printf("PASS VendGuard logical hour: imu=%u customers=%u service_access=%u alarms=%lu\n",
    launches,customers,services,(unsigned long)c.journal.alarm_count);
}
int main(void)
{
  audit_power_cuts(); audit_erase_cuts(); audit_corruption(); audit_unknown_ack(); cadence_gaps(); vending_shift();
  puts("HOST ONLY: no MCU timing, physical power loss or Wokwi run claimed");
}

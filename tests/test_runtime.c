/* HOST ORCHESTRATION TEST. Stub values are not MCU measurements or raw evidence. */
#include "board.h"
#include "instrument.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
static uint32_t ticks, lost, overrun;
static char input[256], output[65536];
static size_t input_size, cursor, output_size;
static uint8_t nvm[4096];
static bool write_fails;
static unsigned writes, heartbeats;
static unsigned missing_probes;
static uint32_t edge;
static bool edge_pending;
int vg_test_printf(const char *format,...)
{
  va_list args; va_start(args,format);
  int n=vsnprintf(output+output_size,sizeof(output)-output_size,format,args);
  va_end(args); assert(n>=0 && (size_t)n<sizeof(output)-output_size);
  output_size+=(size_t)n; return n;
}
uint32_t vg_now(void) { return ticks++; }
uint32_t vg_millis(void) { return ticks/1000U; }
void vg_clock_init(void) {}
void vg_stack_paint(void) {}
unsigned vg_stack_used(void) { return 0; }
vg_heap_t vg_heap_read(void) { return (vg_heap_t){0}; }
void vg_probe(unsigned pin,bool on) { (void)pin;(void)on; }
static bool read_nvm(void *ctx,uint32_t off,uint8_t *data,size_t n)
{ (void)ctx; assert(off+n<=sizeof nvm);memcpy(data,nvm+off,n);return true; }
static bool erase_nvm(void *ctx,unsigned page)
{ (void)ctx;assert(page<2);memset(nvm+page*2048,255,2048);return true; }
static bool program_nvm(void *ctx,uint32_t off,const uint8_t *data,size_t n)
{
  (void)ctx;++writes;if(write_fails)return false;
  assert(off+n<=sizeof nvm);for(size_t i=0;i<n;++i)nvm[off+i]&=data[i];return true;
}
vg_nvm_t vg_internal_flash(void) { return (vg_nvm_t){NULL,read_nvm,erase_nvm,program_nvm}; }
static bus_result_t read_bus(void *ctx,uint8_t address,uint8_t reg,uint8_t *data,size_t n)
{
  (void)ctx;if(address!=0x68) { if(reg==0x75)++missing_probes;return BUS_NACK; }
  memset(data,0,n);
  if(reg==0x75 && n==1)data[0]=0x68;
  if(reg==0x3b && n==6)data[4]=0x40;
  return BUS_OK;
}
static bus_result_t write_bus(void *ctx,uint8_t address,uint8_t reg,const uint8_t *data,size_t n)
{ (void)ctx;(void)reg;(void)data;(void)n;return address==0x68?BUS_OK:BUS_NACK; }
register_bus_t board_bus(void) { return (register_bus_t){NULL,read_bus,write_bus}; }
bool board_light(uint16_t *counts) { *counts=3938;return true; }
bool board_customer(void) { return edge_pending; }
void board_console_start(void) {}
void board_console_irq(void) {}
bool board_console_timed(char *ch,uint32_t *us)
{
  if(cursor==input_size)return false;
  *ch=input[cursor++];*us=ticks;return true;
}
bool board_console_byte(char *ch) { uint32_t unused;return board_console_timed(ch,&unused); }
void board_console_discard(void) { cursor=input_size; }
void board_rx_stats(uint32_t *a,uint32_t *b,uint32_t *c) { *a=lost;*b=overrun;*c=0; }
void board_tx_stats(uint32_t *a,uint32_t *b) { *a=0;*b=0; }
bool board_customer_event(uint32_t *us)
{ if(!edge_pending)return false;*us=edge;edge_pending=false;return true; }
void board_heartbeat(void) { ++heartbeats; }
static void enqueue(const char *s)
{ assert(cursor==input_size);input_size=strlen(s);assert(input_size<sizeof input);memcpy(input,s,input_size);cursor=0; }
static void steps(unsigned n)
{ for(unsigned i=0;i<n;++i) { ticks+=1000;vendguard_poll(); } }
static void command(const char *s) { enqueue(s);steps(4); }
static void clear_output(void) { output_size=0;output[0]=0; }
int main(void)
{
  memset(nvm,255,sizeof nvm);vendguard_start();
  steps(1200);clear_output();
  command("measure on\n");steps(5);command("stats\n");
  char *console=strstr(output,"SCHED name=console ");assert(console);
  unsigned skipped,deadline;
  assert(sscanf(console,"SCHED name=console skipped=%u deadline_miss=%u",&skipped,&deadline)==2);
  assert(skipped==0 && deadline==0);
  puts("PASS HOST runtime: measure-on boundary does not create a false deadline miss");
  clear_output();enqueue("service\n");++lost;steps(2);
  command("ice\n");assert(!strstr(output,"MODE_CHANGED"));
  command("service\n");assert(strstr(output,"event=MODE_CHANGED value=1"));
  clear_output();enqueue("guard\n");++overrun;steps(2);
  command("ard\n");assert(!strstr(output,"MODE_CHANGED"));
  command("guard\n");assert(strstr(output,"event=MODE_CHANGED value=0"));
  puts("PASS HOST runtime: software overflow and ORE discard suffix commands and resume at a delimiter");
  clear_output();write_fails=true;edge=ticks;edge_pending=true;steps(20);
  assert(strstr(output,"event=CUSTOMER_DETECTED") && strstr(output,"STORE result=FAILED"));
  unsigned after_failure=writes;edge=ticks;edge_pending=true;steps(20);
  assert(writes==after_failure);command("status\n");assert(strstr(output,"alarms=0"));
  command("mpu 69\n");assert(strstr(output,"event=MPU_FAILED"));
  unsigned probes_after_command=missing_probes;steps(1200);
  assert(missing_probes>probes_after_command);
  command("mpu 68\n");steps(20);assert(strstr(output,"event=MPU_RECOVERED"));
  assert(heartbeats>0);
  puts("PASS HOST runtime: storage failure does not stop customer events or MPU fault/recovery");
  puts("HOST ONLY: register, clock, NVM and memory adapters were stubs; no MCU timing claimed");
}

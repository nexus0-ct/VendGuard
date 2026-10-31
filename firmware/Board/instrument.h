#ifndef VG_INSTRUMENT_H
#define VG_INSTRUMENT_H
#include "audit.h"
void vg_clock_init(void);
uint32_t vg_now(void);
uint32_t vg_millis(void);
void TIM3_IRQHandler(void);
void vg_stack_paint(void);
unsigned vg_stack_used(void);
vg_nvm_t vg_internal_flash(void);
void vg_probe(unsigned pin, bool on);
typedef struct { unsigned arena, used, free; } vg_heap_t;
vg_heap_t vg_heap_read(void);
#endif

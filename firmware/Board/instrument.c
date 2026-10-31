#include "instrument.h"
#include "main.h"
#include <string.h>
#include <malloc.h>
static volatile uint64_t timer_epoch;
static uint64_t previous_clock;
extern uint32_t _estack, _Min_Stack_Size;
void vg_clock_init(void)
{
  __HAL_RCC_TIM3_CLK_ENABLE();
  TIM3->CR1 = 0; TIM3->PSC = SystemCoreClock/1000000U-1U;
  TIM3->ARR = 65535; TIM3->EGR = TIM_EGR_UG; TIM3->SR = 0;
  timer_epoch = 0; TIM3->CNT = 0; TIM3->DIER = TIM_DIER_UIE;
  HAL_NVIC_SetPriority(TIM3_IRQn,0,0); HAL_NVIC_EnableIRQ(TIM3_IRQn);
  TIM3->CR1 = TIM_CR1_CEN;
  __HAL_RCC_GPIOA_CLK_ENABLE();
  GPIO_InitTypeDef pins = {0};
  pins.Pin = GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8;
  pins.Mode = GPIO_MODE_OUTPUT_PP; pins.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOA,&pins);
}
void TIM3_IRQHandler(void)
{
  if (TIM3->SR & TIM_SR_UIF) { TIM3->SR = 0; timer_epoch += 65536U; }
}
static uint64_t clock_snapshot(void)
{
  /* A model can expose wrapped CNT before UIF. Restore interrupt state and
   * retry such an inconsistent snapshot instead of returning time backwards.
   * This does not recover multiple wraps hidden by a long interrupt mask. */
  for (;;) {
    uint32_t mask = __get_PRIMASK(); __disable_irq();
    uint64_t high = timer_epoch;
    uint32_t low = TIM3->CNT;
    uint32_t sr = TIM3->SR;
    if (sr & TIM_SR_UIF) { high += 65536U; low = TIM3->CNT; }
    uint64_t result = high + low;
    if (result < previous_clock) { __set_PRIMASK(mask); continue; }
    previous_clock = result;
    __set_PRIMASK(mask);
    return result;
  }
}
uint32_t vg_now(void) { return (uint32_t)clock_snapshot(); }
uint32_t vg_millis(void) { return (uint32_t)(clock_snapshot()/1000U); }
void vg_probe(unsigned pin, bool on)
{ GPIOA->BSRR = 1U << (pin + (on ? 0U : 16U)); }
void vg_stack_paint(void)
{
  uint32_t *p = (uint32_t *)((uintptr_t)&_estack-(uintptr_t)&_Min_Stack_Size);
  uint32_t *limit = (uint32_t *)__get_MSP()-16;
  while (p < limit) *p++ = 0xdeadbeefU;
}
unsigned vg_stack_used(void)
{
  uint32_t *p = (uint32_t *)((uintptr_t)&_estack-(uintptr_t)&_Min_Stack_Size);
  while (p < &_estack && *p == 0xdeadbeefU) ++p;
  return (unsigned)((uintptr_t)&_estack-(uintptr_t)p);
}
static bool flash_read(void *ctx, uint32_t off, uint8_t *b, size_t n)
{
  (void)ctx;
  if (off > 4096 || n > 4096-off) return false;
  memcpy(b,(const void *)(0x08007000U+off),n); return true;
}
static bool flash_erase(void *ctx, unsigned page)
{
  (void)ctx;
  if (page > 1 || HAL_FLASH_Lock() != HAL_OK || HAL_FLASH_Unlock() != HAL_OK) return false;
  FLASH_EraseInitTypeDef op = {0}; uint32_t error = 0;
  op.TypeErase = FLASH_TYPEERASE_PAGES; op.Page = 14+page; op.NbPages = 1;
  HAL_StatusTypeDef result = HAL_FLASHEx_Erase(&op,&error);
  HAL_FLASH_Lock();
  if (result != HAL_OK) return false;
  const volatile uint32_t *words = (const volatile uint32_t *)(0x08007000U+page*2048U);
  for (unsigned i = 0; i < 512; ++i) if (words[i] != 0xffffffffU) return false;
  return true;
}
static bool flash_program(void *ctx, uint32_t off, const uint8_t *b, size_t n)
{
  (void)ctx;
  /* Require real LOCK/key side effects before touching the program array. */
  if (n != 8 || off > 4088 || (off & 7U) || HAL_FLASH_Lock() != HAL_OK || HAL_FLASH_Unlock() != HAL_OK) return false;
  const volatile uint32_t *old = (const volatile uint32_t *)(0x08007000U+off);
  if (old[0] != 0xffffffffU || old[1] != 0xffffffffU) { HAL_FLASH_Lock(); return false; }
  uint64_t value; memcpy(&value,b,8);
  HAL_StatusTypeDef result = HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD,0x08007000U+off,value);
  HAL_FLASH_Lock(); return result == HAL_OK;
}
vg_nvm_t vg_internal_flash(void)
{ return (vg_nvm_t){NULL,flash_read,flash_erase,flash_program}; }
vg_heap_t vg_heap_read(void)
{
  struct mallinfo h = mallinfo();
  return (vg_heap_t){(unsigned)h.arena,(unsigned)h.uordblks,(unsigned)h.fordblks};
}

#include "board.h"
#include "main.h"
extern ADC_HandleTypeDef hadc1;
extern I2C_HandleTypeDef hi2c1;
extern UART_HandleTypeDef huart2;
static volatile uint8_t console_head, console_tail;
static char console_rx[64];
/* RM0490 I2C: program CR2 in one write, avoiding intermediate START states. */
static void wire_pause(void)
{
  uint32_t remaining = SystemCoreClock / 1000000U * 120U;
  uint32_t previous = SysTick->VAL;
  while (remaining) {
    uint32_t current = SysTick->VAL;
    uint32_t elapsed = previous >= current ? previous - current : previous + SysTick->LOAD + 1U - current;
    if (elapsed >= remaining) break;
    remaining -= elapsed;
    previous = current;
  }
}
static bus_result_t await_flag(uint32_t flag, uint32_t started)
{
  for (;;) {
    uint32_t flags = I2C1->ISR;
    if (flags & I2C_ISR_NACKF) return BUS_NACK;
    if (flags & (I2C_ISR_BERR | I2C_ISR_ARLO)) return BUS_ERROR;
    if (flags & flag) return BUS_OK;
    if ((uint32_t)(HAL_GetTick() - started) >= 5U) return BUS_TIMEOUT;
  }
}
static void bus_cleanup(bus_result_t status)
{
  if (status != BUS_OK) {
    I2C1->CR1 &= ~I2C_CR1_PE;
    I2C1->CR1 |= I2C_CR1_PE;
  }
  I2C1->ICR = I2C_ICR_STOPCF | I2C_ICR_NACKCF | I2C_ICR_BERRCF | I2C_ICR_ARLOCF;
  I2C1->CR2 = 0;
}
static bus_result_t transfer(uint8_t addr, uint8_t *bytes, size_t length, bool receive)
{
  if (!length || length > 16) return BUS_ERROR;
  uint32_t started = HAL_GetTick();
  I2C1->ICR = I2C_ICR_STOPCF | I2C_ICR_NACKCF | I2C_ICR_BERRCF | I2C_ICR_ARLOCF;
  I2C1->CR2 = ((uint32_t)addr << 1) | ((uint32_t)length << I2C_CR2_NBYTES_Pos)
              | I2C_CR2_AUTOEND | I2C_CR2_START | (receive ? I2C_CR2_RD_WRN : 0U);
  bus_result_t status = BUS_OK;
  for (size_t i = 0; i < length; ++i) {
    if (!receive) wire_pause();
    status = await_flag(receive ? I2C_ISR_RXNE : I2C_ISR_TXIS, started);
    if (status != BUS_OK) break;
    if (receive) bytes[i] = (uint8_t)I2C1->RXDR;
    else I2C1->TXDR = bytes[i];
  }
  if (status == BUS_OK) status = await_flag(I2C_ISR_STOPF, started);
  bus_cleanup(status);
  return status;
}
static bus_result_t read_register(void *context, uint8_t addr, uint8_t reg,
                                  uint8_t *bytes, size_t length)
{
  (void)context;
  bus_result_t status = transfer(addr, &reg, 1, false);
  return status == BUS_OK ? transfer(addr, bytes, length, true) : status;
}
static bus_result_t write_register(void *context, uint8_t addr, uint8_t reg,
                                   const uint8_t *bytes, size_t length)
{
  (void)context;
  uint8_t packet[16];
  if (length > 15) return BUS_ERROR;
  packet[0] = reg;
  for (size_t i = 0; i < length; ++i) packet[i + 1] = bytes[i];
  return transfer(addr, packet, length + 1, false);
}
register_bus_t board_bus(void)
{
  return (register_bus_t){NULL, read_register, write_register};
}
bool board_light(uint16_t *counts)
{
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);
  HAL_StatusTypeDef status = HAL_ADC_Start(&hadc1);
  if (status == HAL_OK) status = HAL_ADC_PollForConversion(&hadc1, 2);
  if (status == HAL_OK) *counts = (uint16_t)HAL_ADC_GetValue(&hadc1);
  HAL_ADC_Stop(&hadc1);
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);
  return status == HAL_OK;
}
bool board_customer(void) { return HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_10) == GPIO_PIN_SET; }
void board_console_start(void)
{
  HAL_NVIC_SetPriority(USART2_IRQn, 1, 0);
  HAL_NVIC_EnableIRQ(USART2_IRQn);
  __HAL_UART_ENABLE_IT(&huart2, UART_IT_RXNE);
}
void board_console_irq(void)
{
  uint32_t flags = USART2->ISR;
  if (flags & USART_ISR_RXNE_RXFNE) {
    char ch = (char)USART2->RDR;
    uint8_t next = (uint8_t)((console_head + 1U) % sizeof(console_rx));
    if (next != console_tail) { console_rx[console_head] = ch; console_head = next; }
  }
  USART2->ICR = USART_ICR_ORECF | USART_ICR_NECF | USART_ICR_FECF | USART_ICR_PECF;
}
bool board_console_byte(char *ch)
{
  if (console_head == console_tail) return false;
  *ch = console_rx[console_tail];
  console_tail = (uint8_t)((console_tail + 1U) % sizeof(console_rx));
  return true;
}
void board_heartbeat(void) { HAL_GPIO_TogglePin(Led_GPIO_Port, Led_Pin); }

#include "board.h"
#include "main.h"
extern ADC_HandleTypeDef hadc1;
extern I2C_HandleTypeDef hi2c1;
extern UART_HandleTypeDef huart2;
static bus_result_t result(HAL_StatusTypeDef status)
{
  if (status == HAL_OK) return BUS_OK;
  if (status == HAL_TIMEOUT) return BUS_TIMEOUT;
  if (HAL_I2C_GetError(&hi2c1) & HAL_I2C_ERROR_AF) return BUS_NACK;
  return BUS_ERROR;
}
static bus_result_t read_register(void *context, uint8_t addr, uint8_t reg,
                                  uint8_t *bytes, size_t length)
{
  (void)context;
  return result(HAL_I2C_Mem_Read(&hi2c1, (uint16_t)addr << 1, reg,
                               I2C_MEMADD_SIZE_8BIT, bytes, (uint16_t)length, 5));
}
static bus_result_t write_register(void *context, uint8_t addr, uint8_t reg,
                                   const uint8_t *bytes, size_t length)
{
  (void)context;
  return result(HAL_I2C_Mem_Write(&hi2c1, (uint16_t)addr << 1, reg,
                                I2C_MEMADD_SIZE_8BIT, (uint8_t *)bytes,
                                (uint16_t)length, 5));
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
bool board_console_byte(char *ch)
{
  return HAL_UART_Receive(&huart2, (uint8_t *)ch, 1, 0) == HAL_OK;
}
void board_heartbeat(void) { HAL_GPIO_TogglePin(Led_GPIO_Port, Led_Pin); }

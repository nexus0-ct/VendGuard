# Залежності прошивки

Основа ЛР1: 8056a0f. STM32C0 HAL 1.0.1 і CMSIS збережено без змін із ліцензіями. ADC/ADCEx/LL_ADC тієї самої HAL 1.0.1 взято з офіційного ST tag v1.0.1, commit 24999b605d0b9da3e4675c4c884036ae724bb59f.

ADC потрібен для LDR; I²C для MPU; UART для протоколу; GPIO для PIR/LED/маркера; RCC/FLASH/PWR/CORTEX для старту й SysTick. EXTI залишається для бортової кнопки. DMA потрібний для типів HAL I²C/UART, transfers DMA не виконуються. SPI/TIM/RTC не ввімкнено.

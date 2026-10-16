# Залежності прошивки

Основа ЛР1: 8056a0f. STM32C0 HAL 1.0.1 і CMSIS збережено без змін із ліцензіями. ADC/ADCEx/LL_ADC тієї самої HAL 1.0.1 взято з офіційного ST tag v1.0.1, commit 24999b605d0b9da3e4675c4c884036ae724bb59f.

ADC потрібен для LDR; I²C для MPU; UART для протоколу; GPIO для PIR/LED/маркера; RCC/FLASH/PWR/CORTEX для старту й SysTick. EXTI залишається для бортової кнопки. DMA потрібний для типів HAL I²C/UART, transfers DMA не виконуються. SPI/TIM/RTC не ввімкнено.

MPU6050 driver власний, без Arduino бібліотек. [Register Map Rev 4.2](https://invensense.tdk.com/wp-content/uploads/2015/02/MPU-6000-Register-Map1.pdf): SMPLRT_DIV §4.3, CONFIG §4.4, ACCEL_CONFIG §4.6, ACCEL_XOUT_H §4.17, PWR_MGMT_1/2 §4.28–29, WHO_AM_I §4.30. Шість big-endian байтів, signed int16, 16384 LSB/g. Помилка не змінює вихідний sample.

Docker використовує Ubuntu 22.04 за digest 5ec03bb3441e8b0bf3b4f9cd4629a1ae763010dc3035bb8da3ae6cf026486401. ARM GCC 15:10.3-2021.07-4, binutils 2.38-3ubuntu1+15build1, newlib 3.3.0-1.3 зафіксовано версіями apt. Host GCC і Python входять у test environment; його фактичний склад зберігається в toolchain log. GitHub runner Ubuntu 24.04 виконує Docker build, host tests, sanitizers, coverage та перевірку порогів. Checkout action зафіксовано commit 11d5960a326750d5838078e36cf38b85af677262. Власні Board/Sensors/Logic збираються з -Wall -Wextra -Werror; ST джерела з -Wall.

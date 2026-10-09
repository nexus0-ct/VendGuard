# Аналіз обміну VendGuard

I²C bring-up записано з реально запущеної HAL-прошивки у Wokwi. Оригінали знаходяться в Lab02/Work/captures; docs/captures містить фрагменти із SHA-256 та часовим зміщенням у VCD header. Зміст сигналів не генерується скриптами. crop_vcd.py лише зберігає переходи обраного інтервалу.

Часова роздільність Wokwi VCD 1 нс. sigrok використовує downsample=50, 20 МГц. D0=SCL, D1=SDA, D2=UART_TX, D3=PIR, D4=ADC_SAMPLE. Запис починається зі стартом симулятора, без окремого тригера; bufferSize=10000000. Для фізичного аналізатора START на SDA при SCL=HIGH є природною умовою trigger.

## I²C

Перше звернення до WHO_AM_I: START; address 0x68 write; регістр 0x75; slave ACK; STOP; новий START; 0x68 read; data 0x68; master NACK; STOP. Адресний byte 11010000 =0xD0, R/W=0; регістр 01110101 =0x75; read address 11010001 =0xD1. Дев’ятий низький біт означає ACK. Завершальний NACK читання є нормальною дією master.

Адаптер атомарно задає CR2, використовує AUTOEND та STOP між pointer і read. Пауза 120 мкс перед TXDR перевищує 9 тактів по 10 мкс із запасом 30 мкс; RX читається за RXNE без штучних пауз. STM32 HAL виконує початкове налаштування, vendor sources не змінюються.

Запис показує SCL 100 кГц. Перший six-byte burst має bytes 00 00 00 00 40 00, тобто 0, 0, 1000 mg. Модуль у спокої 1g, модельна похибка для цього положення 0%. Тестовий вектор та provenance наведено у tests/fixtures/captured-rest.md.

Два записи попереднього bring-up показали аномалії HAL: незавершене memory read після ACK pointer та зайве address 0x00/NACK при переході до read через часткові зміни CR2. Після заміни transfer adapter отримано правильний ID і burst. Це спостереження моделі Wokwi, без перенесення висновку на фізичний STM32.

## Відтворення

Зібрати firmware, запустити Wokwi, дочекатися BACKGROUND_READY і LIGHT_READY. probe читає ID/6 bytes. mpu 69 створює відсутню адресу, mpu 68 повертає робоче опитування. Stop зберігає wokwi.vcd; зберегти оригінал перед наступним запуском.

Відкрити VCD у PulseView з downsample=50. I²C decoder: scl=SCL, sda=SDA; UART: RX=UART_TX, baudrate=115200, 8N1. Текстові анотації формує sigrok-cli. analyze_signals.py незалежно декодує переходи й вимірює час, не стискаючи пауз.

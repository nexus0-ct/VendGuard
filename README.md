# VendGuard

Захист корпусу торгового автомата від ударів та реєстрація світлової ознаки відкриття сервісного відсіку. Поява покупця є інформаційною подією. Світлова ознака в охороні дає тривогу, в обслуговуванні — сервісну подію. Удар реєструється в обох режимах.

Шостак Богдан, ІО-36, №20. STM32C031C6, STM32 HAL, Docker, Ubuntu 24.04, Wokwi. ЛР1 злито в main; ЛР2 міститься у feature/lab-2, фінальний комплект позначено тегом lab2.

## Збірка та перевірка

```sh
make build
make test
make sanitize
make coverage
```

ARM-збірка створює firmware/build/firmware.elf, firmware.hex і firmware.bin. Для однакового GCC-покриття з CI:

```sh
docker run --rm -v "$PWD:/workspace" -w /workspace stm32c0-wokwi-build:local make coverage
python3 tools/check_coverage.py tests/coverage/Linux-aarch64
```

Каталог покриття відповідає архітектурі контейнера; на x86_64 використати Linux-x86_64. Хостові тести перевіряють переносні Sensors/Logic; власний STM32-код перевіряється ARM-збіркою і Wokwi.

## Wokwi

Відкрити корінь проєкту у VS Code, виконати `Wokwi: Start Simulator`. Початкові умови: LDR 1 lux, MPU Z=1g, PIR LOW. Дочекатися LIGHT_READY та BACKGROUND_READY. BOOT/CHECK/READY і STATUS повідомляють стан; LED змінює стан раз на секунду.

Команди з Enter: `guard`, `service`, `status`, `probe`, `journal`, `calibrate`, `mpu 69`, `mpu 68`. Для зовнішнього UART увімкнено RFC2217 localhost:4000. Скрипт tools/serial_command.py використовує pyserial 3.5:

```sh
python3 -m venv /tmp/vendguard-serial
/tmp/vendguard-serial/bin/pip install pyserial==3.5
/tmp/vendguard-serial/bin/python tools/serial_command.py status --output /tmp/vendguard-status.txt
```

Stop зберігає wokwi.vcd. Перед наступним запуском зберегти оригінал. Аналізатор: D0=SCL, D1=SDA, D2=UART_TX, D3=PIR, D4=ADC_SAMPLE, D5=UART_RX.

## Матеріали ЛР2

- [Звіт PDF](docs/lab_2/report.pdf).
- [Підключення та первинні джерела](docs/pinout.md).
- [Архітектура](docs/architecture.md) та [числові критерії](docs/lab2-design.md).
- [UART-протокол](docs/uart.md), [тести](docs/testing.md), [залежності](docs/dependencies.md).
- [Записи та вимірювання](docs/captures/analysis.md): VCD, sigrok .sr, налаштування .pvs, декодування і SHA-256.
- [PRD](docs/PRD.md), [контекст](CONTEXT.md), [результати лабораторних](docs/lab-plan.md).
- [Текст pull request](docs/lab_2/pull-request.md).

Сигнали записано із запущеної прошивки Wokwi. Фізичні струми, електричні фронти та реальні удари потребують окремого стенда. Журнал зберігається у RAM; календарного часу й автентифікації UART немає.

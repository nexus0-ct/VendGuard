# VendGuard

Захист корпусу торгового автомата від ударів та реєстрація світлової ознаки відкриття сервісного відсіку. Поява покупця є інформаційною подією. Світлова ознака в охороні дає тривогу, в обслуговуванні — сервісну подію. Удар реєструється в обох режимах.

Шостак Богдан, ІО-36. STM32C031C6, STM32 HAL, Docker, Ubuntu 24.04, Wokwi. ЛР3 продовжує ЛР2 у feature/lab-3. [Робочий звіт](docs/lab3-report.md), [задачі](docs/tasks.md), [методика](docs/baseline-lab4.md), [обмеження](docs/limitations.md). Попередня eed2 завершила S1/S2/S3, fault/recovery, UART boundary і30 стартів. Часові вимоги порушені: PIR609,238 мс, світлове повідомлення1074,019 мс у S3. Попередні результати позначено окремою версією.

## Збірка та перевірка

```sh
make build
make test
make sanitize
make coverage
```

ARM-збірка створює firmware/build/firmware.elf, firmware.hex і firmware.bin. Для однакового GCC-покриття з CI:

```sh
docker run --rm -v "$PWD:/workspace" -w /workspace vendguard-build:lab3 make coverage
python3 tools/check_coverage.py tests/coverage/Linux-aarch64
```

Каталог покриття відповідає архітектурі контейнера; на x86_64 використати Linux-x86_64. Хостові тести перевіряють переносні Sensors/Logic; власний STM32-код перевіряється ARM-збіркою і Wokwi.

## Wokwi

Відкрити корінь проєкту у VS Code, виконати `Wokwi: Start Simulator`. Початкові умови: LDR 1 lux, MPU Z=1g, PIR LOW. Дочекатися LIGHT_READY та BACKGROUND_READY. BOOT/CHECK/READY і STATUS повідомляють стан; LED змінює стан раз на секунду.

Команди з Enter: `guard`, `service`, `status`, `probe`, `journal`, `calibrate`, `mpu 69`, `mpu 68`. Для зовнішнього UART увімкнено RFC2217 localhost:4136. Скрипт tools/serial_command.py використовує pyserial 3.5:

ЛР3 додає `version`, `measure on`, `stats`, `memory`, `telemetry 0/1`, `verify`, `durable`, `store pause` і `store retry`. `stats` зупиняє вимірюване вікно до нового `measure on`. Опис метрик і повних сценаріїв міститься у [baseline-lab4.md](docs/baseline-lab4.md).

```sh
python3 -m venv /tmp/vendguard-serial
/tmp/vendguard-serial/bin/pip install pyserial==3.5
/tmp/vendguard-serial/bin/python tools/serial_command.py status --output /tmp/vendguard-status.txt
```

Stop зберігає docs/captures/lab3/vendguard-current.vcd. Перед наступним запуском зберегти оригінал через tools/archive_lab3_capture.py. Підключення аналізатора ЛР3 наведено у [tasks.md](docs/tasks.md). Історичні записи ЛР2 мають початкові канали.

## Матеріали ЛР2

- [Звіт PDF](docs/lab_2/report.pdf).
- [Підключення та первинні джерела](docs/pinout.md).
- [Архітектура](docs/architecture.md) та [числові критерії](docs/lab2-design.md).
- [UART-протокол](docs/uart.md), [тести](docs/testing.md), [залежності](docs/dependencies.md).
- [Записи та вимірювання](docs/captures/analysis.md): VCD, sigrok .sr, налаштування .pvs, декодування і SHA-256.
- [PRD](docs/PRD.md), [контекст](CONTEXT.md), [результати лабораторних](docs/lab-plan.md).
- [Текст pull request](docs/lab_2/pull-request.md).

Сигнали записано із запущеної прошивки Wokwi. Фізичні струми, електричні фронти та реальні удари потребують окремого стенда. RAM-журнал доповнено постійним CRC-аудитом. Підтвердження flash/ECC і power-cut ще потрібне; календарного часу й автентифікації UART немає.

# Перевірки VendGuard

Хостові тести виконують переносний код Sensors/Logic, не емулюють периферію STM32. Fake bus зберігає регістри; перевірка sequence очікує точні адреси, порядок і значення п’яти записів MPU. Відмови вводяться на кожній операції, включно з NACK, timeout та IO error. assert.h дає просту C11-збірку; після першого failure процес завершується, окремої ізоляції тестів і mock diagnostics немає.

| Групи | Вимоги та перевірки |
|---|---|
| driver, raw_units | ID, ініціалізація, signed/big-endian, масштаб, failure, реальні 6 bytes із VCD |
| calibration, rest_noise | FR-04: 100 samples, gravity, допустимий розмах, шум, повільна зміна |
| impact_boundaries, step_threshold | FR-04: 649/650 mg, 249/250 mg, axes, latch, 150/151 mg, 299/300 мс |
| pir | FR-02–03: утриманий HIGH, два імпульси, alarm count=0 |
| light_guard, light_exact_timing | FR-05: шум, короткий pulse, 600/300 counts, 399/400 мс, rearm |
| modes | FR-06: service access, удар у service, повернення guard за відкритої ознаки |
| simultaneous | FR-08: PIR та обидві тривоги з одним timestamp |
| fault_recovery, adc_fault | FR-09: одна FAULT, інші канали, RECOVERED, повторна калібровка |
| wrap, journal | FR-07: wrap unsigned tick, час/категорії, 64 records, oldest overwrite |

15 груп проходять macOS arm64 clang і Linux aarch64 GCC. ASan/UBSan завершуються без діагностик. Для Sensors/Logic встановлено критерії ≥90% рядків та ≥80% branch outcomes, щоб перевірити нормальні й помилкові переходи. Фактично GCC: усі чотири модулі 100% lines; controller 97,92%, impact 95,24%, event_log і MPU 100% branch taken. Повні логи збережено в docs/validation.

Тест короткого світлового імпульсу виявив продовження confirmation через хвіст EMA. Виправлення вимагає одночасного виконання порога сирим і filtered ADC. Цей самий regression test після виправлення проходить.

FR-01 та апаратні адаптери перевіряються ARM build і реально запущеною прошивкою Wokwi. Числа host coverage не поширюються на Core, Board чи vendor HAL. Фізичні струми, електричні фронти та вібрація корпусу належать до фізичного стенда; форма B досліджує модель і цифровий обмін.

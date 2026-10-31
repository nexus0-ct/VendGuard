# Результати ЛР3 VendGuard

Поточний build `vg3-eed2dae477f4fd8f`, ELF SHA-256 `b6681b1a211dcd06cc175925fb4d651db004903671e63df4c761d41684c46ca0`. Підсумкові сценарії 29.10.2026 завершені. [Звіт і висновки](../../lab3-report.md), [final assessment](current-eed2/final-assessment.json), [журнал збірки](final-build/manifest-portable.json).

`current-eed2/` містить вибрані первинні UART S1, S2, S3-A, S3-B, S3-LDR та їхні manifests. CSV final-stats, final-scheduler, final-jitter включають основні й додаткові вікна, не усереднюючи їх між собою. Повні VCD, додаткові серії та оригінальні службові журнали залишені локально; вибрані time-preserving fragments та plots є в `docs/captures/lab3/current-eed2/`. У копії журналу збірки локальні шляхи знеособлено, оригінал не змінено.

S2: один MCU window 3665,642537 с, stable stack968/heap1032. S3: 32 PIR всередині export, worst609237,925 мкс; 32 LDR trials worst1074019,125 мкс;31 in-window impact notifications. UART start boundary3/4, RX loss10/11. Startup30. S3-A/B stack972. Хеші runner розділяють профілі, firmware той самий.

Фізична flash/ECC/power-cut перевірка не підтверджена. Старий f2 ELF з зависанням TC зберігається як історія налагодження. Його числа не використовуються в поточних таблицях.


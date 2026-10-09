# Реальні байти MPU6050

Оригінал: Lab02/Work/captures/bringup-register-full.vcd, SHA-256 3d03d7d4d22f2b9febb790e84eee0af92e1220595c59f7e9081ef83c2bd8a463. Запис зроблено під час налагодження адаптера I²C. Незмінні переходи від 0,09 до 1,3 с містить docs/captures/i2c-bringup.vcd.

Перше шестибайтове читання: START 0,136219625 с, адреса 0x68/R, bytes 00 00 00 00 40 00, п’ять data ACK та завершальний master NACK; STOP 0,136864625 с. Перетворення дає 0, 0, 1000 mg. UART STATUS підтверджує ці значення. test_raw_units використовує ці самі байти, а signed_extremes є окремими синтетичними граничними test vectors.

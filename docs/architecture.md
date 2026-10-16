# Структура VendGuard

Core містить init/IRQ та main. Board містить адаптери I²C, ADC, GPIO, UART й планувальник. Sensors містить MPU6050 через register_bus_t. Logic містить незалежні правила, режими та журнал. Drivers містить HAL/CMSIS.

Core викликає Board; Board поєднує Sensors і Logic та використовує HAL; Sensors знає лише read/write callbacks. Logic не включає HAL. tools/check_layers.py перевіряє заборонені апаратні залежності, host compilation перевіряє незалежність виконання.

Номери каналів і peripheral handles зосереджено в Board; згенерована конфігурація Core має відповідати .ioc. Прикладні виклики main розташовані в USER CODE, нові модулі знаходяться поза згенерованими каталогами. Після регенерації звіряють Makefile та периферію.

Heartbeat раз на 1000 мс показує цикл. Головний цикл не містить секундної затримки; кожний канал має свій термін опитування.

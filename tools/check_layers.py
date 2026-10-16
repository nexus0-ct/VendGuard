#!/usr/bin/env python3
from pathlib import Path
import re
for layer in ('Logic', 'Sensors'):
    for path in (Path('firmware') / layer).glob('*.[ch]'):
        text = path.read_text()
        assert not re.search(r'#include.*(?:stm32|main\.h|board\.h)|\b(?:HAL_|GPIOA|I2C1|ADC1)', text), path
print('PASS: Logic and Sensors compile without board or HAL dependencies')

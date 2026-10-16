#!/usr/bin/env python3
from pathlib import Path
import json
schema=json.loads(Path('diagram.json').read_text())
edges={frozenset((a,b)) for a,b,*_ in schema['connections']}
required={'customer:OUT':'nucleo:PA10','serviceLight:AO':'nucleo:PA0','body:SCL':'nucleo:PB8','body:SDA':'nucleo:PB9',
          'body:AD0':'nucleo:GND.1','body:VCC':'nucleo:3V3.1','serviceLight:VCC':'nucleo:3V3.1','customer:VCC':'nucleo:5V.1'}
for channel,pin in enumerate(['PB8','PB9','PA2','PA10','PA4']):required[f'capture:D{channel}']=f'nucleo:{pin}'
for part in ['body','customer','serviceLight','capture']:required[f'{part}:GND']='nucleo:GND.1'
for a,b in required.items():assert frozenset((a,b)) in edges,(a,b)
config=dict(line.split('=',1) for line in Path('firmware/firmware.ioc').read_text().splitlines() if '=' in line)
for key,value in {'PA0.Signal':'ADC1_IN0','PA10.Signal':'GPIO_Input','PA4.Signal':'GPIO_Output','PB8.Signal':'I2C1_SCL','PB9.Signal':'I2C1_SDA','PA2.Signal':'USART2_TX','PA3.Signal':'USART2_RX','PA5.Signal':'GPIO_Output','Mcu.Package':'LQFP48'}.items():assert config[key]==value,(key,config.get(key))
assert len({required[x] for x in ['customer:OUT','serviceLight:AO','body:SCL','body:SDA']})==4
print('PASS: Wokwi connections, analyzer channels and .ioc pin assignments agree')

main=Path('firmware/Core/Src/main.c').read_text()
for label in ['PIR','ADC_SAMPLE']:
    assert f'GPIO_InitStruct.Pin = {label}_Pin;' in main,label
    assert f'HAL_GPIO_Init({label}_GPIO_Port' in main,label
print('PASS: PIR and ADC marker GPIO initialization is present in firmware')

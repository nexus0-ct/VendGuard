# Лабораторна робота №3: VendGuard

Шостак Богдан, ІО-36. Системне програмування. Форма B, Wokwi, STM32C031C6, 48 МГц.

Реалізація працює без ОС. Поточний ELF завершив S1, неперервний S2 понад годину, S3, адресний NACK/recovery, серію UART bursts і 30 незалежних стартів. Часові критерії порушені. Синхронний UART export затримує єдиний foreground. Постійний flash-аудит у моделі не підтверджений.

## Постановка та реалізація

PIR створює CUSTOMER_DETECTED/INFO. Удар дає IMPACT_ALARM в guard і service. Світлова ознака дає SERVICE_ACCESS/INFO у service та SERVICE_OPEN_SUSPECTED/ALARM у guard. PIR не дозволяє й не блокує інші канали. Logic не залежить від HAL; MPU-драйвер використовує callbacks.

Один foreground stack виконує console 1 мс, customer 5 мс, mechanical 10 мс, light 20 мс, recovery 1 с, audit 1 мс, telemetry 500 мс і heartbeat 1 с. ISR зберігає байти або PIR-фронти з часовими мітками. Розклад зберігає фазу, пропущені release не надолужує штучними повторними читаннями. STAT містить n/min/avg/max; SCHED відділяє skipped від deadline miss; JITTER зберігає знак і гістограму. TIM3 1 МГц розширюється IRQ до 64 біт. Одночасне маскування IRQ на кілька wrap не відновлюється цим способом.

Аудит має дві flash-сторінки по 64 записи, запис 32 байти, little-endian, CRC перших 24 байтів і commit останнім doubleword. Foreground-черга 8 подій; після FAILED writer вимкнений до store retry. RAM-журнал продовжує роботу. At-least-once retry може дати дублікат при невизначеному readback.

## Версія та перевірки

Build `vg3-eed2dae477f4fd8f`, ELF SHA-256 `b6681b1a211dcd06cc175925fb4d651db004903671e63df4c761d41684c46ca0`. ARM GCC 13.2.1, binutils 2.42, newlib 4.4.0, Ubuntu 24.04, `-Os`. Шість фактично виконаних перевірок описано у [журналі збірки](metrics/lab3/final-build/manifest-portable.json). 24 групи C-тестів пройшли macOS/clang і Linux/GCC з ASan/UBSan. Host-модель аудиту охопила 33 обриви program, 2049 часткових erase і 256 bit flips. Це не MCU endurance чи фізичний ECC.

Flash image 27792/28672 байти, залишок 880. Область аудиту 4096 байтів зарезервована окремо. `.data=160`, `.bss=3664`, static RAM=3824. Stack reserve=2048, minimum heap=512, linker RAM=6384/12288. Runtime heap arena/used=1032/1032, free=0; maximum observed stack=972. З реальною heap і повним резервом stack бюджет 6904/12288. Найбільший одиночний `.su` frame vendguard_poll=208 не є повним call chain.

## Поточні вимірювання 29.10.2026

| Сценарій | MCU, с | Task load, проміле | PIR n | Stack байти |
|---|---|---|---|---|
| S1 | 63.796442 | 137 | 0 | 968 |
| S2 | 3665.642537 | 173 | 32 | 968 |
| S3-A | 243.991976 | 623 | 40 | 972 |
| S3-B | 243.779495 | 623 | 39 | 972 |
| S3-LDR | 304.222225 | 626 | 0 | 968 |
| S3-LDR-extra | 44.152607 | 589 | 0 | 968 |
| S3-MPU-extra | 44.216694 | 587 | 0 | 968 |
| fault-parallel | 33.869278 | 112 | 1 | 968 |

Task load підсумовує корисну роботу задач. Головний цикл busy-spin без WFI, тому це не вільний відсоток фізичного CPU. SCHED audit включає порожні перевірки, не тільки program.

S1 без входів і telemetry: mechanical 6379 запусків, 1232/1251/1269 мкс; skipped=0; jitter −976…+1065 мкс. VCD 6378 повних періодів PA6 має 9020,325/10000,046/11068,275 мкс. GPIO width avg 1098,873 проти STAT 1251 мкс має різні кінцеві точки. DONE clock crosscheck: MCU 78037003 мкс, VCD 78037067,3 мкс, різниця −64,3 мкс. Це шкала симуляції, не калібрування кварцу.

S2 є одним неперервним запуском без reset: MCU 3665,642537 с, host 3678,104401 с. completed/endurance_adequate=true. Усі 32 PIR зіставлені без втрат, MCU min/avg/max 23/2549/8873 мкс, VCD 31,250/2557,491/8880,925 мкс. Три сервісні світлові ступені мали верхні затримки 416305,325/409223,475/401882,300 мкс, без ALARM. П'ять фактичних MEM показали незмінні stack=968, heap=1032. Mechanical skipped=4119, misses=3428, jitter −8189…+27860 мкс. Після години RAM export seq97…160, не доказ flash integrity. 61 хвилина не перетнула low32 wrap через 71,58 хвилини.

S3-A guard та S3-B service мають 40 і 39 придатних PIR. Усередині завершених journal exports лежать 19+13=32 фронти, max latency=609237,925 мкс, min/avg=19045,325/316758,886 мкс. FIFO pairing valid, drops=0. p99 nearest rank дорівнює максимуму малої вибірки. 402 повністю вкладені експорти A/B дали span min/avg/max 619576,931/629428,725/644125,406 мкс. Нижня оцінка 10N/115200 дала 468142,361/473428,431/481770,833 мкс. Сумарна надбавка до wire minimum 32,08/32,95/33,98%; це не окрема ціна кожного HAL/printf.

S3-LDR і supplement дали 28+4=32 придатні світлові ступені. UART EVENT upper endpoint min/avg/max 403492,075/626158,345/1074019,125 мкс. Максимум ADC lower endpoint 1072247,375 мкс доводить внутрішнє порушення 500 мс; 19 із 32 доводять порушення нижньою межею, 13 вкладаються верхньою. Дві пізні дії головної серії виключені. 12+15+4=31 ударне повідомлення у вікнах A/B/supplement мають ALARM. Вхідного GPIO-маркера удару немає, тому host-click не дає точної апаратної затримки.

S3 використовує seed36, цільовий інтервал .75 с ±25%, 20 status back-to-back, delimiter перед journal, очікування реального DONE до 2 с. Handshake подовжує фактичні інтервали. Не називати це рівномірними 26,7 командами/с. Точні runner snapshots і manifests мають окремі SHA, firmware незмінний. Пілот з одним export і нуль PIR усередині нього не підміняє підсумок.

| Сценарій | RX dropped | Audit dropped | TX timeouts | PIR dropped |
|---|---|---|---|---|
| S1 | 0 | 0 | 0 | 0 |
| S2 | 0 | 152 | 1 | 0 |
| S3-A | 15554 | 110 | 721 | 0 |
| S3-B | 15429 | 113 | 698 | 0 |
| S3-LDR | 19173 | 86 | 900 | 0 |
| S3-LDR-extra | 2618 | 62 | 124 | 0 |
| S3-MPU-extra | 2618 | 62 | 128 | 0 |
| fault-parallel | 0 | 0 | 0 | 0 |

Це cumulative counters від reset, включно з warmup. ORE і tx_errors дорівнюють нулю. TX timeout означає непідтверджений TC; доставку перевіряють raw UART. RX software drops відрізняються від ORE.

## Межі та відмова

Окремий reset перед кожним UART-рівнем, 15 с, telemetry 500 мс, один burst status за host-секунду. Start latency враховує тільки прийняті команди.

| Burst | Заплановано | Прийнято / виміряно | Start max мкс | RX dropped |
|---|---|---|---|---|
| 1 | 15 | 15 | 1268 | 0 |
| 2 | 30 | 30 | 23885 | 0 |
| 3 | 45 | 45 | 44956 | 0 |
| 4 | 60 | 60 | 66867 | 0 |
| 5 | 75 | 75 | 89190 | 0 |
| 8 | 120 | 120 | 154827 | 0 |
| 10 | 150 | 150 | 215891 | 0 |
| 11 | 165 | 1 | 540 | 105 |
| 16 | 240 | 10 | 73523 | 574 |
| 64 | 960 | 16 | 140678 | 4357 |
| 128 | 1920 | 47 | 183464 | 9717 |

FR-12 задає номінал до однієї короткої команди за секунду. На рівні1 max1268 мкс, вимогу виконано. Межа50 мс за перевантаження між bursts3/4: 44956/66867 мкс. Межа RX-втрат між 10/11: 0/105 байтів. На рівні11 лише 1/165 команд виміряна; низький максимум не означає покращення. Це burst-профіль, не рівномірний потік.

Адресний NACK: MPU_FAILED 10365 мс; PIR INFO 20732; LDR ALARM 23812; MPU_RECOVERED 30470; BACKGROUND_READY 31461. Інші канали працювали між fault/recovery. Зміна адреси 0x68→0x69 не є SDA stuck-low.

Окремо 01.11.2026 на тому самому ELF виконано [модельне замикання SDA на землю](metrics/lab3/sda-fault-20261101/README.md). VCD показав безперервний LOW 10,074478 с; UART зафіксував MPU_FAILED, повторні TIMEOUT, потім MPU_RECOVERED і BACKGROUND_READY після звільнення. Це один дослід моделі, не фізична перевірка і не статистика повторів.

30 незалежних reset: VCD-decoded BOOT/FIRST у всіх; від TIM3 перший LDR 42130, PIR initial check 49257, MPU 88260 мкс, min=avg=max детермінованої моделі. До першого UART heap=0 у всіх. Це не power-on latency і PIR check не означає motion latency. Повний direct RFC capture був лише для перших11, підсумок30 послідовно використовує VCD.

Окремий [старт із затримкою готовності тестової I²C-моделі](metrics/lab3/startup-delayed-20261101/README.md): адреса 0x68 до 3 с давала NACK, потім ACK і сталу вибірку 0/0/1g. VCD показав три NACK і перший ACK на 3,086635 с; UART із VCD містить MPU_FAILED 2 мс, FIRST 3094786 мкс, MPU_RECOVERED 3093 мс, BACKGROUND_READY 4083 мс. Це один запуск спрощеної моделі, не фізичний MPU.

## Налагодження та непідтверджені властивості

У старому `vg3-f2a0e90d5be4bb01` GDB довів foreground у UART_WaitOnFlagUntilTimeout, TC=0, TXE=1, HAL_MAX_DELAY, за живих SysTick/TIM3. HardFault breakpoint не спрацював. Причина невстановленого TC у моделі невідома. Поточний board_console_putchar обмежує очікування двома HAL ticks і відновлює RXNEIE після timeout. За працюючого SysTick умова dt>2 дає близько3 мс, не універсальні2 мс. `_write` не передає помилку окремого putchar до printf. Нові S3 повернули повні stats, але довгий синхронний export продовжує затримувати сенсори.

Інші відтворені проблеми: порожній source fingerprint через git у контейнері, неузгоджений TIM3 snapshot, parser resync після RX loss, хвіст старого measure-вікна, подвійна retry-пауза, невірна ініціалізація min jitter, подвійний облік merged rodata, частковий host timestamp. Деталі та межі причинних тверджень наведені в PDF.

Writer у моделі FAILED, verify readable=1/valid=0/damaged=128. RAM export не підтверджує durable flash. Фізичні program/erase stall, ECC/NMI, power-cut, SDA stuck-low і запуск із фізично неготовим сенсором залишаються непідтвердженими. Passport program/erase не видається за власне вимірювання.

## Відтворення та ЛР4

[Усі STAT](metrics/lab3/current-eed2/final-stats.csv), [SCHED](metrics/lab3/current-eed2/final-scheduler.csv), [JITTER](metrics/lab3/current-eed2/final-jitter.csv), [підсумкова атрибуція](metrics/lab3/current-eed2/final-assessment.json), [методика](baseline-lab4.md), [обмеження і гіпотези](limitations.md). Похідні графіки та time-preserving VCD у `docs/captures/lab3/current-eed2/`; кожен має provenance. Повні raw VCD зберігаються локально у власника. Історичні f2/ЛР2 явно відділені й не підміняють поточні таблиці.

Для ЛР4 потрібні асинхронний UART, обмежений бюджет export, ізоляція сенсорів і окреме вимірювання ціни RTOS stack/queues/context switch. Ті самі MCU, швидкість UART, входи й маркери дозволять перевірити гіпотези. RTOS сам по собі не усуває фізичний stall flash.

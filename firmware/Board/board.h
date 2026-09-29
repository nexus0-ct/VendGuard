#ifndef VG_BOARD_H
#define VG_BOARD_H
#include "register_bus.h"
#include <stdbool.h>
#include <stdint.h>
register_bus_t board_bus(void);
bool board_light(uint16_t *counts);
bool board_customer(void);
bool board_console_byte(char *ch);
void board_heartbeat(void);
void vendguard_start(void);
void vendguard_poll(void);
#endif

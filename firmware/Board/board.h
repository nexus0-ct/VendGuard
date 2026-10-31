#ifndef VG_BOARD_H
#define VG_BOARD_H
#include "register_bus.h"
#include <stdbool.h>
#include <stdint.h>
register_bus_t board_bus(void);
bool board_light(uint16_t *counts);
bool board_customer(void);
void board_console_start(void);
int board_console_putchar(int ch);
void board_tx_stats(uint32_t *timeouts, uint32_t *errors);
void board_console_irq(void);
bool board_console_byte(char *ch);
bool board_console_timed(char *ch, uint32_t *us);
bool board_customer_event(uint32_t *us);
void board_rx_stats(uint32_t *dropped, uint32_t *overrun, uint32_t *edges);
void board_console_discard(void);
void board_heartbeat(void);
void vendguard_start(void);
void vendguard_poll(void);
#endif

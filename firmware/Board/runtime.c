#include "board.h"
#include "main.h"
#include <stdio.h>
static uint32_t heartbeat;
void vendguard_start(void)
{
  heartbeat = HAL_GetTick();
  printf("VG BOOT version=2 mode=guard\r\n");
}
void vendguard_poll(void)
{
  uint32_t now = HAL_GetTick();
  if ((uint32_t)(now - heartbeat) >= 1000U) {
    heartbeat = now;
    board_heartbeat();
    printf("VG HEARTBEAT ms=%lu\r\n", (unsigned long)now);
  }
}

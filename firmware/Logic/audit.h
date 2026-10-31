#ifndef VG_AUDIT_H
#define VG_AUDIT_H
#include "event_log.h"
#include <stdbool.h>
#define VG_AUDIT_PAGE 2048U
#define VG_AUDIT_RECORD 32U
#define VG_AUDIT_SLOTS (VG_AUDIT_PAGE / VG_AUDIT_RECORD)
typedef struct {
  void *context;
  bool (*read)(void *, uint32_t, uint8_t *, size_t);
  bool (*erase)(void *, unsigned);
  bool (*program)(void *, uint32_t, const uint8_t *, size_t);
} vg_nvm_t;
typedef struct {
  vg_nvm_t io;
  uint32_t serial, boot, valid, damaged;
  unsigned page, slot;
  bool available;
} vg_audit_t;
uint32_t vg_crc32(const uint8_t *p, size_t n);
bool vg_audit_decode(const uint8_t bytes[VG_AUDIT_RECORD], event_t *event,
                     uint32_t *serial, uint32_t *boot);
bool vg_audit_open(vg_audit_t *a, vg_nvm_t io);
bool vg_audit_append(vg_audit_t *a, const event_t *event);
#endif

#include "audit.h"
#include <string.h>
#define VG_AUDIT_MAGIC 0x33414756U
#define VG_AUDIT_COMMIT 0x51c0a17eU
static uint32_t word(const uint8_t *b)
{ return (uint32_t)b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24; }
static void put(uint8_t *b, uint32_t v)
{ for (unsigned i = 0; i < 4; ++i) b[i] = (uint8_t)(v >> (8U*i)); }
uint32_t vg_crc32(const uint8_t *p, size_t n)
{
  uint32_t c = 0xffffffffU;
  while (n--) {
    c ^= *p++;
    for (unsigned i = 0; i < 8; ++i) c = (c >> 1) ^ (0xedb88320U & (0U-(c & 1U)));
  }
  return ~c;
}
static bool blank(const uint8_t *p)
{
  for (unsigned i = 0; i < VG_AUDIT_RECORD; ++i) if (p[i] != 0xff) return false;
  return true;
}
bool vg_audit_decode(const uint8_t b[VG_AUDIT_RECORD], event_t *e, uint32_t *serial, uint32_t *boot)
{
  if (word(b) != VG_AUDIT_MAGIC || word(b+28) != VG_AUDIT_COMMIT ||
      word(b+24) != vg_crc32(b,24) || b[20] > ADC_RECOVERED ||
      b[21] > VG_FAULT || b[22] > VG_OPERATOR || b[23] != 1) return false;
  *serial = word(b+4); *boot = word(b+8);
  *e = (event_t){*serial, word(b+12), (event_kind_t)b[20],
    (event_category_t)b[21], (event_source_t)b[22], (int32_t)word(b+16)};
  return true;
}
bool vg_audit_open(vg_audit_t *a, vg_nvm_t io)
{
  memset(a, 0, sizeof(*a)); a->io = io;
  int last_used[2] = {-1,-1}; bool found = false;
  uint8_t b[VG_AUDIT_RECORD];
  for (unsigned p = 0; p < 2; ++p) for (unsigned s = 0; s < VG_AUDIT_SLOTS; ++s) {
    if (!io.read(io.context,p*VG_AUDIT_PAGE+s*VG_AUDIT_RECORD,b,sizeof(b))) return false;
    if (blank(b)) continue;
    last_used[p] = (int)s;
    event_t e; uint32_t seq, boot;
    if (!vg_audit_decode(b,&e,&seq,&boot)) { ++a->damaged; continue; }
    ++a->valid;
    if (!found || (int32_t)(seq-a->serial) > 0) {
      found = true; a->serial = seq; a->boot = boot; a->page = p;
    }
  }
  ++a->boot;
  a->slot = (unsigned)(last_used[a->page]+1);
  a->available = true;
  return true;
}
bool vg_audit_append(vg_audit_t *a, const event_t *e)
{
  if (!a->available) return false;
  if (a->slot == VG_AUDIT_SLOTS) {
    unsigned target = a->page ^ 1U;
    /* The page with the newest committed record is never erased here. */
    if (!a->io.erase(a->io.context,target)) { a->available = false; return false; }
    a->page = target; a->slot = 0;
  }
  uint8_t b[VG_AUDIT_RECORD] = {0}, verify[VG_AUDIT_RECORD];
  put(b,VG_AUDIT_MAGIC); put(b+4,a->serial+1); put(b+8,a->boot);
  put(b+12,e->ms); put(b+16,(uint32_t)e->value);
  b[20] = (uint8_t)e->kind; b[21] = (uint8_t)e->category; b[22] = (uint8_t)e->source; b[23] = 1;
  put(b+24,vg_crc32(b,24)); put(b+28,VG_AUDIT_COMMIT);
  uint32_t off = a->page*VG_AUDIT_PAGE + a->slot++*VG_AUDIT_RECORD;
  /* CRC+commit occupy the last doubleword. Never rewrite a used doubleword. */
  for (unsigned i = 0; i < sizeof(b); i += 8) {
    if (!a->io.program(a->io.context,off+i,b+i,8)) { a->available = false; return false; }
  }
  if (!a->io.read(a->io.context,off,verify,sizeof(verify)) || memcmp(b,verify,sizeof(b))) {
    a->available = false; return false;
  }
  ++a->serial;
  return true;
}

#ifndef __PACKET
#define __PACKET

#include <stddef.h>
#include <stdint.h>

#pragma pack(push, 1)
struct PacketIngest {
  uint32_t instrument_id;
  uint64_t seq_no;
  uint8_t  type;       // 0 ADD, 1 CANCEL, 2 TRADE
  uint8_t  side;       // 0 BUY, 1 SELL (TRADE: aggressor)
  uint16_t flags;      // bit0 duplicate marker
  int32_t  price_ticks;
  uint32_t qty;
  uint64_t order_id;
};
#pragma pack(pop)

static_assert(sizeof(PacketIngest) == 32);

#endif

#ifndef __INGEST
#define __INGEST

#include <unordered_map>
#include "packet.h"
#include "ringb.h"
#include "utl.h"
#include "book.h"

template <size_t BackBufferSize, size_t Instruments, typename SPSC = ring_buffer<PacketIngest, 128>>
class Ingest
{
    static constexpr ptrdiff_t buffer_size_signed = BackBufferSize;
    static constexpr size_t buffer_size_mask = BackBufferSize - 1UZ;

    static_assert((BackBufferSize & buffer_size_mask) == 0);
    static_assert(BackBufferSize <= PTRDIFF_MAX);

    struct InstrSeq
    {
        InstrSeq(size_t index, PacketIngest *back_buffer)
            : back_buffer_ref{back_buffer + BackBufferSize * index}, next_seq{1}
        {
        }

        PacketIngest *back_buffer_ref;
        uint64_t next_seq;
    };

    std::unordered_map<uint32_t, InstrSeq> seqs;
    alignas(alignof(PacketIngest)) PacketIngest back_buffer_[BackBufferSize * Instruments];

    SPSC &book_queue;

public:
    Ingest() = delete;
    Ingest(SPSC& spsc);

    void feed(PacketIngest const *p); // always expect full 32 bytes... for simplicity
};

#endif

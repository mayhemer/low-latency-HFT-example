#ifndef __INGEST
#define __INGEST

#include <unordered_map>
#include "packet.h"
#include "ringb.h"
#include "utl.h"
#include "book.h"

template <size_t BackBufferSize, size_t Instruments>
class Ingest
{
    static_assert((BackBufferSize & (BackBufferSize - 1)) == 0);

    struct InstrSeq
    {
        InstrSeq(size_t index, PacketIngest *back_buffer)
            : next_seq{1}, back_buffer_ref{back_buffer + BackBufferSize * index}
        {
        }

        PacketIngest *back_buffer_ref;
        uint64_t next_seq;
    };

    std::unordered_map<uint32_t, InstrSeq> seqs;
    alignas(alignof(PacketIngest)) PacketIngest back_buffer_[BackBufferSize * Instruments];

    Book<Instruments>::SPSC &book_queue;

public:
    Ingest() = delete;
    Ingest(Book<Instruments>::SPSC &bq);

    void feed(PacketIngest const *p); // always expect full 32 bytes... for simplicity

};

#endif

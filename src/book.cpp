#include "book.h"
#include "testdata.h"

template<size_t Instruments>
Book<Instruments>::Book()
    : thread(&Book::thread_root, this)
{
}

template<size_t Instruments>
void Book<Instruments>::thread_root()
{
    for (;;) {
        PacketIngest p;
        feed.load(p);

        // our stop mark...
        if (p.instrument_id == stop_instrument) break;

        printf("instr: %u, seq: %llu\n", p.instrument_id, p.seq_no);
    }
}

template class Book<5>;

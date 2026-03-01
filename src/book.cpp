#include "book.h"

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
        printf("instr: %u, seq: %llu\n", p.instrument_id, p.seq_no);
    }
}

template class Book<5>;

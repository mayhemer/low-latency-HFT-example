#ifndef __BOOK
#define __BOOK

#include <thread>
#include <unordered_map>
#include "ringb.h"
#include "packet.h"

template<size_t Instruments>
class Book
{
public:
    using SPSC = ring_buffer<PacketIngest, 128>;

    struct InstrBook {
        int32_t px_bid{0};
        int32_t px_ask{0};
    };

    std::unordered_map<uint32_t, InstrBook> instrs; 
    SPSC feed;
    Book();

private:
    std::jthread thread;

    void thread_root();
};

#endif

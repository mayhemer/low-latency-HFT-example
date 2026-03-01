#ifndef __BOOK
#define __BOOK

#include <atomic>
#include <thread>
#include <unordered_map>
#include <map>
#include <optional>
#include <functional>

#include "ringb.h"
#include "packet.h"

template <size_t Instruments>
class Book
{
public:
    using SPSC = ring_buffer<PacketIngest, 128>;

    struct OrderRec
    {
        OrderRec(int32_t price) : px(price) {}

        uint32_t qty{0};
        uint64_t next_order{0};
        int32_t px{INT32_MIN};
    };
    
    struct PriceOrders {
        uint64_t first_order{0};
        uint64_t last_order{0};
        uint32_t qty{0};
    };

    struct InstrBook
    {
        InstrBook();

        // order_id -> qty,px
        std::unordered_map<uint64_t, OrderRec> orders_buy;
        std::unordered_map<uint64_t, OrderRec> orders_sell;

        // price -> qty (TODO: PriceOrder)
        std::map<int32_t, uint32_t, std::greater<int32_t>> price_buy;
        std::map<int32_t, uint32_t, std::less<int32_t>> price_sell;

        // TODO: have a FIFO of orders for the TRADE packet
        // to remove orders
        // This is optional as those orders are logically fullfilled
        // and we will never get updates for them.  Only issue: we will
        // leak memory for those orders...
    };

    // instrument id -> record
    std::unordered_map<uint32_t, InstrBook> instrs;
    SPSC feed;
    std::atomic_flag done;

    Book();

    void print();

private:
    std::optional<std::jthread> thread;

    void thread_root();
};

#endif

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
    Book();

    void wait();
    void print();

    using SPSC = ring_buffer<PacketIngest, 128>;

private:
    struct OrderRec
    {
        OrderRec(int32_t price) : px(price) {}

        uint64_t next_order{0};
        uint32_t qty{0};
        int32_t px{INT32_MIN}; // to find the price entry in price_* maps to update it
    };

    struct PriceOrders
    {
        PriceOrders(uint64_t order) : first_order(order), last_order(order) {}

        uint64_t first_order{0};
        uint64_t last_order{0};
        uint32_t qty{0};
    };

    struct InstrBook
    {
        InstrBook();

        // order_id -> OrderRec
        std::unordered_map<uint64_t, OrderRec> orders_buy;
        std::unordered_map<uint64_t, OrderRec> orders_sell;

        // price -> PriceOrder
        std::map<int32_t, PriceOrders> price_buy;
        std::map<int32_t, PriceOrders> price_sell;
    };

    // instrument id -> record
    std::unordered_map<uint32_t, InstrBook> instrs;

    template<size_t, size_t> friend class Ingest;
    SPSC feed;

    std::optional<std::jthread> thread;
    std::atomic_flag done;
    void thread_root();
};

#endif

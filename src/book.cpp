#include "utl.h"
#include "book.h"
#include "testdata.h"

template <size_t Instruments>
Book<Instruments>::Book()
    : instrs(Instruments + 1)
{
    thread.emplace(&Book::thread_root, this);
}

template <size_t Instruments>
void Book<Instruments>::thread_root()
{
    for (;;)
    {
        PacketIngest p;
        feed.load(p);

        // HACK/our stop mark...
        uint32_t instr = p.instrument_id;
        if (instr == stop_instrument)
        {
            LOG("stoppped by stop instrument");
            break;
        }

        LOG("instr: %u, seq: %llu", p.instrument_id, p.seq_no);

        InstrBook &book = instrs[instr];
        if (instrs.size() > Instruments)
        {
            instrs.erase(instr);
            printf("too many instruments");
            continue;
        }

        switch (p.type)
        {
        case 0: // ADD
        {
            auto &orders_map = p.side ? book.orders_sell : book.orders_buy;
            OrderRec &order_rec = orders_map.try_emplace(p.order_id, p.price_ticks).first->second;
            order_rec.qty += p.qty;

            auto &price_map = p.side ? book.price_sell : book.price_buy;
            PriceOrders &price_orders = price_map.try_emplace(p.price_ticks, p.order_id).first->second;
            price_orders.qty += p.qty;
            if (p.order_id != price_orders.first_order)
            {
                auto last_order = orders_map.find(price_orders.last_order);
                last_order->second.next_order = p.order_id;
                price_orders.last_order = p.order_id;
            }

            break;
        }
        case 1: // CANCEL
        {
            auto &orders_map = p.side ? book.orders_sell : book.orders_buy;
            auto order_entry = orders_map.find(p.order_id);
            if (order_entry == orders_map.end())
            {
                break;
            }

            OrderRec &order_rec = order_entry->second;
            order_rec.qty -= std::min(p.qty, order_rec.qty);

            auto &price_map = p.side ? book.price_sell : book.price_buy;
            auto price_entry = price_map.find(order_rec.px);
            if (price_entry == price_map.end())
            {
                break;
            }

            PriceOrders &price_orders = price_entry->second;
            price_orders.qty -= std::min(p.qty, price_orders.qty);
            // else { vvv } ?
            auto last_order = orders_map.find(price_orders.last_order);
            last_order->second.next_order = p.order_id;
            price_orders.last_order = p.order_id;

            if (!price_orders.qty)
            {
                price_map.erase(order_rec.px);
            }

            break;
        }
        case 2: // TRADE
        {
            auto &price_map = p.side ? book.price_buy : book.price_sell;
            auto price_entry = price_map.find(p.price_ticks);
            if (price_entry == price_map.end())
            {
                break;
            }

            PriceOrders &price_orders = price_entry->second;
            price_orders.qty -= std::min(p.qty, price_orders.qty);
            if (!price_orders.qty)
            {
                price_map.erase(p.price_ticks);
            }

            auto &orders_map = p.side ? book.orders_buy : book.orders_sell;

            uint32_t qty = p.qty;
            while (qty)
            {
                auto order_entry = orders_map.find(price_orders.first_order);
                if (order_entry == orders_map.end())
                {
                    break;
                }

                OrderRec &order_rec = order_entry->second;
                uint32_t sub = std::min(order_rec.qty, qty);
                order_rec.qty -= sub;
                qty -= sub;

                price_orders.first_order = order_rec.qty ? order_entry->first : order_rec.next_order;
            }

            break;
        }
        }
    }

    done.test_and_set();
    done.notify_all();
}

template <size_t Instruments>
void Book<Instruments>::print()
{
    LOG("\nprinting final status");
    for (auto instr = instrs.cbegin(); instr != instrs.cend(); ++instr)
    {
        InstrBook const &book = instr->second;
        LOG("instrument: %u", instr->first);
        auto ask = book.price_sell.cbegin();
        auto bid = book.price_buy.crbegin();
        bool has_ask = ask != book.price_sell.cend();
        bool has_bid = bid != book.price_buy.crend();
        int32_t ask_px = has_ask ? ask->first : -1;
        int32_t ask_qty = has_ask ? ask->second.qty : 0;
        int32_t bid_px = has_bid ? bid->first : -1;
        int32_t bid_qty = has_bid ? bid->second.qty : 0;
        LOG("  ask: %d (qty=%u), bid: %d (qty=%u)",
            ask_px, ask_qty, bid_px, bid_qty);

        for (auto order_entry = book.orders_buy.cbegin(); order_entry != book.orders_buy.cend(); ++order_entry)
        {
            LOG("   buy order: %llu, qty: %u, px: %d", order_entry->first, order_entry->second.qty, order_entry->second.px);
        }
        for (auto order_entry = book.orders_sell.cbegin(); order_entry != book.orders_sell.cend(); ++order_entry)
        {
            LOG("  sell order: %llu, qty: %u, px: %d", order_entry->first, order_entry->second.qty, order_entry->second.px);
        }
    }
}

template <size_t Instruments>
inline Book<Instruments>::InstrBook::InstrBook()
    : orders_buy(1024), orders_sell(1024)
{
}

template class Book<5>;

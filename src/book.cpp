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
            LOG("stoppped by stop instrument\n");
            break;
        }

        LOG("instr: %u, seq: %llu\n", p.instrument_id, p.seq_no);

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
            OrderRec &order_rec = (p.side ? book.orders_sell : book.orders_buy)
                                      .try_emplace(p.order_id, p.price_ticks)
                                      .first->second;
            order_rec.qty += p.qty;

            if (p.side)
            {
                uint32_t &price_qty = book.price_sell
                                          .try_emplace(p.price_ticks, 0)
                                          .first->second;
                price_qty += p.qty;
            }
            else
            {
                uint32_t &price_qty = book.price_buy
                                          .try_emplace(p.price_ticks, 0)
                                          .first->second;
                price_qty += p.qty;
            }

            break;
        }
        case 1: // CANCEL
        {
            auto &orders_map = (p.side ? book.orders_sell : book.orders_buy);
            auto iter = orders_map.find(p.order_id);
            if (iter == orders_map.end())
            {
                break;
            }

            OrderRec &order_rec = iter->second;
            order_rec.qty -= std::min(p.qty, order_rec.qty);

            if (p.side)
            {
                uint32_t &price_qty = book.price_sell.try_emplace(order_rec.px, 0).first->second;
                price_qty -= std::min(p.qty, price_qty);
                if (!price_qty)
                {
                    book.price_sell.erase(order_rec.px);
                }
            }
            else
            {
                uint32_t &price_qty = book.price_buy.try_emplace(order_rec.px, 0).first->second;
                price_qty -= std::min(p.qty, price_qty);
                if (!price_qty)
                {
                    book.price_buy.erase(order_rec.px);
                }
            }

            break;
        }
        case 2: // TRADE
        {
            if (p.side)
            {
                uint32_t &price_qty = book.price_sell.try_emplace(p.price_ticks, 0).first->second;
                price_qty -= std::min(p.qty, price_qty);
                if (!price_qty)
                {
                    book.price_sell.erase(p.price_ticks);
                }
            }
            else
            {
                uint32_t &price_qty = book.price_buy.try_emplace(p.price_ticks, 0).first->second;
                price_qty -= std::min(p.qty, price_qty);
                if (!price_qty)
                {
                    book.price_buy.erase(p.price_ticks);
                }
            }

            // TODO: update when we have an order FIFO
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

    // log result
    LOG("\nprinting final status\n");
    for (auto instr = instrs.cbegin(); instr != instrs.cend(); ++instr)
    {
        InstrBook const &book = instr->second;
        LOG("instrument: %u\n", instr->first);
        auto ask = book.price_sell.cbegin();
        auto bid = book.price_buy.cbegin();
        bool has_ask = ask != book.price_sell.cend();
        bool has_bid = bid != book.price_buy.cend();
        int32_t ask_px = has_ask ? ask->first : -1;
        int32_t ask_qty = has_ask ? ask->second : 0;
        int32_t bid_px = has_bid ? bid->first : -1;
        int32_t bid_qty = has_bid ? bid->second : 0;
        LOG("  ask: %d (qty=%u), bid: %d (qty=%u)\n",
            ask_px, ask_qty, bid_px, bid_qty);

        for (auto order = book.orders_buy.cbegin(); order != book.orders_buy.cend(); ++order)
        {
            LOG("   buy order: %llu, qty: %u, px: %d\n", order->first, order->second.qty, order->second.px);
        }
        for (auto order = book.orders_sell.cbegin(); order != book.orders_sell.cend(); ++order)
        {
            LOG("  sell order: %llu, qty: %u, px: %d\n", order->first, order->second.qty, order->second.px);
        }
    }
}

template <size_t Instruments>
inline Book<Instruments>::InstrBook::InstrBook()
    : orders_buy(1024), orders_sell(1024)
{
}

template class Book<5>;

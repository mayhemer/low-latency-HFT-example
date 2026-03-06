# Overview

This is an example of high-frequency-trading code.  It is only made for educational purposes and is far from complete.

_DISCLAIMER: only lightly tested and definitely not for production use, as-is.  This is just a demo, training item._

Now it only contains three parts that I can claim are working well:
1. packet ingest for multiple instruments
2. ringbuffer in push-back and overwrite-oldest variants
3. a simple book builder for multiple instruments

## Ingest

For simplicity, to demo low-latency structures only, it is directly fed with packet structures - no parsing of raw binary data.

It can order incoming packets in a per-instrument sequence when delivered out-of-order.  It handles duplicate packets.  It marks points in the code to send push back hints and retransmit announcements.

Note that packet back buffer capacity is static and limited and oldest packets are thrown away when overran.  This may not be a good approach when a single packet is lost forever and we wait to long for a retransmit.

## Ring buffer

There is a simple SPSC, single producer, single consumer thread-safe, lock-free queue implememtation.  I have SPSC in a push back variant as well as an SPSC that overrides oldest items, both thread safe.  Includes atomated tests.

## Book builder

This is more an outline of how to build a per instrument book.  It holds order history and tracks best bid and ask.  The structure is prepared to produce a snapshot fast.

There are following operations supported:
* ADD, an order with a unique id, to buy or sell for an instrument at price and quantity
* CANCEL, to remove quantity from a buy or sell order with a given id (does not hold price information)
* TRADE, when an agressor makes a trade at a price and quantity (does not refer orders, only price and quantity)

All these operations are updating the book, best ask, and bid.  

For cancellation, we need to track orders for fast access (unordered_map, _O(1)_ add and remove, preallocated).  For trade, we need to keep FIFO of orders (linked list, _O(1)_ updates).  For best ask and bid, we need to keep track of quantities for a price (map - with ordered keys, _O(log N)_ updates [possible bottlenek], min and max for best bid and ask are _O(1)_).

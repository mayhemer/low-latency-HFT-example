# Initial task

Here’s an **8-hour C++ (C++20/C++23) HFT (High-Frequency Trading)**-style coding test you can realistically finish in one day. It’s designed to hit the exact interview surface area: **latency-aware design, concurrency, lock-free/lock-light data structures, correctness, and observability**.

---

## Task: Build a Low-Latency Market Data + Order Gateway Simulator

You will implement a small single-process system with 3 main components:

1. **Market Data Ingest** (UDP-like feed simulator with packet loss/reorder)
2. **Order Book Builder** (top-of-book + limited depth)
3. **Order Gateway** (send orders, receive fills/acks, measure latencies)

All components run as threads connected by **SPSC (Single-Producer Single-Consumer)** ring buffers.

### Time box (8 hours) suggested split

* 1h: architecture + data model + ring buffer
* 2h: market data feed + parser + sequencing
* 2h: order book + top-of-book + snapshot/replay
* 2h: order gateway + matching engine simulator + latency stats
* 1h: tests, perf sanity, polish

---

## Requirements

### A) SPSC Ring Buffer (core infrastructure)

Implement `spsc_ring<T, N>` with:

* fixed capacity `N` (power of two recommended)
* `try_push(const T&)` / `try_pop(T&)` in **O(1)**
* only `std::atomic` for synchronization (**no mutex**)
* cache-line padding to reduce false sharing

**What you’ll be judged on**

* correct memory ordering (`std::memory_order_release/acquire`)
* avoiding ABA (A→B→A) is not needed here due to single producer/consumer, but you must avoid torn publication.

---

### B) Market Data Feed (sequencing + recovery)

You’ll simulate receiving packets (no real sockets required unless you want). Each packet contains:

* `instrument_id` (32-bit)
* `seq_no` (64-bit)
* `type` = `ADD`, `CANCEL`, `TRADE`
* fields for price/qty/order_id

Generator thread:

* emits a stream for 1–4 instruments
* injects:

  * out-of-order delivery (swap neighboring packets)
  * drops (skip some seq numbers)
  * duplicates (repeat some packets)

Ingest thread:

* parses packets (binary decode) into events
* must **detect gaps**
* must maintain a per-instrument **reorder buffer** of small bounded size (e.g., up to 64 packets)
* when a gap is detected, request a **retransmit** from generator (function call is fine)

**Deliverable**

* an in-order event stream per instrument sent to the order book via ring buffer

**What you’ll be judged on**

* gap handling logic
* bounded memory, predictable behavior
* clean separation of concerns

---

### C) Order Book Builder (top-of-book + depth)

Implement a simplified limit order book per instrument:

* maintain best bid/ask (top-of-book) and optionally depth up to `K` levels (e.g., 10)
* handle events:

  * `ADD(order_id, side, price, qty)`
  * `CANCEL(order_id, qty)` (partial allowed)
  * `TRADE(price, qty)` (optional; depending on model)
* **constraint**: you don’t need full matching engine here—just update book state as dictated by feed events

You must support:

* `snapshot()` producing a compact representation
* `apply_snapshot()` and then continue with live events (used by tests)

**What you’ll be judged on**

* data structure choice (price level map vs arrays; predictable latency)
* correctness under event ordering and retransmit

---

### D) Order Gateway + Mini Matching Engine Simulator

Create an “exchange simulator” thread:

* accepts client orders from a ring buffer
* maintains a very simple matching rule against the order book (you can do “cross the spread = immediate fill at best”)
* returns `ACK`, `FILL`, `REJECT` messages via another ring buffer

Client side:

* submits orders periodically (or based on top-of-book changes)
* measures:

  * enqueue time
  * exchange receive time
  * ack/fill time
* prints latency distribution summary:

  * p50, p90, p99, max
  * count of rejects

**What you’ll be judged on**

* time measurement choice (`std::chrono::steady_clock`)
* avoiding allocation in hot paths
* message encoding (struct-of-scalars; avoid std::string)

---

## Constraints (HFT-flavored)

* No dynamic allocation in hot path (after init)
* No locks on hot path (ring buffers only)
* No iostream in hot path (buffer and flush)
* Compile with `-O3 -DNDEBUG`
* Provide a `--duration` and `--instruments` command-line options
* Provide unit tests for:

  * reorder+gap recovery
  * book updates
  * gateway round-trip correctness

---

## Extension objectives (if you finish early)

1. Add **multi-producer** market data fan-in (one feed thread per instrument) using **MPSC (Multi-Producer Single-Consumer)** queue.
2. Add **sequence number watermarking** and book “consistency checks”.
3. Add **CPU affinity** (platform-specific) behind a compile-time switch.
4. Add a **fixed-point price type** and avoid floating point entirely.

---

## Scoring rubric (self-evaluation)

* Correctness under reorder/drop/dup: 40%
* Concurrency correctness: 25%
* Latency awareness / no allocations / minimal branching: 20%
* Test coverage & debuggability: 15%

---

If you want, I can also give you:

* a skeleton repo layout (`src/`, `include/`, `tests/`)
* exact binary packet layout for the feed
* a minimal set of deterministic test vectors (seeded RNG) so you can validate correctness quickly.


# Overview

I still need to better understand on what bases the (simple) order gateway works.  Does it (for the purpose of this exercise) just emit random orders or should there be some simple algorithm to base order emissions on?  Is the matching engine simulator standing for the exchange that also is the source of the original orders that build my book?  Are orders emitted by my order gateway also read by my ingest part and thus update my own book?  You can also add an overall "for dummies" overview of how all the components work together.

---

For this exercise, you’re building a **toy trading system in one process**. The goal is not “trading strategy correctness”, it’s **systems correctness**: message flow, sequencing, concurrency, and latency measurement. So you get to choose how “smart” the order gateway is—as long as it produces a realistic order lifecycle and stresses the pipeline.

Below is the cleanest mental model and two reasonable variants.

---

## 1) What the “order gateway” does in this exercise

Think of the **order gateway** as *your trader’s adapter to the exchange*:

* It **creates orders** (NewOrder messages)
* It **sends orders** to the exchange simulator (over a queue)
* It **receives** acknowledgements and fills/rejects
* It **measures latency** end-to-end

### Does it emit random orders?

It *can*, but pure randomness tends to be a weak test because:

* it may not interact with the book (no fills),
* it may produce unrealistic price distributions.

A better approach is **book-driven but simple**: emit orders based on the current best bid/ask.

---

## 2) What the “matching engine simulator” is

Yes: it stands for the **exchange** (or a tiny part of it). In real life, the exchange has:

* market data dissemination (feeds)
* an order entry gateway
* a matching engine that updates the official book and produces trades

In your program, the simulator thread is the exchange side of the order-entry path. It decides what happens to your orders.

---

## 3) Is the matching engine simulator also the source of the original orders that build my book?

This depends on which architecture variant you pick.

### Variant A (recommended for an 8-hour test): **Market data feed is independent**

* A **market-data generator thread** produces book events (ADD/CANCEL/TRADE) → ingest → book builder.
* Separately, the **exchange simulator thread** processes *your* orders and generates acks/fills.
* Your own orders do **not** feed back into the book builder (unless you explicitly add that as an extension).

This is simplest and still realistic enough: in real trading, your internal market-data view comes from an exchange feed and is not necessarily updated by your own order messages unless you also process your own execution reports into your local state.

### Variant B (more “closed loop”): **Exchange simulator generates market data**

* The exchange simulator is the *source of truth* for the book.
* When it matches orders (including your own), it emits market-data updates/trades into the feed pipeline.
* Your ingest/book builder consumes those updates, so your local book reflects the exchange output.

This is closer to a “full stack” exchange-in-a-box simulation, but more code and more coupling.

For an interview-style task, **Variant A is usually enough** and keeps scope sane.

---

## 4) Do orders emitted by my order gateway also update my own book?

Not automatically. In real systems there are (at least) two distinct internal states:

1. **Market book** (what the market feed says the exchange book looks like)
2. **Your order state** (your working orders, positions, fills)

Your book builder is typically about (1). Your gateway + execution reports are about (2).

If you want to incorporate your own orders into your local “view”, you usually build a **combined view**:

* market book from feed
* plus your own resting orders (from your order management system)
* minus fills/cancels

That’s a real problem (“self-trade prevention”, “order tracking”), but it’s beyond the minimal exercise unless you opt in.

So for this exercise:

* **Book builder** = market feed only.
* **Gateway** = your orders + acknowledgements/fills, measured.

---

## 5) What should drive order emission?

Pick one of these three “reasonable interview” drivers:

### Driver 1: deterministic scripted orders (best for tests)

* Predefined list like: place 1000 orders alternating BUY/SELL.
* Prices derived from top-of-book at time of send.
* Great for unit/integration tests.

### Driver 2: “spread taker” (simple and generates fills)

* Always cross the spread:

  * BUY at `ask_px` (or `ask_px + 1 tick`)
  * SELL at `bid_px` (or `bid_px - 1 tick`)
* You’ll get lots of fills, good for latency stats.
* Not realistic as a strategy, but great as a stress test.

### Driver 3: “market maker” (slightly more realistic, still simple)

* Place resting orders near the mid price:

  * mid = (bid + ask)/2
  * BUY at `mid - 1 tick`, SELL at `mid + 1 tick`
* Many orders won’t fill immediately → more ACK-only, fewer fills.
* Tests both “resting” and “crossing” behavior if you sometimes also send taker orders.

**I recommend Driver 2 for the baseline** (you always get fills, easy to validate), then optionally add Driver 3 as an extension.

---

## 6) “For dummies” overview: how the pieces fit together

### The simplest end-to-end picture (Variant A)

You have three largely independent pipelines:

#### Pipeline 1: Market data → Book

1. **MarketDataGenerator thread** emits packets (ADD/CANCEL/TRADE), with reorder/drop/dup
2. **Ingest thread**:

   * parses packets
   * fixes ordering per instrument (reorder buffer)
   * detects gaps, calls retransmit
3. **BookBuilder thread**:

   * applies events in order
   * maintains top-of-book snapshot (best bid/ask)

Result: at any moment you can read `best_bid` and `best_ask`.

#### Pipeline 2: Orders → Exchange simulator

4. **OrderGateway thread**:

   * reads top-of-book snapshot
   * decides price/side/qty (simple driver)
   * sends NewOrder into `orders_q` with a send timestamp

5. **ExchangeSimulator thread**:

   * pops NewOrder
   * validates it (or rejects)
   * checks top-of-book and decides fill/no-fill
   * pushes ExecReport (ACK, optional FILL/REJECT) into `reports_q`

#### Pipeline 3: Reports → Latency stats

6. **OrderGateway thread** (same thread as #4):

   * pops ExecReport
   * correlates by `client_order_id`
   * computes ack/fill latency = `t_report - t_send`
   * aggregates percentiles, counts

That’s it. The “exchange” does *not* produce the market feed in Variant A; the feed is a separate simulation source.

---

## 7) What to implement minimally (clear acceptance spec)

Baseline implementation that matches the intent:

* Gateway emits orders using Driver 2 (“spread taker”):

  * if ask exists: BUY at ask
  * else if bid exists: SELL at bid
* Exchange simulator:

  * always send ACK
  * if order crosses current top-of-book, send FILL (full or partial vs top qty)
* No feedback from your orders into the market book.

This gives:

* predictable fills
* simple correctness checks
* meaningful latency distributions

---

## 8) Optional “closed loop” extension (if you want more realism)

If you want your orders to affect the market book:

* Exchange simulator maintains the authoritative book.
* On ACK/REST it emits an ADD into the market data stream.
* On CANCEL it emits CANCEL.
* On FILL it emits TRADE and/or reductions.
  Then your ingest/book builder consumes those updates.

This becomes a mini exchange simulator + client, closer to reality, but it’s a bigger build.

---

If you tell me which variant you want (A minimal, B closed-loop), I’ll write the exact message flows and the required state per thread so you can implement without ambiguity. For most interview practice: **Variant A + spread-taker gateway** is the sweet spot.

# Notes, vocab

BID = BUY for the highest price
ASK = SELL for the lowest price


1. Ingest:
    - thread reading raw data packets which are sequenced
    - dedups, put to order, asks for retrans on gaps (assume UDP)
    - feeds to next stage using SPSC ring buffer with push back 
2. Book builder
    - thread reading sequnced orders: ADD, CANCEL, TRADE
    - ADD (instr, order_id, px, qty)
    - CANCEL (instr, order_id, qty)
    - TRADE (instr, buy/sell, px, qty)
    - produces book history and immediate access to best bid and ask

3. Order gateway
    - emits orders (BUY/SELL with instr, qty, px) based on SOME ALGO (ideally spread taker from "overview #6")
    - feeds to the next stage, ordered/sequenced (assume TCP)
    - awaits the response from the next stage
    - measure the time from send to recv for orders
4. Exchange simulator
    - receives orders from me 
    - reads the top book state
    - sends back ACK and maybe FILL when met by the top of the book

Test input for the Ingest:

A1: 65000000010000000000000000000000102700000a000000e903000000000000
A2: 650000000200000000000000000100001527000007000000d107000000000000
A3: 650000000300000000000000000000000b27000005000000ea03000000000000
A4: 650000000400000000000000010000000000000004000000e903000000000000
A5: 6500000005000000000000000200000015270000030000000000000000000000
B1: ca0000000100000000000000000100008813000014000000b90b000000000000
B2: ca000000020000000000000000000000831300000c000000ba0b000000000000
B3: ca0000000300000000000000000100008d1300000a000000a10f000000000000
B4: ca000000040000000000000001000000000000000c000000ba0b000000000000
B5: ca00000005000000000000000200000088130000050000000000000000000000

Expected in-order output after recovery (per instrument)
Instrument 101

Final emitted sequence (deduped, ordered):
A1, A2, A3, A4, A5

Instrument 202

Final emitted sequence (deduped, ordered):
B1, B2, B3, B4, B5

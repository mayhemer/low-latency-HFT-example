#ifndef __RINGB
#define __RINGB

#include <stddef.h>
#include <cstring>
#include <atomic>

#define SYNC_WITH_WAIT true

template <typename T>
constexpr uint32_t bitroot(T N)
{
    return N ? bitroot(N >> 1U) + 1U : 0U;
}

struct ring_debugger
{
    // inside the code
    void _debug_wait_sync_store() { str_rel.wait(true, std::memory_order_relaxed); }
    void _debug_wait_sync_load() { load_acq.wait(true, std::memory_order_relaxed); }

    // inside the test
    void _debug_block_sync_store()
    {
        str_rel.test_and_set(std::memory_order_relaxed);
        str_rel.notify_one();
    }
    void _debug_block_sync_load()
    {
        load_acq.test_and_set(std::memory_order_relaxed);
        load_acq.notify_one();
    }
    void _debug_unblock_sync_store()
    {
        str_rel.clear(std::memory_order_relaxed);
        str_rel.notify_one();
    }
    void _debug_unblock_sync_load()
    {
        load_acq.clear(std::memory_order_relaxed);
        load_acq.notify_one();
    }

private:
    std::atomic_flag str_rel;
    std::atomic_flag load_acq;
};

template <typename T, size_t N>
struct ring_buffer_fallible : public ring_debugger
{
    static constexpr size_t mask = N - 1;
    static constexpr size_t cache_line_size = 64;

    static_assert((N & mask) == 0);

    alignas(cache_line_size) std::atomic<size_t> write{0};
    alignas(cache_line_size) std::atomic<size_t> read{0};
    alignas(alignof(T)) T buffer[N];

    bool store(T const &item)
    {
        size_t r = read.load(std::memory_order_acquire);
        size_t w = write.load(std::memory_order_relaxed);

        size_t w_next = (w + 1) & mask;
        if (w_next == r)
        {
            return false;
        }

        buffer[w] = item;

        _debug_wait_sync_store();

        write.store(w_next, std::memory_order_release);
#if SYNC_WITH_WAIT
        write.notify_one();
#endif
        return true;
    }

    void load(T &item)
    {
        size_t r = read.load(std::memory_order_relaxed);
#if SYNC_WITH_WAIT
        write.wait(r, std::memory_order_acquire);
#else
        while (r == write.load(std::memory_order_acquire))
            ;
#endif
        item = buffer[r];

        _debug_wait_sync_load();

        read.store((++r) & mask, std::memory_order_release);
    }
};

/**
 * Final inspiration for the pop bit from:
 * https://groups.google.com/g/lock-free/c/P1rkoA0Oy7Y/m/qn8hI_81IAAJ
 */

template <typename T, size_t N>
struct ring_buffer_overwriting : public ring_debugger
{
    static constexpr size_t mask = N - 1;
    static constexpr size_t cache_line_size = 64;

    static_assert((N & mask) == 0);

    alignas(cache_line_size) std::atomic<size_t> write{0};
    alignas(cache_line_size) std::atomic<size_t> read{0};
    alignas(alignof(T)) T buffer[N];

    constexpr bool store(T const &item)
    {
        size_t r = read.load(std::memory_order_acquire);
        size_t w = write.load(std::memory_order_relaxed);

        size_t w_next = (w + 1) & mask;
        while (w_next == r && !read.compare_exchange_weak(r, (r + 1) & mask,
                                                          std::memory_order_release,
                                                          std::memory_order_acquire))
        {
        }

        buffer[w] = item;

        _debug_wait_sync_store();

        write.store(w_next, std::memory_order_release);
#if SYNC_WITH_WAIT
        write.notify_one();
#endif

        return true;
    }

    void load(T &item)
    {
        size_t r = read.load(std::memory_order_acquire);
        do
        {
#if SYNC_WITH_WAIT
            write.wait(r, std::memory_order_acquire);
#else
            while (r == write.load(std::memory_order_acquire))
            {
            }
#endif
            item = buffer[r];

            _debug_wait_sync_load();

        } while (!read.compare_exchange_weak(r, (r + 1) & mask,
                                             std::memory_order_release,
                                             std::memory_order_acquire));
    }
};

// Following approaches don't work, one sign is that these are too complicated!
#if 0

template <typename T, size_t N>
struct ring_buffer_overwriting_seq : public ring_debugger
{
    static constexpr size_t mask = N - 1;
    static constexpr uint32_t tag_shift = bitroot(N);
    static constexpr size_t cache_line_size = 64;

    static_assert((N & mask) == 0);

    alignas(cache_line_size) std::atomic<size_t> write{0};
    alignas(cache_line_size) size_t read{0};
    alignas(alignof(size_t)) std::atomic<size_t> seq[N];
    alignas(alignof(T)) T buffer[N];

    constexpr bool store(T const &item)
    {
        size_t w = write.load(std::memory_order_relaxed);
        size_t idx = w & mask;

        buffer[idx] = item;
        seq[idx].store(w, std::memory_order_release);
        write.store(w + 1, std::memory_order_release);
#if SYNC_WITH_WAIT
        write.notify_one();
#endif
        return true;
    }

    void load(T &item)
    {
#if SYNC_WITH_WAIT
        write.wait(read, std::memory_order_relaxed);
#endif

        size_t w = write.load(std::memory_order_acquire);
#if !SYNC_WITH_WAIT
        while (read == w)
        {
            w = write.load(std::memory_order_acquire);
        }
#endif
        size_t s1, s2;
        const size_t span = w - read;
        read += span > N ? span - N : 0;
        do
        {
            size_t const index = read & mask;
            s1 = seq[index].load(std::memory_order_acquire);
            item = buffer[index];

            _debug_wait_sync_load();

            s2 = seq[index].load(std::memory_order_acquire);
        } while (s1 != s2);

        ++read;
    }
};

template <typename T, size_t N>
struct ring_buffer_overwriting_alter : public ring_debugger
{
    static constexpr size_t mask = N - 1;
    static constexpr size_t cache_line_size = 64;

    static_assert((N & mask) == 0);
    static_assert(N < (SIZE_MAX >> 1));

    alignas(cache_line_size) std::atomic<size_t> write{0};
    alignas(cache_line_size) size_t read{0};
    alignas(alignof(T)) T buffer[N];

    constexpr bool store(T const &item)
    {
        size_t w = write.fetch_add(1, std::memory_order_acq_rel); // odd: we write
        buffer[(w >> 1) & mask] = item;

        _debug_wait_sync_store();

        write.store(w + 2, std::memory_order_release); // even: we done
#if SYNC_WITH_WAIT
        write.notify_one();
#endif
        return true;
    }

    void load(T &item)
    {
        size_t w1, w2, span;

#if SYNC_WITH_WAIT
        write.wait(read, std::memory_order_relaxed);
#endif

        do
        {
            w1 = write.load(std::memory_order_acquire);
#if !SYNC_WITH_WAIT
            while (read == w1)
            {
                w1 = write.load(std::memory_order_acquire);
            }
#endif

            item = buffer[read & mask];

            _debug_wait_sync_load();

            w2 = write.load(std::memory_order_acquire);

            while (w2 & 1U)
            {
#if SYNC_WITH_WAIT // makes probably sense only w/o affinity: to give the producer a chance to wake up
                write.wait(w2, std::memory_order_relaxed);
#endif
                w2 = write.load(std::memory_order_acquire);
            }

            span = (w2 >> 1) - read;
            read += span > N ? span - N : 0;
        } while (/* w2 - w1 >= N || */ span > N); // writer overlapped the buffer, possible torn write/read, read cursor must shift

        ++read;
    }
};

#endif

#endif

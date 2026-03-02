#ifndef __RINGB
#define __RINGB

#include <stddef.h>
#include <cstring>
#include <atomic>

#define SYNC_WITH_WAIT true

template <typename T, size_t N>
struct ring_buffer
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
        while (read == write.load(std::memory_order_acquire))
            ;
#endif
        item = buffer[r];
        read.store((++r) & mask, std::memory_order_release);
    }
};

#endif

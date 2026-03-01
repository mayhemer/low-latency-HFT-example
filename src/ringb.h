#ifndef __RINGB
#define __RINGB

#include <stddef.h>
#include <cstring>
#include <atomic>

template <typename T, size_t N>
struct ring_buffer
{
    static_assert((N & (N - 1)) == 0);
    static constexpr size_t cache_line_size = 64;

    alignas(cache_line_size) std::atomic<size_t> write{0};
    alignas(cache_line_size) size_t read{0};
    alignas(alignof(T)) T buffer[N];

    void store(T const &item)
    {
        size_t w = write.load(std::memory_order_relaxed);
        // just override...
        size_t w_next = (w + 1) & (N - 1);
        buffer[w] = item;
        write.store(w_next, std::memory_order_release);
    }

    void load(T &item)
    {
        while (read == write.load(std::memory_order_acquire))
        {
        }

        item = buffer[read];
        read = (read + 1) & (N - 1);
    }
};

#endif
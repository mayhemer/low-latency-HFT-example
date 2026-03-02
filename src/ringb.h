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

template <typename T, size_t N>
struct ring_buffer_fallible
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
        while (r == write.load(std::memory_order_acquire))
            ;
#endif
        item = buffer[r];
        read.store((++r) & mask, std::memory_order_release);
    }
};

template <typename T, size_t N>
struct ring_buffer_overwriting_seq
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

        write.store((w + 1) & mask, std::memory_order_release);
#if SYNC_WITH_WAIT
        write.notify_one();
#endif
        return true;
    }

    void load(T &item)
    {
#if SYNC_WITH_WAIT
        write.wait(read, std::memory_order_acquire);
#else
        while (read == write.load(std::memory_order_acquire))
            ;
#endif
        size_t s1, s2;
        do
        {
            s1 = seq[read].load(std::memory_order_acquire);
            item = buffer[read];
            s2 = seq[read].load(std::memory_order_acquire);
        } while (s1 != s2);

        read = (read + 1) & mask;
    }
};

template <typename T, size_t N>
struct ring_buffer_overwriting_alter
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
        size_t w = write.fetch_add(1, std::memory_order_release); // odd: we write
        buffer[(w >> 1) & mask] = item;
        write.store(w + 2, std::memory_order_release); // even: we done
#if SYNC_WITH_WAIT
        write.notify_one();
#endif
        return true;
    }

    void load(T &item)
    {
        size_t w1;
#if SYNC_WITH_WAIT
        write.wait(read, std::memory_order_relaxed);
#endif
        w1 = write.load(std::memory_order_acquire);
#if !SYNC_WITH_WAIT
        while (read == w1)
        {
            w1 = write.load(std::memory_order_acquire);
        }
#endif
        for (;;)
        {
            item = buffer[read & mask];
            size_t w2 = write.load(std::memory_order_acquire);
            while (w2 & 1U)
            {
#if SYNC_WITH_WAIT // makes probably sense only w/o affinity: to give the producer a chance to wake up
                write.wait(w2, std::memory_order_relaxed);
#endif
                w2 = write.load(std::memory_order_acquire);
            }

            if (w2 - w1 < N)
            {
                // writing didn't loop, the buffer read is not torn
                break;
            }

            w1 = write.load(std::memory_order_acquire);
        }

        ++read;
    }
};

#endif

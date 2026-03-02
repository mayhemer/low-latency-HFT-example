#include <array>
#include <cstddef>
#include <cstdint>
#include <random>

#include "utl.h"
#include "packet.h"
#include "ringb.h"
#include "book.h"
#include "ingest.h"
#include "testdata.h"

uint32_t rand_u32_0_to_N(uint32_t N)
{
    // Thread-local engine: fast, avoids contention, seeded once per thread.
    thread_local std::mt19937 rng{std::random_device{}()};

    // Uniform in <0, N).
    std::uniform_int_distribution<uint32_t> dist(0, N - 1);
    return dist(rng);
}

int main()
{
    using SPSC = SPSC_overwrite;
    Book<5, SPSC> book;
    Ingest<16, 5, SPSC> ingest(book.feed);

    for (unsigned int i = 0; i < 200; ++i)
    {
        auto n = rand_u32_0_to_N(sizeof(test_packets) / sizeof(test_packets[0]));
        ingest.feed(test_packets + n);
    }
    ingest.feed(&stop_packet);

    book.wait();
    book.print();

    return 0;
}

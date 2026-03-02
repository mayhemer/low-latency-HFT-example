#include "ringb.h"
#include <cassert>
#include <latch>
#include <thread>
#include <iostream>

template <template <typename, size_t> typename ring_buffer>
void test_fallible()
{
    ring_buffer<int, 4> r;

    std::latch l(2);
    std::atomic_flag f;
    auto consumer = [&]()
    {
        l.arrive_and_wait();

        int v;

        r.load(v);
        assert(v == 1);
        r.load(v);
        assert(v == 2);
        r.load(v);
        assert(v == 3);
        r.load(v);
        assert(v == 4);

        f.test_and_set();
        f.notify_one();

        r.load(v);
        assert(v == 5);
        r.load(v);
        assert(v == 6);
    };

    std::jthread t(consumer);
    l.arrive_and_wait();

    bool s;

    s = r.store(1);
    assert(s);

    r._debug_block_sync_load();

    s = r.store(2);
    assert(s);
    s = r.store(3);
    assert(s);
    s = r.store(4);
    assert(!s);

    r._debug_unblock_sync_load();

    while (!r.store(4))
        ;

    f.wait(false);
    f.clear();

    s = r.store(5);
    assert(s);
    s = r.store(6);
    assert(s);
}

template <template <typename, size_t> typename ring_buffer>
void test_infallible()
{
    ring_buffer<int, 4> r;

    std::latch l(2);
    std::atomic_flag f1, f2;
    auto consumer = [&]()
    {
        l.arrive_and_wait();

        int v;

        f1.wait(false);

        r.load(v);
        assert(v == 4);
        r.load(v);
        assert(v == 5);
        r.load(v);
        assert(v == 6);

        f2.test_and_set();
        f2.notify_one();

        r.load(v);
        assert(v == 9);
        r.load(v);
        assert(v == 10);
        r.load(v);
        assert(v == 11);
    };

    std::jthread t(consumer);
    l.arrive_and_wait();

    bool s;

    s = r.store(1);
    assert(s);
    s = r.store(2);
    assert(s);
    s = r.store(3);
    assert(s);
    s = r.store(4); // deletes 1
    assert(s);
    s = r.store(5); // deletes 2
    assert(s);
    s = r.store(6); // deletes 3
    assert(s);

    f1.test_and_set();
    f1.notify_one();

    f2.wait(false);

    r._debug_block_sync_load();

    s = r.store(7);
    assert(s);
    s = r.store(8);
    assert(s);
    s = r.store(9);
    assert(s);
    s = r.store(10);
    assert(s);
    s = r.store(11);
    assert(s);

    r._debug_unblock_sync_load();
}

int main()
{
    test_fallible<ring_buffer_fallible>();
    test_infallible<ring_buffer_overwriting>();

#if 0
    // THE FOLLOWING TWO ARE BROKEN
    test_infallible<ring_buffer_overwriting_alter>();
    test_infallible<ring_buffer_overwriting_seq>();
#endif

    std::cout << "done\n";
    return 0;
}
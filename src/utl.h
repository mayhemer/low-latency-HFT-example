#ifndef __UTL
#define __UTL

#include <chrono>

inline uint64_t now_ns() noexcept
{
    using clock = std::chrono::steady_clock;
    return (uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(
        clock::now().time_since_epoch()).count();
}

#endif

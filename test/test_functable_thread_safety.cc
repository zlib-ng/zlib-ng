/* test_functable_thread_safety.cc -- verify concurrent functable initialization.
 *
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include "zbuild.h"
#ifdef ZLIB_COMPAT
#include "zlib.h"
#else
#include "zlib-ng.h"
#endif

#include <gtest/gtest.h>

#include <atomic>
#include <thread>

TEST(functable, thread_safety) {
    uint8_t byte = 0;
    std::atomic<int> barrier{0};

    std::thread t([&]() {
        barrier.fetch_add(1, std::memory_order_relaxed);
        while (barrier.load(std::memory_order_relaxed) < 2) {}
        PREFIX(adler32)(0, &byte, 1);
    });

    barrier.fetch_add(1, std::memory_order_relaxed);
    while (barrier.load(std::memory_order_relaxed) < 2) {}
    PREFIX(crc32)(0, &byte, 1);

    t.join();
}

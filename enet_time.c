#define ENET_BUILDING_LIB 1
#include "enet/enet.h"
#include "atomics.h"

#include <stdint.h>
#include <stdlib.h>
#include <time.h>

#if defined(_WIN32) && defined(_MSC_VER)
#if _MSC_VER < 1900
typedef struct timespec {
    long tv_sec;
    long tv_nsec;
};
#endif
#define CLOCK_MONOTONIC 0
#endif

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#include <unistd.h>
#endif

#ifdef __APPLE__
#include <mach/clock.h>
#include <mach/mach.h>
#include <Availability.h>
#endif

#ifdef _WIN32
static LARGE_INTEGER getFILETIMEoffset()
{
    SYSTEMTIME s;
    FILETIME f;
    LARGE_INTEGER t;

    s.wYear = 1970;
    s.wMonth = 1;
    s.wDay = 1;
    s.wHour = 0;
    s.wMinute = 0;
    s.wSecond = 0;
    s.wMilliseconds = 0;
    SystemTimeToFileTime(&s, &f);
    t.QuadPart = f.dwHighDateTime;
    t.QuadPart <<= 32;
    t.QuadPart |= f.dwLowDateTime;
    return (t);
}

static int clock_gettime(int X, struct timespec *tv)
{
    LARGE_INTEGER t;
    FILETIME f;
    double microseconds;
    static LARGE_INTEGER offset;
    static double frequencyToMicroseconds;
    static int initialized = 0;
    static BOOL usePerformanceCounter = 0;

    if (!initialized) {
        LARGE_INTEGER performanceFrequency;
        initialized = 1;
        usePerformanceCounter = QueryPerformanceFrequency(&performanceFrequency);
        if (usePerformanceCounter) {
            QueryPerformanceCounter(&offset);
            frequencyToMicroseconds = (double)performanceFrequency.QuadPart / 1000000.;
        } else {
            offset = getFILETIMEoffset();
            frequencyToMicroseconds = 10.;
        }
    }
    if (usePerformanceCounter) {
        QueryPerformanceCounter(&t);
    } else {
        GetSystemTimeAsFileTime(&f);
        t.QuadPart = f.dwHighDateTime;
        t.QuadPart <<= 32;
        t.QuadPart |= f.dwLowDateTime;
    }

    t.QuadPart -= offset.QuadPart;
    microseconds = (double)t.QuadPart / frequencyToMicroseconds;
    t.QuadPart = (LONGLONG)microseconds;
    tv->tv_sec = (long)(t.QuadPart / 1000000);
    tv->tv_nsec = t.QuadPart % 1000000 * 1000;
    return (0);
}
#elif __APPLE__ && __MAC_OS_X_VERSION_MIN_REQUIRED < 101200

#define CLOCK_MONOTONIC 0

static int clock_gettime(int X, struct timespec *ts)
{
    clock_serv_t cclock;
    mach_timespec_t mts;

    host_get_clock_service(mach_host_self(), SYSTEM_CLOCK, &cclock);
    clock_get_time(cclock, &mts);
    mach_port_deallocate(mach_task_self(), cclock);

    ts->tv_sec = mts.tv_sec;
    ts->tv_nsec = mts.tv_nsec;

    return 0;
}
#endif

enet_uint32 enet_time_get()
{
    // TODO enet uses 32 bit timestamps. We should modify it to use
    // 64 bit timestamps, but this is not trivial since we'd end up
    // changing half the structs in enet. For now, retain 32 bits, but
    // use an offset so we don't run out of bits. Basically, the first
    // call of enet_time_get() will always return 1, and follow-up calls
    // indicate elapsed time since the first call.
    //
    // Note that we don't want to return 0 from the first call, in case
    // some part of enet uses 0 as a special value (meaning time not set
    // for example).
    static uint64_t start_time_ns = 0;

    struct timespec ts;

    // When building for chromebook with the nacl toolchain for the ARM
    // architecture, CLOCK_MONOTONIC_RAW returns random values from a
    // small set of possibilities. This is not very useful as a measure of
    // time. Luckily, CLOCK_MONOTONIC still works.
#if defined(CLOCK_MONOTONIC_RAW) && !defined(NACL_ARM)
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
#else
    clock_gettime(CLOCK_MONOTONIC, &ts);
#endif

    static const uint64_t ns_in_s = 1000 * 1000 * 1000;
    static const uint64_t ns_in_ms = 1000 * 1000;
    uint64_t current_time_ns = ts.tv_nsec + (uint64_t)ts.tv_sec * ns_in_s;

    // Most of the time we just want to atomically read the start time. We
    // could just use a single CAS instruction instead of this if, but it
    // would be slower in the average case.
    //
    // Note that statics are auto-initialized to zero, and starting a thread
    // implies a memory barrier. So we know that whatever thread calls this,
    // it correctly sees the start_time_ns as 0 initially.
    uint64_t offset_ns = ATOMIC_READ(&start_time_ns);
    if (offset_ns == 0) {
        // We still need to CAS, since two different threads can get here
        // at the same time.
        //
        // We assume that current_time_ns is > 1ms.
        //
        // Set the value of the start_time_ns, such that the first timestamp
        // is at 1ms. This ensures 0 remains a special value.
        uint64_t want_value = current_time_ns - 1 * ns_in_ms;
        uint64_t old_value = ATOMIC_CAS(&start_time_ns, 0, want_value);
        offset_ns = old_value == 0 ? want_value : old_value;
    }

    uint64_t result_in_ns = current_time_ns - offset_ns;
    return (enet_uint32)(result_in_ns / ns_in_ms);
}

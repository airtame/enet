#ifndef TRACETAME_ATOMICS_H
#define TRACETAME_ATOMICS_H

#include <stdbool.h>

#if defined(_MSC_VER)
#if _MSC_VER >= 1910
/* It looks like there were changes as of Visual Studio 2017 and there are no 32/64 bit
   versions of _InterlockedExchange[operation], only InterlockedExchange[operation]
   (without leading underscore), so we have to distinguish between compiler versions */
#define NOT_UNDERSCORED_INTERLOCKED_EXCHANGE
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <intrin.h>
#include <stdint.h>

#define AT_CASSERT_PRED(predicate) sizeof(char[2 * !!(predicate)-1])
#define IS_SUPPORTED_ATOMIC(size) AT_CASSERT_PRED(size == 1 || size == 2 || size == 4 || size == 8)
#define ATOMIC_SIZEOF(variable) (IS_SUPPORTED_ATOMIC(sizeof(*(variable))), sizeof(*(variable)))

__inline int64_t at_atomic_read(char *ptr, size_t size)
{
    switch (size) {
        case 1:
            return _InterlockedExchangeAdd8((volatile char *)ptr, 0);
        case 2:
            return _InterlockedExchangeAdd16((volatile SHORT *)ptr, 0);
        case 4:
#ifdef NOT_UNDERSCORED_INTERLOCKED_EXCHANGE
            return InterlockedExchangeAdd((volatile LONG *)ptr, 0);
#else
            return _InterlockedExchangeAdd((volatile LONG *)ptr, 0);
#endif
        case 8:
#ifdef NOT_UNDERSCORED_INTERLOCKED_EXCHANGE
            return InterlockedExchangeAdd64((volatile LONGLONG *)ptr, 0);
#else
            return _InterlockedExchangeAdd64((volatile LONGLONG *)ptr, 0);
#endif
        default:
            return 0xbad13bad; /* never reached */
    }
}

__inline int64_t at_atomic_write(char *ptr, int64_t value, size_t size)
{
    switch (size) {
        case 1:
            return _InterlockedExchange8((volatile char *)ptr, (char)value);
        case 2:
            return _InterlockedExchange16((volatile SHORT *)ptr, (SHORT)value);
        case 4:
#ifdef NOT_UNDERSCORED_INTERLOCKED_EXCHANGE
            return InterlockedExchange((volatile LONG *)ptr, (LONG)value);
#else
            return _InterlockedExchange((volatile LONG *)ptr, (LONG)value);
#endif
        case 8:
#ifdef NOT_UNDERSCORED_INTERLOCKED_EXCHANGE
            return InterlockedExchange64((volatile LONGLONG *)ptr, (LONGLONG)value);
#else
            return _InterlockedExchange64((volatile LONGLONG *)ptr, (LONGLONG)value);
#endif
        default:
            return 0xbad13bad; /* never reached */
    }
}

__inline int64_t at_atomic_cas(char *ptr, int64_t new_val, int64_t old_val, size_t size)
{
    switch (size) {
        case 1:
            return _InterlockedCompareExchange8((volatile char *)ptr, (char)new_val, (char)old_val);
        case 2:
            return _InterlockedCompareExchange16((volatile SHORT *)ptr, (SHORT)new_val,
                                                 (SHORT)old_val);
        case 4:
#ifdef NOT_UNDERSCORED_INTERLOCKED_EXCHANGE
            return InterlockedCompareExchange((volatile LONG *)ptr, (LONG)new_val, (LONG)old_val);
#else
            return _InterlockedCompareExchange((volatile LONG *)ptr, (LONG)new_val, (LONG)old_val);
#endif
        case 8:
#ifdef NOT_UNDERSCORED_INTERLOCKED_EXCHANGE
            return InterlockedCompareExchange64((volatile LONGLONG *)ptr, (LONGLONG)new_val,
                                                (LONGLONG)old_val);
#else
            return _InterlockedCompareExchange64((volatile LONGLONG *)ptr, (LONGLONG)new_val,
                                                 (LONGLONG)old_val);
#endif
        default:
            return 0xbad13bad; /* never reached */
    }
}

__inline int64_t at_atomic_inc(char *ptr, int64_t delta, size_t data_size)
{
    switch (data_size) {
        case 1:
            return _InterlockedExchangeAdd8((volatile char *)ptr, (char)delta);
        case 2:
            return _InterlockedExchangeAdd16((volatile SHORT *)ptr, (SHORT)delta);
        case 4:
#ifdef NOT_UNDERSCORED_INTERLOCKED_EXCHANGE
            return InterlockedExchangeAdd((volatile LONG *)ptr, (LONG)delta);
#else
            return _InterlockedExchangeAdd((volatile LONG *)ptr, (LONG)delta);
#endif
        case 8:
#ifdef NOT_UNDERSCORED_INTERLOCKED_EXCHANGE
            return InterlockedExchangeAdd64((volatile LONGLONG *)ptr, (LONGLONG)delta);
#else
            return _InterlockedExchangeAdd64((volatile LONGLONG *)ptr, (LONGLONG)delta);
#endif
        default:
            return 0xbad13bad; /* never reached */
    }
}

#define ATOMIC_READ(variable) at_atomic_read((char *)(variable), ATOMIC_SIZEOF(variable))
#define ATOMIC_WRITE(variable, new_val)                                                            \
    at_atomic_write((char *)(variable), (int64_t)(new_val), ATOMIC_SIZEOF(variable))
#define ATOMIC_CAS(variable, old_value, new_val)                                                   \
    at_atomic_cas((char *)(variable), (int64_t)(new_val), (int64_t)(old_value),                    \
                  ATOMIC_SIZEOF(variable))
#define ATOMIC_INC(variable) at_atomic_inc((char *)(variable), 1, ATOMIC_SIZEOF(variable))
#define ATOMIC_DEC(variable) at_atomic_inc((char *)(variable), -1, ATOMIC_SIZEOF(variable))
#define ATOMIC_INC_BY(variable, delta)                                                             \
    at_atomic_inc((char *)(variable), (delta), ATOMIC_SIZEOF(variable))
#define ATOMIC_DEC_BY(variable, delta)                                                             \
    at_atomic_inc((char *)(variable), -(delta), ATOMIC_SIZEOF(variable))

#elif defined(__GNUC__) || defined(__clang__)

#if defined(__clang__) || (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 7))
#define AT_HAVE_ATOMICS
#endif

/* We want to use __atomic built-ins if possible because the __sync primitives are
   deprecated, because the __atomic build-ins allow us to use ATOMIC_WRITE on
   uninitialized memory without running into undefined behavior, and because the
   __atomic versions generate more efficient code since we don't need to rely on
   CAS when we don't actually want it.

   Note that we use acquire-release memory order (like mutexes do). We could use
   sequentially consistent memory order but that has lower performance and is
   almost always unneeded. */
#ifdef AT_HAVE_ATOMICS

#define ATOMIC_READ(ptr) __atomic_load_n((ptr), __ATOMIC_ACQUIRE)
#define ATOMIC_WRITE(ptr, value) __atomic_store_n((ptr), (value), __ATOMIC_RELEASE)

/* clang_analyzer doesn't know that CAS writes to memory so it complains about
   potentially lost data. Replace the code with the equivalent non-sync code. */
#ifdef __clang_analyzer__

#define ATOMIC_CAS(ptr, old_value, new_value)                                                      \
    ({                                                                                             \
        typeof(*(ptr)) ATOMIC_CAS_old_actual_ = (*(ptr));                                          \
        if (ATOMIC_CAS_old_actual_ == (old_value)) {                                               \
            *(ptr) = new_value;                                                                    \
        }                                                                                          \
        ATOMIC_CAS_old_actual_;                                                                    \
    })

#else

/* Could use __auto_type instead of typeof but that shouldn't work in C++.
   The ({ }) syntax is a GCC extension called statement expression. It lets
   us return a value out of the macro.

   TODO We should return bool here instead of the old value to avoid the ABA
   problem. */
#define ATOMIC_CAS(ptr, old_value, new_value)                                                      \
    ({                                                                                             \
        typeof(*(ptr)) ATOMIC_CAS_expected_ = (old_value);                                         \
        __atomic_compare_exchange_n((ptr), &ATOMIC_CAS_expected_, (new_value), false,              \
                                    __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);                           \
        ATOMIC_CAS_expected_;                                                                      \
    })

#endif /* __clang_analyzer__ */

#define ATOMIC_INC(ptr) __atomic_fetch_add((ptr), 1, __ATOMIC_ACQ_REL)
#define ATOMIC_DEC(ptr) __atomic_fetch_sub((ptr), 1, __ATOMIC_ACQ_REL)
#define ATOMIC_INC_BY(ptr, delta) __atomic_fetch_add((ptr), (delta), __ATOMIC_ACQ_REL)
#define ATOMIC_DEC_BY(ptr, delta) __atomic_fetch_sub((ptr), (delta), __ATOMIC_ACQ_REL)

#else

#define ATOMIC_READ(variable) __sync_fetch_and_add(variable, 0)
#define ATOMIC_WRITE(variable, new_val)                                                            \
    (void) __sync_val_compare_and_swap((variable), *(variable), (new_val))
#define ATOMIC_CAS(variable, old_value, new_val)                                                   \
    __sync_val_compare_and_swap((variable), (old_value), (new_val))
#define ATOMIC_INC(variable) __sync_fetch_and_add((variable), 1)
#define ATOMIC_DEC(variable) __sync_fetch_and_sub((variable), 1)
#define ATOMIC_INC_BY(variable, delta) __sync_fetch_and_add((variable), (delta), 1)
#define ATOMIC_DEC_BY(variable, delta) __sync_fetch_and_sub((variable), (delta), 1)

#endif /* AT_HAVE_ATOMICS */

#undef AT_HAVE_ATOMICS

#endif /* defined(_MSC_VER) */
#endif /* TRACETAME_ATOMICS_H */

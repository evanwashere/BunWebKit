/*
 * Copyright (c) 2018-2021 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef PAS_LOCK_H
#define PAS_LOCK_H

#include "pas_log.h"
#include "pas_race_test_hooks.h"
#include "pas_utils.h"
#include <pthread.h>

PAS_BEGIN_EXTERN_C;

enum pas_lock_hold_mode {
    pas_lock_is_not_held,
    pas_lock_is_held
};

typedef enum pas_lock_hold_mode pas_lock_hold_mode;

enum pas_lock_lock_mode {
    pas_lock_lock_mode_try_lock,
    pas_lock_lock_mode_lock,
};

typedef enum pas_lock_lock_mode pas_lock_lock_mode;

#define PAS_LOCK_VERBOSE 0

PAS_END_EXTERN_C;

#if PAS_USE_SPINLOCKS

PAS_BEGIN_EXTERN_C;

struct pas_lock;
typedef struct pas_lock pas_lock;

struct pas_lock {
    bool lock;
    bool is_spinning;
};

#define PAS_LOCK_INITIALIZER ((pas_lock){ .lock = false, .is_spinning = false })

static inline void pas_lock_construct(pas_lock* lock)
{
    *lock = PAS_LOCK_INITIALIZER;
}

static inline void pas_lock_construct_disabled(pas_lock* lock)
{
    *lock = PAS_LOCK_INITIALIZER;
    lock->lock = true; /* This isn't great; it'll just mean that if we use the lock wrong then
                          we'll infinite loop. But we test in a mode where we don't use this
                          code path. */
}

PAS_API PAS_NEVER_INLINE void pas_lock_lock_slow(pas_lock* lock);

static inline void pas_lock_lock(pas_lock* lock)
{
    pas_race_test_will_lock(lock);
    if (!pas_compare_and_swap_bool_weak(&lock->lock, false, true))
        pas_lock_lock_slow(lock);
    pas_race_test_did_lock(lock);
}

static inline bool pas_lock_try_lock(pas_lock* lock)
{
    bool result;
    result = !pas_compare_and_swap_bool_strong(&lock->lock, false, true);
    if (result)
        pas_race_test_did_try_lock(lock);
    return result;
}

static inline void pas_lock_unlock(pas_lock* lock)
{
    pas_race_test_will_unlock(lock);
    pas_atomic_store_bool((bool*)&lock->lock, false);
}

static inline void pas_lock_assert_held(pas_lock* lock)
{
    PAS_ASSERT(lock->lock);
}

static inline void pas_lock_testing_assert_held(pas_lock* lock)
{
    PAS_TESTING_ASSERT(lock->lock);
}

PAS_END_EXTERN_C;

#elif PAS_OS(DARWIN) /* !PAS_USE_SPINLOCKS */

#if defined(__has_include) && __has_include(<os/lock_private.h>) && (defined(LIBPAS) || defined(PAS_BMALLOC)) && (!defined(OS_UNFAIR_LOCK_INLINE) || OS_UNFAIR_LOCK_INLINE)
#define PAS_USE_ULOCK_SPI 1
#define PAS_USE_ULOCK_FLAGS_API 0
#ifndef OS_UNFAIR_LOCK_INLINE
#define OS_UNFAIR_LOCK_INLINE 1
#endif
#include <os/lock_private.h>

#else
#define PAS_USE_ULOCK_SPI 0
#include <os/lock.h>

#define OS_UNFAIR_LOCK_DATA_SYNCHRONIZATION 0x00010000
#define OS_UNFAIR_LOCK_ADAPTIVE_SPIN 0x00040000

#if (PAS_PLATFORM(MAC) && __MAC_OS_X_VERSION_MAX_ALLOWED >= 150000) \
    || (PAS_PLATFORM(MACCATALYST) && __IPHONE_OS_VERSION_MAX_ALLOWED >= 180000) \
    || (PAS_PLATFORM(IOS) && __IPHONE_OS_VERSION_MAX_ALLOWED >= 180000) \
    || (PAS_PLATFORM(APPLETV) && __TV_OS_VERSION_MAX_ALLOWED >= 180000) \
    || (PAS_PLATFORM(WATCHOS) && __WATCH_OS_VERSION_MAX_ALLOWED >= 110000) \
    || (PAS_PLATFORM(VISION) && __VISION_OS_VERSION_MAX_ALLOWED >= 20000)
/* After this version, OS_UNFAIR_LOCK_DATA_SYNCHRONIZATION etc. are public API */
#define PAS_USE_ULOCK_FLAGS_API 1
#else
#define PAS_USE_ULOCK_FLAGS_API 0
#endif

#define os_unfair_lock_lock_with_options_inline os_unfair_lock_lock_with_options
#define os_unfair_lock_trylock_inline os_unfair_lock_trylock
#define os_unfair_lock_unlock_inline os_unfair_lock_unlock
#endif

PAS_BEGIN_EXTERN_C;

struct pas_lock;
typedef struct pas_lock pas_lock;

struct pas_lock {
    os_unfair_lock lock;
};

#define PAS_LOCK_INITIALIZER ((pas_lock){ .lock = OS_UNFAIR_LOCK_INIT })

static inline void pas_lock_construct(pas_lock* lock)
{
    *lock = PAS_LOCK_INITIALIZER;
}

static inline void pas_lock_construct_disabled(pas_lock* lock)
{
    pas_zero_memory(lock, sizeof(pas_lock));
}

static inline void pas_lock_lock(pas_lock* lock)
{
    if (PAS_LOCK_VERBOSE)
        pas_log("Thread %p Locking lock %p\n", pthread_self(), lock);
    pas_race_test_will_lock(lock);
#if PAS_USE_ULOCK_SPI
    const os_unfair_lock_options_t options = (os_unfair_lock_options_t)(OS_UNFAIR_LOCK_DATA_SYNCHRONIZATION | OS_UNFAIR_LOCK_ADAPTIVE_SPIN);
    os_unfair_lock_lock_with_options_inline(&lock->lock, options);
#elif PAS_USE_ULOCK_FLAGS_API
    const os_unfair_lock_flags_t options = (os_unfair_lock_flags_t)(OS_UNFAIR_LOCK_DATA_SYNCHRONIZATION | OS_UNFAIR_LOCK_ADAPTIVE_SPIN);
    os_unfair_lock_lock_with_flags(&lock->lock, options);
#else
    os_unfair_lock_lock(&lock->lock);
#endif
    pas_race_test_did_lock(lock);
}

static inline bool pas_lock_try_lock(pas_lock* lock)
{
    bool result;
    if (PAS_LOCK_VERBOSE)
        pas_log("Thread %p Trylocking lock %p\n", pthread_self(), lock);
    result = os_unfair_lock_trylock_inline(&lock->lock);
    if (result)
        pas_race_test_did_try_lock(lock);
    return result;
}

static inline void pas_lock_unlock(pas_lock* lock)
{
    if (PAS_LOCK_VERBOSE)
        pas_log("Thread %p Unlocking lock %p\n", pthread_self(), lock);
    pas_race_test_will_unlock(lock);
    os_unfair_lock_unlock_inline(&lock->lock);
}

static inline void pas_lock_assert_held(pas_lock* lock)
{
    os_unfair_lock_assert_owner(&lock->lock);
}

static inline void pas_lock_testing_assert_held(pas_lock* lock)
{
    if (PAS_ENABLE_TESTING)
        os_unfair_lock_assert_owner(&lock->lock);
}

PAS_END_EXTERN_C;

#elif PAS_PLATFORM(PLAYSTATION) /* !PAS_USE_SPINLOCKS */

#include <errno.h>
#include <pthread_np.h>

PAS_BEGIN_EXTERN_C;

struct pas_lock;
typedef struct pas_lock pas_lock;

struct pas_lock {
    pthread_mutex_t mutex;
};

#define PAS_LOCK_INITIALIZER ((pas_lock){ .mutex = PTHREAD_MUTEX_INITIALIZER })

static inline void pas_lock_construct(pas_lock* lock)
{
    *lock = PAS_LOCK_INITIALIZER;
}

static inline void pas_lock_construct_disabled(pas_lock* lock)
{
    pthread_mutex_destroy(&lock->mutex);
    pas_zero_memory(lock, sizeof(pas_lock));
}

static inline void pas_lock_mutex_setname(pas_lock* lock)
{
    pthread_mutex_setname_np(&lock->mutex, "SceNKLibpas");
}

static inline void pas_lock_lock(pas_lock* lock)
{
    bool unnamed;
    unnamed = (lock->mutex == PTHREAD_MUTEX_INITIALIZER);
    pas_race_test_will_lock(lock);
    pthread_mutex_lock(&lock->mutex);
    pas_race_test_did_lock(lock);
    if (PAS_UNLIKELY(unnamed))
        pas_lock_mutex_setname(lock);
}

static inline bool pas_lock_try_lock(pas_lock* lock)
{
    int error;
    bool unnamed;
    unnamed = (lock->mutex == PTHREAD_MUTEX_INITIALIZER);
    error = pthread_mutex_trylock(&lock->mutex);
    PAS_ASSERT(!error || error == EBUSY);
    if (!error) {
        pas_race_test_did_try_lock(lock);
        if (PAS_UNLIKELY(unnamed))
            pas_lock_mutex_setname(lock);
    }
    return !error;
}

static inline void pas_lock_unlock(pas_lock* lock)
{
    pas_race_test_will_unlock(lock);
    pthread_mutex_unlock(&lock->mutex);
}

static inline bool pas_lock_test_held(pas_lock* lock)
{
    if (pthread_mutex_trylock(&lock->mutex))
        return true;
    pthread_mutex_unlock(&lock->mutex);
    return false;
}

static inline void pas_lock_assert_held(pas_lock* lock)
{
    PAS_ASSERT(pas_lock_test_held(lock));
}

static inline void pas_lock_testing_assert_held(pas_lock* lock)
{
    PAS_TESTING_ASSERT(pas_lock_test_held(lock));
}

PAS_END_EXTERN_C;


#elif PAS_OS(LINUX)  /* !PAS_USE_SPINLOCKS */

#include <sys/syscall.h>
#include <linux/futex.h>
#include <unistd.h>     // For syscall()

PAS_BEGIN_EXTERN_C;

/*
 *   0b00 => unlocked
 *   0b01 => locked (uncontended)
 *   0b11 => locked + contended
 */
enum {
    PAS_LOCK_UNLOCKED  = 0, /* 0b00 */
    PAS_LOCK_LOCKED    = 1, /* 0b01 */
    PAS_LOCK_CONTENDED = 3  /* 0b11 */
};

struct pas_lock {
    int state;
};

typedef struct pas_lock pas_lock;

#define PAS_LOCK_INITIALIZER ((pas_lock){ .state = PAS_LOCK_UNLOCKED })

static inline long pas_sys_futex(void* addr1, int op, int val1,
                                 struct timespec* timeout, void* addr2, int val3)
{
    return syscall(SYS_futex, addr1, op, val1, timeout, addr2, val3);
}

#if PAS_X86_64

/*
 * On x86_64, we can use inline assembly for "lock bts" to set bit 0 of [memory].
 * If the bit was already 1, CF=1 => the lock was held; if CF=0, we acquired it from 0.
 * 
 * Acquire semantics: we typically rely on the “locked” instruction plus an lfence or
 * a concurrency barrier. In practice, “lock” prefixed instructions on x86 enforce enough
 * ordering for lock acquisition. For additional caution, you might add an "mfence" or
 * "__atomic_thread_fence(__ATOMIC_ACQUIRE)" after bts if needed in your environment.
 */
static inline bool pas_try_lock_bts_x86(volatile int* state_ptr)
{
    bool was_locked; 
    /* was_locked gets CF=1 if bit 0 was already set => lock was not free. */
    __asm__ volatile(
        "lock btsl $0, (%1)\n\t"
        /* setc = "set carry if CF=1". So was_locked=1 if the bit was already set. */
        "setc %0\n\t"
        : "=r"(was_locked)
        : "r"(state_ptr)
        : "memory", "cc"
    );
    /* If was_locked==true => CF=1 => the bit was set => we failed to acquire. */
    return !was_locked;
}

#endif /* PAS_X86_64 */

/*
 * Fallback if not x86_64: compare-and-swap 0->1. 
 * You could also do something else for ARM, like an LSE-based approach, etc.
 */
static inline bool pas_try_lock_cas(volatile int* state_ptr)
{
    uint32_t oldv = pas_compare_and_swap_uint32_strong((uint32_t*)state_ptr, PAS_LOCK_UNLOCKED, PAS_LOCK_LOCKED);
    return (oldv == PAS_LOCK_UNLOCKED);
}

/* CPU pause or yield for spin loops. */
static PAS_ALWAYS_INLINE void pas_lock_cpu_pause(void)
{
#if PAS_ARM64
    __asm__ volatile("yield" ::: "memory");
#elif PAS_X86_64
    __asm__ volatile("pause" ::: "memory");
#else
    sched_yield();
#endif
}

/* Lock initialization. */
static inline void pas_lock_construct(pas_lock* lock)
{
    *lock = PAS_LOCK_INITIALIZER;
}

/* Disabled => set state to INT_MAX so it's never acquired. */
static inline void pas_lock_construct_disabled(pas_lock* lock)
{
    *lock = PAS_LOCK_INITIALIZER;
    lock->state = INT_MAX;
}

/*
 * Attempt a fast lock with inline assembly on x86_64,
 * otherwise fallback to CAS. 
 */
static inline bool pas_lock_try_lock_impl(pas_lock* lock)
{
#if PAS_X86_64
    return pas_try_lock_bts_x86((volatile int*)&lock->state);
#else
    return pas_try_lock_cas((volatile int*)&lock->state);
#endif
}

/*
 * Full lock routine:
 *   1) Quick attempt
 *   2) Spin for a short while if that fails.
 *   3) Mark contended (0b11), wait with futex until unlocked.
 */
static inline void pas_lock_lock(pas_lock* lock)
{
    static const size_t spin_count = 40;
    pas_race_test_will_lock(lock);

    /* --- Fast path: if lock was 0 => now 1. */
    if (pas_lock_try_lock_impl(lock)) {
        pas_race_test_did_lock(lock);
        return;
    }

    /* --- Spin a little, hoping it becomes unlocked. */
    for (size_t i = 0; i < spin_count; i++) {
        pas_lock_cpu_pause();
        /* Check if it’s unlocked => try again. */
        if (lock->state == PAS_LOCK_UNLOCKED) {
            if (pas_lock_try_lock_impl(lock)) {
                pas_race_test_did_lock(lock);
                return;
            }
        }
    }

    /* --- Slow path: unify behind "contended=3" & futex. */
    for (;;) {
        int s = __atomic_load_n(&lock->state, __ATOMIC_RELAXED);

        if (s == PAS_LOCK_UNLOCKED) {
            /* Try to lock from 0->1 again. */
            if (pas_lock_try_lock_impl(lock)) {
                pas_race_test_did_lock(lock);
                return;
            }
        } else {
            /* Mark contended if it's only locked=0b01. 
             * If it's already contended=0b11, that's fine; we can skip.
             */
            if (s == PAS_LOCK_LOCKED) {
                pas_compare_and_swap_uint32_strong((uint32_t*)&lock->state, PAS_LOCK_LOCKED, PAS_LOCK_CONTENDED);
            }

            /* If still locked or contended => wait on futex. */
            if (lock->state != PAS_LOCK_UNLOCKED)
                pas_sys_futex(&lock->state, FUTEX_WAIT_PRIVATE, PAS_LOCK_CONTENDED, NULL, NULL, 0);
        }

        /* Then loop around in case of spurious wake-up or CAS race. */
    }
}

/*
 * Non-blocking try-lock entry point.
 * Returns true if we acquired from unlocked => locked.
 */
static inline bool pas_lock_try_lock(pas_lock* lock)
{
    bool result = pas_lock_try_lock_impl(lock);
    if (result)
        pas_race_test_did_try_lock(lock);
    return result;
}

/*
 * Unlock: set state=0, do a FUTEX_WAKE if it was contended (0b11).
 * On x86, "store .release" can typically be done by just writing,
 * but we want a memory barrier if needed. We can also do a small CAS loop.
 */
static inline void pas_lock_unlock(pas_lock* lock)
{
    pas_race_test_will_unlock(lock);

    int old_state = __atomic_exchange_n(&lock->state, PAS_LOCK_UNLOCKED, __ATOMIC_RELEASE);
    if (old_state == PAS_LOCK_CONTENDED)
        pas_sys_futex(&lock->state, FUTEX_WAKE_PRIVATE, INT_MAX, NULL, NULL, 0);
}

static inline void pas_lock_assert_held(pas_lock* lock)
{
    PAS_ASSERT(lock->state != PAS_LOCK_UNLOCKED);
}

static inline void pas_lock_testing_assert_held(pas_lock* lock)
{
    PAS_TESTING_ASSERT(lock->state != PAS_LOCK_UNLOCKED);
}

PAS_END_EXTERN_C;

#else /* !PAS_USE_SPINLOCKS */
#error "No pas_lock implementation found"
#endif /* !PAS_USE_SPINLOCKS */

PAS_BEGIN_EXTERN_C;

#define PAS_DECLARE_LOCK(name) \
    PAS_API extern pas_lock name##_lock; \
    \
    PAS_UNUSED static inline void name##_lock_lock(void) \
    { \
        pas_lock_lock(&name##_lock); \
    } \
    \
    PAS_UNUSED static inline bool name##_lock_try_lock(void) \
    { \
        return pas_lock_try_lock(&name##_lock); \
    } \
    \
    PAS_UNUSED static inline bool name##_lock_lock_with_mode(pas_lock_lock_mode mode) \
    { \
        return pas_lock_lock_with_mode(&name##_lock, mode); \
    } \
    \
    PAS_UNUSED static inline void name##_lock_unlock(void) \
    { \
        pas_lock_unlock(&name##_lock); \
    } \
    \
    PAS_UNUSED static inline void name##_lock_assert_held(void) \
    { \
        pas_lock_assert_held(&name##_lock); \
    } \
    \
    PAS_UNUSED static inline void name##_lock_testing_assert_held(void) \
    { \
        pas_lock_testing_assert_held(&name##_lock); \
    } \
    \
    PAS_UNUSED static inline void name##_lock_lock_conditionally(pas_lock_hold_mode hold_mode) \
    { \
        if (hold_mode == pas_lock_is_not_held) \
            name##_lock_lock(); \
        pas_compiler_fence(); \
    } \
    \
    PAS_UNUSED static inline bool name##_lock_try_lock_conditionally(pas_lock_hold_mode hold_mode) \
    { \
        if (hold_mode == pas_lock_is_not_held) { \
            if (!name##_lock_try_lock()) \
                return false; \
        } \
        pas_compiler_fence(); \
        return true; \
    } \
    \
    PAS_UNUSED static inline void name##_lock_lock_dependently(unsigned token) \
    { \
        pas_lock_lock(&name##_lock + token); \
    } \
    \
    PAS_UNUSED static inline void name##_lock_unlock_conditionally(pas_lock_hold_mode hold_mode) \
    { \
        pas_compiler_fence(); \
        if (hold_mode == pas_lock_is_not_held) \
            name##_lock_unlock(); \
    } \
    struct pas_dummy

#define PAS_DEFINE_LOCK(name) \
    pas_lock name##_lock = PAS_LOCK_INITIALIZER; \
    struct pas_dummy

static inline bool pas_lock_lock_with_mode(pas_lock* lock,
                                           pas_lock_lock_mode mode)
{
    switch (mode) {
    case pas_lock_lock_mode_try_lock:
        return pas_lock_try_lock(lock);
    case pas_lock_lock_mode_lock:
        pas_lock_lock(lock);
        return true;
    }
    PAS_ASSERT(!"Should not be reached");
    return false;
}

static PAS_ALWAYS_INLINE bool pas_lock_switch_with_mode(pas_lock** held_lock,
                                                        pas_lock* lock,
                                                        pas_lock_lock_mode mode)
{
    bool result;
    pas_compiler_fence();
    if (PAS_LIKELY(lock == *held_lock))
        return true;
    if (*held_lock)
        pas_lock_unlock(*held_lock);
    if (lock)
        result = pas_lock_lock_with_mode(lock, mode);
    else
        result = true;
    if (result)
        *held_lock = lock;
    else
        *held_lock = NULL;
    pas_compiler_fence();
    return result;
}

static PAS_ALWAYS_INLINE void pas_lock_switch(pas_lock** held_lock, pas_lock* lock)
{
    bool result;
    result = pas_lock_switch_with_mode(held_lock, lock, pas_lock_lock_mode_lock);
    PAS_ASSERT(result);
}

static inline void pas_lock_lock_conditionally(pas_lock* lock,
                                               pas_lock_hold_mode hold_mode)
{
    if (hold_mode == pas_lock_is_not_held)
        pas_lock_lock(lock);
    pas_compiler_fence();
}

static inline void pas_lock_unlock_conditionally(pas_lock* lock,
                                                 pas_lock_hold_mode hold_mode)
{
    pas_compiler_fence();
    if (hold_mode == pas_lock_is_not_held)
        pas_lock_unlock(lock);
}

static PAS_ALWAYS_INLINE pas_lock* pas_lock_for_switch_conditionally(pas_lock* lock,
                                                                     pas_lock_hold_mode hold_mode)
{
    switch (hold_mode) {
    case pas_lock_is_held:
        return NULL;
    case pas_lock_is_not_held:
        return lock;
    }
    PAS_ASSERT(!"Should not be reached");
    return NULL;
}

static PAS_ALWAYS_INLINE void pas_lock_switch_conditionally(pas_lock** held_lock,
                                                            pas_lock* lock,
                                                            pas_lock_hold_mode hold_mode)
{
    pas_lock_switch(held_lock, pas_lock_for_switch_conditionally(lock, hold_mode));
}

PAS_END_EXTERN_C;

#endif /* PAS_LOCK_H */


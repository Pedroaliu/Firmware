#pragma once

#include <stdint.h>

namespace jixia::microkernel {

class Spinlock final {
  public:
    constexpr Spinlock() : value_(0U) {
    }

    Spinlock(const Spinlock&) = delete;
    Spinlock& operator=(const Spinlock&) = delete;

    void lock() {
        while (__atomic_exchange_n(&value_, 1U, __ATOMIC_ACQUIRE) != 0U) {
            while (__atomic_load_n(&value_, __ATOMIC_RELAXED) != 0U) {
                __asm__ volatile("nop");
            }
        }
    }

    void unlock() {
        __atomic_store_n(&value_, 0U, __ATOMIC_RELEASE);
    }

  private:
    uint32_t value_;
};

class SpinlockGuard final {
  public:
    explicit SpinlockGuard(Spinlock& lock) : lock_(lock) {
        lock_.lock();
    }

    ~SpinlockGuard() {
        lock_.unlock();
    }

    SpinlockGuard(const SpinlockGuard&) = delete;
    SpinlockGuard& operator=(const SpinlockGuard&) = delete;

  private:
    Spinlock& lock_;
};

} // namespace jixia::microkernel

#pragma once

#include <atomic>
#include <thread>

class SpinLock final {
  public:
    SpinLock() = default;

    void lock() {
      while(flg.test_and_set(std::memory_order_acquire))
        std::this_thread::yield();
      }

    void unlock() {
      flg.clear(std::memory_order_release);
      }

  private:
    std::atomic_flag flg = ATOMIC_FLAG_INIT;
  };

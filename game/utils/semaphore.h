#pragma once

#include <condition_variable>
#include <mutex>

class Semaphore final {
  public:
    Semaphore()=default;
    Semaphore(uint32_t c) : count{c} {}

    void release(uint32_t n=1) {
      {
        std::unique_lock<std::mutex> lck(mtx);
        count+=n;
      }
      cv.notify_one();
      }

    void acquire(uint32_t n=1) {
      std::unique_lock<std::mutex> lck(mtx);
      while(count<n)
        cv.wait(lck);
      count-=n;
      }

  private:
    std::mutex              mtx;
    std::condition_variable cv;
    uint32_t                count=0;
  };

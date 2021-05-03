#pragma once

#include <thread>
#include <mutex>
#include <vector>
#include <functional>
#include <atomic>
#include <algorithm>

#include "semaphore.h"

class Workers final {
  public:
    Workers();
    ~Workers();

    template<class T,class F>
    static void parallelFor(T* b, T* e, const F& func) {
      inst().runParallelFor(b,std::distance(b,e),std::thread::hardware_concurrency(),func);
      }

    template<class T,class F>
    static void parallelFor(std::vector<T>& data, const F& func) {
      inst().runParallelFor(data.data(),data.size(),std::thread::hardware_concurrency(),func);
      }

    template<class T,class F>
    static void parallelFor(std::vector<T>& data, size_t maxTh, const F& func) {
      inst().runParallelFor(data.data(),data.size(),maxTh,func);
      }

  private:
    enum { MAX_THREADS=16 };

    void threadFunc(size_t id);
    void execWork();
    static Workers& inst();

    template<class T,class F>
    void runParallelFor(T* data, size_t sz, size_t maxTh, const F& func) {
      workSet     = reinterpret_cast<uint8_t*>(data);
      workSize    = sz;
      workEltSize = sizeof(T);

      workFunc = [&func](void* data,size_t sz) {
        T* tdata = reinterpret_cast<T*>(data);
        for(size_t i=0;i<sz;++i)
          func(tdata[i]);
        };

      if(maxTh>MAX_THREADS)
        workTasks = MAX_THREADS; else
        workTasks = maxTh;

      batchSize = std::max<size_t>(16,(sz+workTasks-1)/workTasks);
      execWork();
      }

    std::thread                       th     [MAX_THREADS];
    bool                              workInc[MAX_THREADS];

    bool                              running=true;

    uint8_t*                          workSet=nullptr;
    size_t                            workSize=0, batchSize=0, workEltSize=0;
    size_t                            workTasks=0;
    std::function<void(void*,size_t)> workFunc;

    std::mutex                        sync;
    std::condition_variable           workWait;
    std::atomic_int                   workDone{0};
  };

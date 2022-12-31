#pragma once

#include <thread>
#include <mutex>
#include <vector>
#include <functional>
#include <atomic>
#include <algorithm>
#include <condition_variable>
#include <new>

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

    template<class T,class F>
    static void parallelTasks(std::vector<T>& data, const F& func) {
      inst().runParallelFor2(data.data(),data.size(),std::thread::hardware_concurrency(),func);
      }

    template<class F>
    static void parallelTasks(size_t taskCount, const F& func) {
      inst().runParallelTasks<F>(taskCount,func);
      }

    static uint8_t maxThreads() {
      uint32_t th = std::thread::hardware_concurrency();
      if(th>MAX_THREADS)
        return MAX_THREADS;
      return uint8_t(th);
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

    template<class T,class F>
    void runParallelFor2(T* data, size_t sz, size_t maxTh, const F& func) {
      workTasks   = std::min<size_t>(MAX_THREADS, sz);
      workTasks   = std::min<size_t>(workTasks,  maxTh);
      workSize    = workTasks;
      batchSize   = 1;
      workEltSize = 0;

      workSet     = reinterpret_cast<uint8_t*>(data);
      taskDone.store(0);
      workFunc = [this, &func, sz](void* data, size_t /*sz*/) {
        T* tdata = reinterpret_cast<T*>(data);
        const size_t increment = (64+sizeof(T)-1)/sizeof(T);
        while(true) {
          size_t id = size_t(taskDone.fetch_add(increment));
          for(size_t i=0;i<increment;++i) {
            if(id+i<sz)
              func(tdata[id+i]);
            }

          if(id+increment>=sz)
            break;
          }
        };
      execWork();
      }

    template<class F>
    void runParallelTasks(size_t taskCount, const F& func) {
      workTasks   = taskCount;
      workSize    = taskCount;
      batchSize   = 1;
      workSet     = nullptr;
      workEltSize = 1;
      workFunc = [&func](void* data, size_t sz) {
        func(reinterpret_cast<uintptr_t>(data));
        };
      execWork();
      }

    std::thread                       th     [MAX_THREADS];
    bool                              workInc[MAX_THREADS] = {};

    bool                              running=true;

    uint8_t*                          workSet=nullptr;
    size_t                            workSize=0, batchSize=0, workEltSize=0;
    size_t                            workTasks=0;
    std::function<void(void*,size_t)> workFunc;

    std::mutex                        sync;
    std::condition_variable           workWait;
    std::atomic_int                   workDone{0};
    std::atomic_int                   taskDone{0};
  };

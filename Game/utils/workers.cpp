#include "workers.h"

Workers::Workers() {
  size_t id=0;
  for(auto& i:th) {
    i = std::thread([this,id]() noexcept {
      threadFunc(id);
      });
    ++id;
    }
  }

Workers::~Workers() {
  std::lock_guard<std::mutex> guard(sync);(void)guard;
  running=false;
  for(size_t i=0;i<MAX_THREADS;++i)
    workInc[i].release(1);
  for(auto& i:th)
    i.join();
  }

Workers &Workers::inst() {
  static Workers w;
  return w;
  }

void Workers::threadFunc(size_t id) {
  while(true) {
    workInc[id].acquire(1);
    if(!running) {
      workDone.release(1);
      return;
      }

    size_t b = ((id  )*workSize)/workTasks;
    size_t e = ((id+1)*workSize)/workTasks;

    void* d = &workSet[b*workEltSize];
    if(b!=e)
      workFunc(d,e-b);

    workDone.release(1);
    }
  }

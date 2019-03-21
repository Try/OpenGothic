#include "workers.h"

Workers::Workers() {
  size_t id=0;
  for(auto& i:th) {
    i = std::thread([this,id](){
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

    for(size_t i=b;i<e;++i){
      void* d = &workSet[i*workEltSize];
      workFunc(d);
      }

    workDone.release(1);
    }
  }

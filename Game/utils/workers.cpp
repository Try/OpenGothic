#include "workers.h"

#include <Tempest/Log>

using namespace Tempest;

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
  running   = false;
  workTasks = MAX_THREADS;
  execWork();
  for(auto& i:th)
    i.join();
  }

Workers &Workers::inst() {
  static Workers w;
  return w;
  }

void Workers::threadFunc(size_t id) {
  while(true) {
    {
    std::unique_lock<std::mutex> lck(sync);
    while(!workInc[id])
      workWait.wait(lck);
    workInc[id]=false;
    }

    if(!running) {
      workDone.fetch_add(1);
      return;
      }

    size_t b = std::min((id  )*batchSize, workSize);
    size_t e = std::min((id+1)*batchSize, workSize);
    // Log::d("worker: id = ",id," [",b, ", ",e,"]");

    if(b!=e) {
      void* d = &workSet[b*workEltSize];
      workFunc(d,e-b);
      }

    workDone.fetch_add(1);
    }
  }

void Workers::execWork() {
  {
    std::unique_lock<std::mutex> lck(sync);
    for(size_t i=0; i<workTasks; ++i)
      workInc[i]=true;
    workWait.notify_all();
  }

  while(true) {
    int expect = int(workTasks);
    if(workDone.compare_exchange_strong(expect,0))
      break;
    std::this_thread::yield();
    }
  }

#include "workers.h"
#include "utils/string_frm.h"

#include <Tempest/Log>

#if defined(_MSC_VER)
#include <windows.h>

void Workers::setThreadName(const char* threadName) {
  const DWORD MS_VC_EXCEPTION = 0x406D1388;
  DWORD dwThreadID = GetCurrentThreadId();
#pragma pack(push,8)
  struct THREADNAME_INFO {
    DWORD  dwType = 0x1000; // Must be 0x1000.
    LPCSTR szName;          // Pointer to name (in user addr space).
    DWORD  dwThreadID;      // Thread ID (-1=caller thread).
    DWORD  dwFlags;         // Reserved for future use, must be zero.
    };
#pragma pack(pop)

  THREADNAME_INFO info = {};
  info.szName     = threadName;
  info.dwThreadID = dwThreadID;
  info.dwFlags    = 0;

  __try {
    RaiseException(MS_VC_EXCEPTION, 0, sizeof(info)/sizeof(ULONG_PTR), (ULONG_PTR*)&info );
    }
  __except(EXCEPTION_EXECUTE_HANDLER) {
    }
  }
#elif defined(__GNUC__) && !defined(__clang__)
void Workers::setThreadName(const char* threadName){
  pthread_setname_np(pthread_self(), threadName);
  }
#else
void Workers::setThreadName(const char* threadName) { (void)threadName; }
#endif

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
  {
  string_frm tname("Workers [",int(id),"]");
  setThreadName(tname.c_str());
  }

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
      void* d = workSet + b*workEltSize;
      workFunc(d,e-b);
      }

    if(size_t(workDone.fetch_add(1)+1)==workTasks)
      std::this_thread::yield();
    }
  }

void Workers::execWork() {
  {
    std::unique_lock<std::mutex> lck(sync);
    for(size_t i=0; i<workTasks; ++i)
      workInc[i]=true;
    workWait.notify_all();
  }
  std::this_thread::yield();

  while(true) {
    int expect = int(workTasks);
    if(workDone.compare_exchange_strong(expect,0,std::memory_order::acq_rel))
      break;
    std::this_thread::yield();
    }
  }

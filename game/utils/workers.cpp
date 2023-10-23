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

const size_t Workers::taskPerThread = 128;
const size_t Workers::taskPerStep   = 16;

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
  running  = false;
  workSet  = nullptr;
  workSize = MAX_THREADS;
  execWork(minWorkSize<void,void>());
  for(auto& i:th)
    i.join();
  }

Workers &Workers::inst() {
  static Workers w;
  return w;
  }

uint8_t Workers::maxThreads() {
  int32_t th = int32_t(std::thread::hardware_concurrency())-1;
  if(th<=0)
    th = 1;
  if(th>MAX_THREADS)
    return MAX_THREADS;
  return uint8_t(th);
  }

void Workers::threadFunc(size_t id) {
  {
  string_frm tname("Workers [",int(id),"]");
  setThreadName(tname.c_str());
  }

  while(true) {
    {
    std::unique_lock<std::mutex> lck(sync);
    workWait.wait(lck, [this]() { return workTbd>0; });
    --workTbd;
    }

    if(!running) {
      taskDone.fetch_add(1);
      return;
      }

    if(workSet==nullptr) {
      auto idx = progressIt.fetch_add(1);
      workFunc(workSet+idx, 1);
      } else {
      taskLoop();
      }

    taskDone.fetch_add(1);
    // if(size_t(taskDone.fetch_add(1)+1)==taskCount)
    //   std::this_thread::yield();
    }
  }

uint32_t Workers::taskLoop() {
  uint32_t count = 0;
  while(true) {
    size_t b = size_t(progressIt.fetch_add(taskPerStep));
    size_t e = std::min(b+taskPerStep, workSize);
    if(e<=b)
      break;

    void* d = workSet + b*workEltSize;
    workFunc(d,e-b);
    count += uint32_t(e-b);
    }
  return count;
  }

void Workers::execWork(uint32_t& minElts) {
  if(workSize==0)
    return;

  if(workSet!=nullptr) {
    const auto maxTheads = maxThreads();
    taskCount = uint32_t((workSize+taskPerThread-1)/taskPerThread);
    if(taskCount>maxTheads)
      taskCount = maxTheads;
    //taskCount = 1;
    } else {
    taskCount = uint32_t(workSize);
    }

  if(running && taskCount==1) {
    workFunc(workSet, workSize);
    return;
    }

  minElts = std::max<uint32_t>(minElts, taskPerThread);

  if(workSet!=nullptr && workSize<=minElts && true) {
    workFunc(workSet, workSize);
    if(minElts > workSize*2)
      minElts = 0;
    return;
    }

  progressIt.store(0);
  taskDone.store(0);

  {
  std::unique_lock<std::mutex> lck(sync);
  workTbd = int32_t(taskCount);
  }
  //std::this_thread::yield();
  workWait.notify_all();

  uint32_t cnt = 0;
  if(workSet==nullptr) {
    std::this_thread::yield();
    } else {
    // std::this_thread::yield();
    cnt = taskLoop(); (void)cnt;
    }

  while(true) {
    int expect = int(taskCount);
    if(taskDone.load()==expect) {
      taskDone.store(0);
      break;
      }
    std::this_thread::yield();
    }
  }

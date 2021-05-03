#include "crashlog.h"

#include <Tempest/Platform>

#include <iostream>
#include <cstring>
#include <csignal>
#include <fstream>
#include <cstring>
#ifdef __LINUX__
#include <execinfo.h> // backtrace
#include <dlfcn.h>    // dladdr
#include <cxxabi.h>   // __cxa_demangle
#endif

#include <dbg/frames.hpp>
#include <dbg/symbols.hpp>

static char gpuName[64]="?";

#ifdef __WINDOWS__
#include <windows.h>

static LONG WINAPI exceptionHandler(PEXCEPTION_POINTERS) {
  SetUnhandledExceptionFilter(nullptr);
  CrashLog::dumpStack("ExceptionFilter");
  return EXCEPTION_EXECUTE_HANDLER;
  }
#endif

static void signalHandler(int sig) {
  std::signal(sig, SIG_DFL);
  const char* sname = nullptr;
  char buf[16]={};
  if(sig==SIGSEGV)
    sname = "SIGSEGV";
  else if(sig==SIGABRT)
    sname = "SIGABRT";
  else if(sig==SIGFPE)
    sname = "SIGFPE";
  else if(sig==SIGABRT)
    sname = "SIGABRT";
  else {
    std::snprintf(buf,16,"%d",sig);
    sname = buf;
    }

  CrashLog::dumpStack(sname);
  std::raise(sig);
  }

[[noreturn]]
static void terminateHandler() {
  char msg[128] = "std::terminate";
  std::exception_ptr p = std::current_exception();
  if(p) {
    try {
      std::rethrow_exception(p);
      }
    catch (std::system_error& e) {
      std::snprintf(msg,sizeof(msg),"std::system_error(%s)",e.what());
      }
    catch (std::runtime_error& e) {
      std::snprintf(msg,sizeof(msg),"std::runtime_error(%s)",e.what());
      }
    catch (std::logic_error& e) {
      std::snprintf(msg,sizeof(msg),"std::logic_error(%s)",e.what());
      }
    catch (std::bad_alloc& e) {
      std::snprintf(msg,sizeof(msg),"std::bad_alloc(%s)",e.what());
      }
    catch (...) {
      }
    }
  CrashLog::dumpStack(msg);
  std::signal(SIGABRT, SIG_DFL); // avoid recursion
  std::abort();
  }

void CrashLog::setup() {
#ifdef __WINDOWS__
  SetUnhandledExceptionFilter(exceptionHandler);
#endif

  std::signal(SIGSEGV, &signalHandler);
  std::signal(SIGABRT, &signalHandler);
  std::signal(SIGFPE,  &signalHandler);
  std::signal(SIGABRT, &signalHandler);
  std::set_terminate(terminateHandler);
  }

void CrashLog::setGpu(const char *name) {
  std::strncpy(gpuName,name,sizeof(gpuName)-1);
  }

void CrashLog::dumpStack(const char *sig) {
#ifdef __WINDOWS__
  dbg::symdb          db;
  dbg::call_stack<64> traceback;
#endif
  if(sig==nullptr)
    sig = "SIGSEGV";

  std::cout.setf(std::ios::unitbuf);
  std::cout << std::endl << "---crashlog(" <<  sig   << ")---" << std::endl;
  writeSysInfo(std::cout);
#ifdef __WINDOWS__
  traceback.collect(0);
  traceback.log(db, std::cout);
#elif __LINUX__
  tracebackLinux(std::cout);
#endif
  std::cout << std::endl;

  std::ofstream fout("crash.log");
  fout.setf(std::ios::unitbuf);
  fout << "---crashlog(" <<  sig << ")---" << std::endl;
  writeSysInfo(fout);
#ifdef __WINDOWS__
  traceback.log(db, fout);
#elif __LINUX__
  tracebackLinux(fout);
#endif
  }

void CrashLog::tracebackLinux(std::ostream &out) {
  #ifdef __LINUX__
  // inspired by https://gist.github.com/fmela/591333/36faca4c2f68f7483cd0d3a357e8a8dd5f807edf (BSD)
  void *callstack[64] = {};
  char **symbols = nullptr;
  int framesNum = 0;
  framesNum = backtrace(callstack, 64);
  symbols = backtrace_symbols(callstack, framesNum);
  if(symbols != nullptr) {
    int skip = 4; // skip the signal handler frames
    bool loop = true;
    Dl_info info;
    const char* frame;
    for(int i = skip; i < framesNum && loop; i++) {
      if(dladdr(callstack[i], &info) && info.dli_sname) {
        int status = -1;
        if(info.dli_sname[0] == '_')
          frame = abi::__cxa_demangle(info.dli_sname, NULL, 0, &status);
        frame = status == 0 ? frame : info.dli_sname == 0 ? symbols[i] : info.dli_sname;
        }
      if(!strcmp("main", frame)) {
        // looping beyond the main() causes crashes
        loop = false;
        }
      if(strcmp(frame, symbols[i]))
        out << "#" << i-skip+1 << ": " << frame << " - " << symbols[i] << std::endl;
      else
        out << "#" << i-skip+1 << ": " << frame << std::endl;
      }
    free(symbols);
    }
  #endif
  }

void CrashLog::writeSysInfo(std::ostream &fout) {
  fout << "GPU: " << gpuName << std::endl;
  }

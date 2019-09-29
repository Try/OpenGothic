#include "crashlog.h"

#include <Tempest/Platform>

#include <iostream>
#include <cstring>
#include <csignal>
#include <fstream>
#include <cstring>

#include <dbg/frames.hpp>
#include <dbg/symbols.hpp>

static char gpuName[64]={};

#ifdef __WINDOWS__
#include <errhandlingapi.h>

static LONG WINAPI exceptionHandler(PEXCEPTION_POINTERS) {
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
  CrashLog::dumpStack("std::terminate");
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

  if(sig==nullptr)
    sig = "SIGSEGV";

  std::cout << std::endl << "---crashlog(" <<  sig   << ")---" << std::endl;
  writeSysInfo(std::cout);
  traceback.collect(0);
  traceback.log(db, std::cout);
  std::cout << std::endl;

  std::ofstream fout("crash.log");
  fout << "---crashlog(" <<  sig << ")---" << std::endl;
  writeSysInfo(fout);
  traceback.log(db, fout);
#endif
  }

void CrashLog::writeSysInfo(std::ostream &fout) {
  fout << "GPU: " << gpuName << std::endl;
  }

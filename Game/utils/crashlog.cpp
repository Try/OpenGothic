#include "crashlog.h"

#include <Tempest/Platform>

#include <iostream>
#include <cstring>
#include <csignal>
#include <fstream>

#include <dbg/frames.hpp>
#include <dbg/symbols.hpp>

#ifdef __WINDOWS__
#include <errhandlingapi.h>

static LONG WINAPI exceptionHandler(PEXCEPTION_POINTERS) {
  CrashLog::dumpStack("ExceptionFilter");
  return EXCEPTION_EXECUTE_HANDLER;
  }
#endif

static void signalHandler(int signum) {
  std::signal(signum, SIG_DFL);
  CrashLog::dumpStack(signum);
  std::raise(signum);
  }

[[noreturn]]
static void terminateHandler() {
  CrashLog::dumpStack("std::terminate");
  std::abort();
  }

void CrashLog::setup() {
#ifdef __WINDOWS__
  SetUnhandledExceptionFilter(exceptionHandler);
#else
  std::signal(SIGSEGV, &signalHandler);
  std::signal(SIGABRT, &signalHandler);
  std::signal(SIGFPE,  &signalHandler);
  std::signal(SIGABRT, &signalHandler);
#endif
  std::set_terminate(terminateHandler);
  }

void CrashLog::dumpStack(int sig) {
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

  dumpStack(sname);
  }

void CrashLog::dumpStack(const char *sig) {
  dbg::symdb          db;
  dbg::call_stack<64> traceback;

  if(sig==nullptr)
    sig = "SIGSEGV";

  std::cout << std::endl << "---crashlog(" <<  sig   << ")---" << std::endl;
  traceback.collect(0);
  traceback.log(db, std::cout);
  std::cout << std::endl;

  std::ofstream fout("crash.log");
  fout << "---crashlog(" <<  sig << ")---" << std::endl;
  traceback.log(db, fout);
  }

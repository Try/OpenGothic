#pragma once

#include <cstdint>
#include <ostream>

class CrashLog final {
  public:
    static void setup();
    static void setGpu(const char* name);

    static void dumpStack(const char* sig);

  private:
    static void writeSysInfo(std::ostream& fout);
  };


#pragma once

#include <cstdint>

class CrashLog final {
  public:
    static void setup();

    static void dumpStack(const char* sig);
  };


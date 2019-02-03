#pragma once

#include <cstdint>

class gtime final {
  public:
    gtime()=default;
    gtime(int32_t hour,int32_t min):time(((hour+min*60)*60)*1000){}

    int64_t toInt() const { return time; }

  private:
    int64_t time=0;
  };

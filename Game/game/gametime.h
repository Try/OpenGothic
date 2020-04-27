#pragma once

#include <cstdint>
#include <limits>
#include <time.h>

class gtime final {
  public:
    gtime()=default;
    gtime(int32_t hour,int32_t min):time(hour*hourMilis+min*minMilis){}
    gtime(int64_t day,int32_t hour,int32_t min):time(day*dayMilis+hour*hourMilis+min*minMilis){}
    gtime(int64_t day,int64_t hour,int64_t min):time(day*dayMilis+hour*hourMilis+min*minMilis){}

    int64_t toInt() const { return time; }
    void    addMilis(uint64_t t){ time+=t; }

    int64_t day()       const { return time/dayMilis; }
    gtime   timeInDay() const { return gtime(time%dayMilis); }

    int64_t hour()      const { return (time/hourMilis)%24; }
    int64_t minute()    const { return (time/minMilis )%60; }

    static const gtime endOfTime() { return gtime(std::numeric_limits<int64_t>::max()); }
    static gtime getLocaltime() {
      time_t now = ::time(nullptr);
      tm* tp = ::localtime(&now);
      return gtime(tp->tm_hour, tp->tm_min);
      }

  private:
    explicit gtime(int64_t milis):time(milis){}

    int64_t time=0;
    static const int64_t dayMilis =24*60*60*1000;
    static const int64_t hourMilis=60*60*1000;
    static const int64_t minMilis =60*1000;

  friend bool operator == (gtime a,gtime b);
  friend bool operator != (gtime a,gtime b);
  friend bool operator <  (gtime a,gtime b);
  friend bool operator <= (gtime a,gtime b);
  };

inline bool operator == (gtime a,gtime b){
  return a.time==b.time;
  }

inline bool operator != (gtime a,gtime b){
  return a.time!=b.time;
  }

inline bool operator < (gtime a,gtime b){
  return a.time<b.time;
  }

inline bool operator <=(gtime a,gtime b){
  return a.time<=b.time;
  }

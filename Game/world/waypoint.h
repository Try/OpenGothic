#pragma once

#include <zenload/zTypes.h>

class FpLock;

class WayPoint final {
  public:
    WayPoint();
    WayPoint(const ZenLoad::zCWaypointData& dat);
    WayPoint(float x,float y,float z,const char* name);
    WayPoint(const WayPoint&)=default;
    WayPoint(WayPoint&&)=default;

    WayPoint& operator = (WayPoint&&)=default;
    bool isLocked() const { return useCount!=0; }

    float x=0;
    float y=0;
    float z=0;

    float dirX=0;
    float dirY=0;
    float dirZ=0;

    std::string name;

    float qDistTo(float x,float y,float z) const;

    void connect(WayPoint* w);

  private:
    mutable uint32_t useCount=0;

    std::vector<WayPoint*> conn;

    static std::string upcaseof(const std::string& src);

  friend class FpLock;
  };

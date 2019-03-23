#pragma once

#include <zenload/zTypes.h>
#include <limits>

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
    bool isFreePoint() const { return conn.size()==0; }

    float x=0;
    float y=0;
    float z=0;

    float dirX=0;
    float dirY=0;
    float dirZ=0;

    std::string name;

    // TODO: beautify
    mutable float   pathLen=std::numeric_limits<float>::max();
    mutable uint8_t pathGen=0;

    float qDistTo(float x,float y,float z) const;

    void connect(WayPoint& w);
    const std::vector<WayPoint*>& connections() const { return conn; }

  private:
    mutable uint32_t useCount=0;

    std::vector<WayPoint*> conn;

    static std::string upcaseof(const std::string& src);

  friend class FpLock;
  };

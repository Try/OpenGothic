#pragma once

#include <zenload/zTypes.h>
#include <limits>

class FpLock;

class WayPoint final {
  public:
    WayPoint();
    WayPoint(const ZenLoad::zCWaypointData& dat);
    WayPoint(float x,float y,float z,const char* name);
    WayPoint(float x,float y,float z,float dx,float dy,float dz,const char* name);
    WayPoint(const WayPoint&)=default;
    WayPoint(WayPoint&&)=default;

    WayPoint& operator = (WayPoint&&)=default;
    bool isLocked() const { return useCount!=0; }
    bool isFreePoint() const;

    bool checkName(const std::string& name) const;
    bool checkName(const char* name) const;

    float x=0;
    float y=0;
    float z=0;

    float dirX=0;
    float dirY=0;
    float dirZ=0;

    Daedalus::ZString name;

    struct Conn final {
      WayPoint* point=nullptr;
      int32_t   len  =0;
      };

    // TODO: beautify
    mutable int32_t  pathLen=std::numeric_limits<int32_t>::max();
    mutable uint16_t pathGen=0;

    float qDistTo(float x,float y,float z) const;

    void connect(WayPoint& w);
    const std::vector<Conn>& connections() const { return conn; }

  private:
    mutable uint32_t useCount=0;

    std::vector<Conn> conn;

    static std::string upcaseof(const std::string& src);

  friend class FpLock;
  };

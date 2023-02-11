#pragma once

#include <phoenix/world/way_net.hh>

#include <limits>
#include <Tempest/Vec>

class FpLock;

class WayPoint final {
  public:
    WayPoint();
    WayPoint(const phoenix::way_point& dat);
    WayPoint(const Tempest::Vec3& pos, std::string_view name);
    WayPoint(const Tempest::Vec3& pos, const Tempest::Vec3& dir, std::string_view name);
    WayPoint(const WayPoint&)=default;
    WayPoint(WayPoint&&)=default;

    WayPoint& operator = (WayPoint&&)=default;
    bool isLocked() const { return useCount!=0; }
    bool isFreePoint() const;

    uint32_t useCounter() const { return useCount; }
    bool checkName(std::string_view name) const;

    Tempest::Vec3 position() const;

    float x=0;
    float y=0;
    float z=0;

    float dirX=0;
    float dirY=0;
    float dirZ=0;

    bool  underWater = false;

    std::string name;

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

    static std::string upcaseof(std::string_view src);

  friend class FpLock;
  };

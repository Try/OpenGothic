#pragma once

#include <zenkit/world/WayNet.hh>

#include <limits>
#include <Tempest/Vec>

class FpLock;
class Interactive;

class WayPoint final {
  public:
    WayPoint();
    WayPoint(const zenkit::WayPoint& dat);
    WayPoint(const Tempest::Vec3& pos, std::string_view name);
    WayPoint(const Tempest::Vec3& pos, const Tempest::Vec3& dir, std::string_view name, bool isFp);
    WayPoint(const WayPoint&)=default;
    WayPoint(WayPoint&&)=default;

    WayPoint& operator = (WayPoint&&)=default;
    bool isLocked() const { return useCount!=0; }
    bool isFreePoint() const;
    bool isConnected() const;

    uint32_t useCounter() const { return useCount; }
    bool checkName(std::string_view name, bool inexact = true) const;

    Tempest::Vec3 position () const;
    Tempest::Vec3 direction() const;

    Tempest::Vec3 pos;
    Tempest::Vec3 dir;
    bool          underWater = false;
    bool          freePoint  = false;
    std::string   name;
    Interactive*  ladder = nullptr;

    struct Conn final {
      WayPoint* point=nullptr;
      int32_t   len  =0;
      };

    // TODO: beautify
    mutable int32_t  pathLen = std::numeric_limits<int32_t>::max();
    mutable uint16_t pathGen = 0;

    float qDistTo(float x,float y,float z) const;

    void connect(WayPoint& w);
    const std::vector<Conn>& connections() const { return conn; }
    bool hasLadderConn(const WayPoint* w) const;

  private:
    mutable uint32_t useCount=0;

    std::vector<Conn> conn;

  friend class FpLock;
  };

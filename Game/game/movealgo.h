#pragma once

#include <cstdint>
#include <array>

#include <zenload/zTypes.h>

class Npc;
class World;
class WayPoint;

class MoveAlgo final {
  public:
    MoveAlgo(Npc& unit,const World& w);

    void tick(uint64_t dt);
    void multSpeed(float s){ mulSpeed=s; }
    void clearSpeed();

    static bool isClose(const std::array<float,3>& w,const WayPoint& p);
    static bool isClose(float x,float y,float z,const WayPoint& p);

    auto aiGoTarget() -> const WayPoint* { return currentGoTo; }
    bool aiGoTo(const WayPoint* p);
    bool aiGoTo(const Npc* p, float destDist);
    void aiGoTo(const std::nullptr_t p);
    bool startClimb();
    bool hasGoTo() const;

    bool isFaling() const;
    bool isSlide()  const;
    bool isFrozen() const;
    bool isInAir()  const;
    bool isClimb()  const;

    uint8_t groundMaterial() const;

  private:
    enum Flags : uint32_t {
      NoFlags=0,
      Frozen =1<<1,
      InAir  =1<<2,
      Faling =1<<3,
      Slide  =1<<4,
      Climb  =1<<5
      };

    void   setAsFrozen(bool f);
    void   setInAir(bool f);
    void   setAsClimb(bool f);
    void   setSllideFaling(bool slide,bool faling);

    bool   processClimb();
    void   setPos(std::array<float,3> pos, uint64_t dt, float speed);
    bool   trySlide(std::array<float,3> &pos, std::array<float,3> &norm);
    bool   slideDir() const;
    void   onMoveFailed();
    float  dropRay(float x, float y, float z, bool &hasCol) const;
    void   applyRotation(std::array<float,3> &out, float *in);

    Npc&                npc;
    const World&        world;
    const WayPoint*     currentGoTo   =nullptr;
    const Npc*          currentGoToNpc=nullptr;

    float               mulSpeed  =1.f;
    std::array<float,3> aniSpeed={};
    std::array<float,3> fallSpeed={};
    uint64_t            climbStart=0;
    std::array<float,3> climbPos0={};
    float               climbHeight=0.f;
    Flags               flags=NoFlags;

    static const float slideBegin;
    static const float slideEnd;
    static const float slideSpeed;
    static const float fallThreshold;
    static const float fallWlkThreshold;
    static const float closeToPointThreshold;
  };

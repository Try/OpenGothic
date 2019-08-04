#pragma once

#include <cstdint>
#include <array>
#include <limits>

#include <zenload/zTypes.h>

class Npc;
class World;
class WayPoint;

class MoveAlgo final {
  public:
    MoveAlgo(Npc& unit);

    enum JumpCode : uint8_t {
      JM_OK,
      JM_UpLow,
      JM_UpMid,
      JM_Up,
      };

    void tick(uint64_t dt);
    void multSpeed(float s){ mulSpeed=s; }
    void clearSpeed();

    static bool isClose(const std::array<float,3>& w,const WayPoint& p);
    static bool isClose(float x,float y,float z,const WayPoint& p);

    auto aiGoTarget() -> const WayPoint* { return currentGoTo; }
    bool aiGoTo(const WayPoint* p);
    bool aiGoTo(const Npc* p, float destDist);
    void aiGoTo(const std::nullptr_t p);
    bool startClimb(JumpCode ani);
    bool hasGoTo() const;

    bool isFaling()  const;
    bool isSlide()   const;
    bool isInAir()   const;
    bool isClimb()   const;
    bool isInWater() const;
    bool isSwim() const;

    uint8_t groundMaterial() const;

  private:
    void tickMobsi  (uint64_t dt);
    bool tickSlide  (uint64_t dt);
    void tickGravity(uint64_t dt);
    void tickSwim   (uint64_t dt);
    void tickClimb  (uint64_t dt);
    bool tryMove    (float x, float y, float z);

    enum Flags : uint32_t {
      NoFlags=0,
      InAir  =1<<1,
      Faling =1<<2,
      Slide  =1<<3,
      Climb  =1<<4,
      InWater=1<<5,
      Swim   =1<<6
      };

    void   setInAir(bool f);
    void   setAsClimb(bool f);
    void   setAsSlide(bool f);
    void   setInWater(bool f);
    void   setAsSwim (bool f);

    bool   processClimb();
    bool   slideDir() const;
    void   onMoveFailed();
    void   applyRotation(std::array<float,3> &out, float *in) const;
    auto   animMoveSpeed(uint64_t dt) const -> std::array<float,3>;
    auto   npcMoveSpeed (uint64_t dt) -> std::array<float,3>;

    float  stepHeight()  const;
    float  slideAngle()  const;
    float  slideAngle2() const;
    float  waterDepthKnee() const;
    float  waterDepthChest() const;

    float  dropRay  (float x, float y, float z, bool &hasCol) const;
    float  waterRay (float x, float y, float z) const;
    auto   normalRay(float x, float y, float z) const -> std::array<float,3>;

    Npc&                npc;
    const WayPoint*     currentGoTo   =nullptr;
    const Npc*          currentGoToNpc=nullptr;

    struct Cache {
      float x=0,y=0,z=std::numeric_limits<float>::infinity();
      float rayCastRet=0; bool hasCol=false;

      float nx=0,ny=0,nz=std::numeric_limits<float>::infinity();
      std::array<float,3> norm={};

      float wx=0,wy=0,wz=std::numeric_limits<float>::infinity();
      float wdepth=0.f;
      };
    mutable Cache       cache;
    Flags               flags=NoFlags;

    float               mulSpeed  =1.f;
    std::array<float,3> fallSpeed ={};
    float               fallCount=0.f;

    uint64_t            climbStart=0;
    std::array<float,3> climbPos0={};
    float               climbHeight=0.f;
    JumpCode            jmp=JumpCode::JM_OK;

    static const float closeToPointThreshold;
    static const float gravity;
  };

#pragma once

#include <Tempest/Matrix4x4>
#include <vector>

#include "game/inventory.h"
#include "game/constants.h"
#include "staticobjects.h"
#include "animation.h"

class Skeleton;
class Overlay;
class Pose;
class Interactive;

class AnimationSolver final {
  public:
    AnimationSolver();

    enum Anim : uint16_t {
      NoAnim,
      Idle,
      GuardL,
      GuardH,
      Sit,
      Sleep,
      GuardSleep,
      IdleLoopLast=GuardSleep,

      GuardLChLeg,
      GuardLScratch,
      GuardLStrectch,
      Lookaround,
      Perception,
      Chair1,
      Chair2,
      Chair3,
      Chair4,
      Roam1,
      Roam2,
      Roam3,
      Eat,
      IdleLast=Eat,
      Warn,

      Move,
      MoveBack,
      MoveL,
      MoveR,
      RotL,
      RotR,
      Jump,
      JumpUpLow,
      JumpUpMid,
      JumpUp,
      Fall,
      FallDeep,
      SlideA,
      SlideB,
      Training,
      Interact,
      Talk,

      Atack,
      AtackL,
      AtackR,
      AtackBlock,

      MagNoMana,
      MagFib,
      MagWnd,
      MagHea,
      MagPyr,
      MagFea,
      MagTrf,
      MagSum,
      MagMsd,
      MagStm,
      MagFrz,
      MagSle,
      MagWhi,
      MagSck,

      MagFirst=MagFib,
      MagLast =MagSck
      };

    struct Overlay final {
      const Skeleton* sk  =nullptr;
      uint64_t        time=0;
      };

    void                           setPos   (const Tempest::Matrix4x4 &m);
    void                           setVisual(const Skeleton *visual, uint64_t tickCount, WeaponState ws, WalkBit walk, Interactive* inter);
    void                           setVisualBody(StaticObjects::Mesh &&h, StaticObjects::Mesh &&body);
    ZMath::float3                  animMoveSpeed(Anim a, uint64_t tickCount, uint64_t dt, WeaponState weaponSt) const;

    void                           updateAnimation(uint64_t tickCount);

    void                           addOverlay(const Skeleton *sk, uint64_t time, uint64_t tickCount, WalkBit wlk, Interactive *inter);
    void                           delOverlay(const char *sk);
    void                           delOverlay(const Skeleton *sk);

    bool                           setAnim(Anim a, uint64_t tickCount, WeaponState nextSt, WeaponState weaponSt,
                                           WalkBit walk, Interactive *inter);

    bool                           isFlyAnim(uint64_t tickCount) const;
    void                           invalidateAnim(const Animation::Sequence *ani, const Skeleton *sk, uint64_t tickCount);

    const Animation::Sequence*     solveAnim(const char *format, WeaponState st) const;
    const Animation::Sequence*     solveAnim(Anim a, WeaponState st0, Anim cur, WeaponState st, WalkBit wlk, Interactive *inter) const;

    AnimationSolver::Anim          animByName  (const std::string &name) const;
    const Animation::Sequence*     animSequence(const char *name) const;

    Tempest::Matrix4x4             pos;
    StaticObjects::Mesh            head;
    StaticObjects::Mesh            view;
    StaticObjects::Mesh            sword, bow, armour;

    std::shared_ptr<Pose>          skInst;
    const Skeleton*                skeleton=nullptr;
    const Animation::Sequence*     animSq   =nullptr;
    uint64_t                       sAnim    =0;
    Anim                           current  =NoAnim;
    Anim                           prevAni  =NoAnim;

  private:
    const Animation::Sequence*     solveMag    (const char *format,Anim spell) const;

    std::vector<Overlay>           overlay;
    const Animation::Sequence*     idle=nullptr;
  };

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
class World;

class AnimationSolver final {
  public:
    AnimationSolver();

    enum Anim : uint16_t {
      NoAnim,
      Idle,
      DeadA,
      DeadB,
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
      Food1,
      Food2,
      FoodHuge1,
      Potition1,
      Potition2,
      Potition3,
      Joint1,
      Meat1,
      Dance1,
      Dance2,
      Dance3,
      Dance4,
      Dance5,
      Dance6,
      Dance7,
      Dance8,
      Dance9,
      Pee,
      Eat,
      IdleLast=Eat,
      Warn,
      UnconsciousA,
      UnconsciousB,

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
      StumbleA,
      StumbleB,
      GotHit,

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
      MagFbt,

      MagFirst=MagFib,
      MagLast =MagFbt
      };

    struct Overlay final {
      const Skeleton* sk  =nullptr;
      uint64_t        time=0;
      };

    struct Sequence final {
      Sequence()=default;
      Sequence(const Animation::Sequence* s):Sequence(nullptr,s){}
      Sequence(const Animation::Sequence* s,const Animation::Sequence* s1)
        :cls(s1 ? s1->animCls : Animation::UnknownAnim),l0(s),l1(s1){}

      Animation::AnimClass       cls=Animation::UnknownAnim;
      const Animation::Sequence* l0=nullptr;
      const Animation::Sequence* l1=nullptr;

      const char* name() const { return l1->name.c_str(); }

      bool  isFinished(uint64_t t) const { return l1->isFinished(t); }
      float totalTime() const { return l1->totalTime(); }
      bool  isFly() const { return l1->isFly(); }

      operator bool () const { return l1!=nullptr; }

      bool operator == (std::nullptr_t) const { return l1==nullptr; }
      bool operator != (std::nullptr_t) const { return l1!=nullptr; }

      bool operator == (const Sequence& s) const { return l0==s.l0 && l1==s.l1; }
      bool operator != (const Sequence& s) const { return l0!=s.l0 || l1!=s.l1; }
      };

    void                           setPos   (const Tempest::Matrix4x4 &m);
    void                           setVisual(const Skeleton *visual, uint64_t tickCount, WeaponState ws, WalkBit walk, Interactive* inter, World &owner);
    void                           setVisualBody(StaticObjects::Mesh &&h, StaticObjects::Mesh &&body);
    ZMath::float3                  animMoveSpeed(Anim a, uint64_t tickCount, uint64_t dt, WeaponState weaponSt) const;

    void                           updateAnimation(uint64_t tickCount);
    bool                           stopAnim(const std::string& ani);
    void                           resetAni();

    void                           addOverlay(const Skeleton *sk, uint64_t time, uint64_t tickCount, WalkBit wlk, Interactive *inter, World &owner);
    void                           delOverlay(const char *sk);
    void                           delOverlay(const Skeleton *sk);

    bool                           setAnim(Anim a, uint64_t tickCount, WeaponState nextSt, WeaponState weaponSt,
                                           WalkBit walk, Interactive *inter, World &owner);

    bool                           isFlyAnim(uint64_t tickCount) const;
    void                           invalidateAnim(const Sequence ani, const Skeleton *sk, World &owner, uint64_t tickCount);

    Sequence                       solveAnim(const char *format, WeaponState st) const;
    Sequence                       solveAnim(Anim a, WeaponState st0, Anim cur, WeaponState st, WalkBit wlk, Interactive *inter) const;

    AnimationSolver::Anim          animByName  (const std::string &name) const;
    Sequence                       animSequence(const char *name) const;
    Sequence                       layredSequence(const char *name, const char *base) const;
    Sequence                       layredSequence(const char *name, const char *base, WeaponState st) const;

    Tempest::Matrix4x4             pos;
    StaticObjects::Mesh            head;
    StaticObjects::Mesh            view;
    StaticObjects::Mesh            sword, bow, armour;

    std::shared_ptr<Pose>          skInst;
    const Skeleton*                skeleton=nullptr;
    Sequence                       animSq;
    uint64_t                       sAnim    =0;
    Anim                           current  =NoAnim;
    Anim                           prevAni  =NoAnim;
    Anim                           lastIdle =Idle;

  private:
    Sequence                       solveMag (const char *format,Anim spell) const;
    Sequence                       solveDead(const char *format1,const char *format2) const;

    std::vector<Overlay>           overlay;
  };

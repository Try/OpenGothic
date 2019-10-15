#pragma once

#include <Tempest/Matrix4x4>
#include <vector>

#include "world/gsoundeffect.h"
#include "game/inventory.h"
#include "game/constants.h"
#include "meshobjects.h"
#include "pfxobjects.h"
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
      MagicSleep,
      IdleLoopLast=MagicSleep,

      Pray,
      PrayRand,
      Talk,
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
      PotitionFast,
      Joint1,
      Map1,
      MapSeal1,
      Firespit1,
      Meat1,
      Rice1,
      Rice2,
      Dance1,
      Dance2,
      Dance3,
      Dance4,
      Dance5,
      Dance6,
      Dance7,
      Dance8,
      Dance9,
      Dialog1,
      Dialog2,
      Dialog3,
      Dialog4,
      Dialog5,
      Dialog6,
      Dialog7,
      Dialog8,
      Dialog9,
      Dialog10,
      Dialog11,
      Dialog12,
      Dialog13,
      Dialog14,
      Dialog15,
      Dialog16,
      Dialog17,
      Dialog18,
      Dialog19,
      Dialog20,
      Dialog21,

      DialogFirst =Dialog1,
      DialogLastG2=Dialog11,
      DialogLastG1=Dialog21,

      Fear1,
      Fear2,
      Fear3,
      Plunder,
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
      InteractOut,

      Atack,
      AtackL,
      AtackR,
      AtackBlock,
      AtackFinish,
      StumbleA,
      StumbleB,
      GotHit,
      AimBow,

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
      const Skeleton* skeleton=nullptr;
      uint64_t        time    =0;
      };

    void                           save(Serialize& fout);
    void                           load(Serialize& fin);

    void                           setSkeleton(const Skeleton* sk);
    void                           update(uint64_t tickCount);

    void                           addOverlay(const Skeleton *sk, uint64_t time);
    void                           delOverlay(const char *sk);
    void                           delOverlay(const Skeleton *sk);

    AnimationSolver::Anim          animByName  (const std::string &name) const;
    const Animation::Sequence*     solveFrm    (const char *format) const;
    const Animation::Sequence*     solveAnim(Anim a, WeaponState st, WalkBit wlk, const Pose &pose) const;
    const Animation::Sequence*     solveAnim(WeaponState st, WeaponState cur, const Pose &pose) const;
    const Animation::Sequence*     solveAnim(Interactive *inter, Anim a, const Pose &pose) const;

  private:
    const Animation::Sequence*     solveFrm    (const char *format, WeaponState st) const;

    const Animation::Sequence*     solveMag    (const char *format,Anim spell) const;
    const Animation::Sequence*     solveDead   (const char *format1,const char *format2) const;
    const Animation::Sequence*     solveItemUse(const char *format,const char* scheme) const;

    const Skeleton*                baseSk=nullptr;
    std::vector<Overlay>           overlay;
  };

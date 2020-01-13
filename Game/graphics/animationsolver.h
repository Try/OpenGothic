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
      Fallen,
      SlideA,
      SlideB,
      InteractIn,
      InteractOut,

      Atack,
      AtackL,
      AtackR,
      AtackBlock,
      AtackFinish,
      StumbleA,
      StumbleB,
      AimBow,

      MagNoMana
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

    const Animation::Sequence*     solveAsc (const char *format) const;
    const Animation::Sequence*     solveFrm (const char *format) const;
    const Animation::Sequence*     solveAnim(Anim a, WeaponState st, WalkBit wlk, const Pose &pose) const;
    const Animation::Sequence*     solveAnim(WeaponState st, WeaponState cur, bool run) const;
    const Animation::Sequence*     solveAnim(Interactive *inter, Anim a, const Pose &pose) const;

  private:
    const Animation::Sequence*     solveFrm    (const char *format, WeaponState st) const;

    const Animation::Sequence*     solveMag    (const char *format, const std::string& spell) const;
    const Animation::Sequence*     solveDead   (const char *format1, const char *format2) const;

    const Skeleton*                baseSk=nullptr;
    std::vector<Overlay>           overlay;
  };

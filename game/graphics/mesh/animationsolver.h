#pragma once

#include <Tempest/Matrix4x4>
#include <vector>

#include "game/constants.h"
#include "animation.h"

class Skeleton;
class Overlay;
class Pose;
class Interactive;
class Serialize;
class World;

class AnimationSolver final {
  public:
    AnimationSolver();

    enum Anim : uint16_t {
      NoAnim,
      Idle,
      Move,
      CacheLast = Move,

      MoveBack,
      MoveL,
      MoveR,
      RotL,
      RotR,
      WhirlL,
      WhirlR,
      Fall,
      FallDeep,
      FallDeepA,
      FallDeepB,

      Jump,
      JumpUpLow,
      JumpUpMid,
      JumpUp,
      JumpHang,
      Fallen,
      FallenA,
      FallenB,
      SlideA,
      SlideB,

      DeadA,
      DeadB,
      UnconsciousA,
      UnconsciousB,

      InteractIn,
      InteractOut,
      InteractToStand,
      InteractFromStand,

      Attack,
      AttackL,
      AttackR,
      AttackBlock,
      AttackFinish,
      StumbleA,
      StumbleB,
      AimBow,
      PointAt,

      ItmGet,
      ItmDrop,

      MagNoMana
      };

    struct Overlay final {
      const Skeleton* skeleton=nullptr;
      uint64_t        time    =0;
      };

    void                           save(Serialize& fout) const;
    void                           load(Serialize& fin);

    void                           setSkeleton(const Skeleton* sk);
    void                           update(uint64_t tickCount);

    bool                           hasOverlay(const Skeleton*  sk) const;
    void                           addOverlay(const Skeleton*  sk, uint64_t time);
    void                           delOverlay(std::string_view sk);
    void                           delOverlay(const Skeleton*  sk);
    void                           clearOverlays();

    const Animation::Sequence*     solveNext(const Animation::Sequence& sq) const;
    const Animation::Sequence*     solveFrm (std::string_view format) const;
    const Animation::Sequence*     solveAnim(Anim a, WeaponState st, WalkBit wlk, const Pose &pose) const;
    const Animation::Sequence*     solveAnim(WeaponState st, WeaponState cur, bool run) const;
    const Animation::Sequence*     solveAnim(std::string_view scheme, bool run, bool invest) const;
    const Animation::Sequence*     solveAnim(Interactive *inter, Anim a, const Pose &pose) const;

  private:
    const Animation::Sequence*     solveFrm    (std::string_view format, WeaponState st) const;

    const Animation::Sequence*     solveMag    (std::string_view format, std::string_view spell) const;
    const Animation::Sequence*     solveDead   (std::string_view format1, std::string_view format2) const;

    const Animation::Sequence*     implSolveAnim(Anim a, WeaponState st, WalkBit wlk, const Pose &pose) const;
    void                           invalidateCache();

    const Skeleton*                baseSk=nullptr;
    std::vector<Overlay>           overlay;

    mutable const Animation::Sequence* cache [CacheLast][2][2] = {};
  };

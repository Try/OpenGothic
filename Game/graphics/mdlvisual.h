#pragma once

#include <zenload/zCModelScript.h>
#include <Tempest/Matrix4x4>

#include "game/constants.h"
#include "pfxobjects.h"
#include "meshobjects.h"
#include "animationsolver.h"

class Serialize;
class Npc;
class Item;

class MdlVisual final {
  public:
    MdlVisual();
    MdlVisual(const MdlVisual&)=delete;

    void                           save(Serialize& fout);
    void                           load(Serialize& fin, Npc &npc);

    void                           setPos(float x,float y,float z);
    void                           setPos(const Tempest::Matrix4x4 &m);
    void                           setVisual(const Skeleton *visual);
    void                           setVisualBody(MeshObjects::Mesh &&h, MeshObjects::Mesh &&body);

    void                           addOverlay(const Skeleton *sk, uint64_t time);
    void                           delOverlay(const char*     sk);
    void                           delOverlay(const Skeleton *sk);

    void                           setArmour     (MeshObjects::Mesh&& body);
    void                           setSword      (MeshObjects::Mesh&& sword);
    void                           setRangeWeapon(MeshObjects::Mesh&& bow);
    void                           setMagicWeapon(PfxObjects::Emitter&& spell);
    bool                           setFightMode(const ZenLoad::EFightMode mode);

    bool                           setToFightMode(const WeaponState ws);
    void                           updateWeaponSkeleton(const Item *sword, const Item *bow);

    const Pose&                    pose() const { return *skInst; }
    void                           updateAnimation(Npc &owner);

    void                           stopAnim(const char *ani);
    bool                           isInAnim(AnimationSolver::Anim a) const;
    bool                           isStanding() const;
    bool                           setAnim(Npc &npc, AnimationSolver::Anim a, WeaponState st);
    bool                           setAnim(Npc &npc, WeaponState st);
    void                           setRotation(Npc &npc, int dir);
    AnimationSolver::Anim          animByName(const std::string& name) const;

    const Skeleton*                skeleton=nullptr;
    Tempest::Matrix4x4             pos;

  private:
    MeshObjects::Mesh              head;
    MeshObjects::Mesh              view;
    MeshObjects::Mesh              sword, bow;
    PfxObjects::Emitter            pfx;

    WeaponState                    fightMode=WeaponState::NoWeapon;
    const Animation::Sequence*     rotation=nullptr;
    AnimationSolver                solver;
    std::unique_ptr<Pose>          skInst;
  };


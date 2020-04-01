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

    bool                           hasOverlay(const Skeleton *sk) const;
    void                           addOverlay(const Skeleton *sk, uint64_t time);
    void                           delOverlay(const char*     sk);
    void                           delOverlay(const Skeleton *sk);

    void                           setArmour     (MeshObjects::Mesh&& body);
    void                           setSword      (MeshObjects::Mesh&& sword);
    void                           setRangeWeapon(MeshObjects::Mesh&& bow);
    void                           setAmmoItem   (MeshObjects::Mesh&& ammo, const char* bone);
    void                           setMagicWeapon(PfxObjects::Emitter&& spell);
    void                           setSlotItem   (MeshObjects::Mesh&& itm, const char *bone);
    void                           setStateItem  (MeshObjects::Mesh&& itm, const char *bone);
    void                           clearSlotItem (const char *bone);
    bool                           setFightMode(const ZenLoad::EFightMode mode);

    bool                           setToFightMode(const WeaponState ws);
    void                           updateWeaponSkeleton(const Item *sword, const Item *bow);

    const Pose&                    pose() const { return *skInst; }
    void                           updateAnimation(Npc &owner, int comb);
    auto                           mapBone(const char* b) const -> std::array<float,3>;
    auto                           mapWeaponBone() const -> std::array<float,3>;

    bool                           isStanding() const;
    bool                           isItem() const;

    bool                           isAnimExist(const char* name) const;
    const Animation::Sequence*     startAnimAndGet(Npc &npc, const char* name, bool forceAnim, BodyState bs);
    const Animation::Sequence*     startAnimAndGet(Npc &npc, AnimationSolver::Anim a, WeaponState st, WalkBit wlk);
    bool                           startAnim(Npc &npc, WeaponState st);
    bool                           startAnimItem(Npc &npc, const char* scheme);
    bool                           startAnimSpell(Npc &npc, const char* scheme);
    bool                           startAnimDialog(Npc &npc);
    void                           stopDlgAnim();
    void                           stopAnim(Npc &npc, const char *ani);
    void                           stopItemStateAnim(Npc &npc);
    void                           stopWalkAnim(Npc &npc);
    void                           setRotation(Npc &npc, int dir);
    void                           interrupt();
    WeaponState                    fightMode() const { return fgtMode; }
    std::array<float,3>            displayPosition() const;
    const Tempest::Matrix4x4&      position() const { return pos; }

    const Animation::Sequence*     continueCombo(Npc& npc, AnimationSolver::Anim a, WeaponState st, WalkBit wlk);
    uint32_t                       comboLength() const;

  private:
    Tempest::Matrix4x4             pos;
    MeshObjects::Mesh              head;
    MeshObjects::Mesh              view;
    MeshObjects::Mesh              sword, bow;
    std::vector<MeshObjects::Mesh> item;
    PfxObjects::Emitter            pfx;

    MeshObjects::Mesh              ammunition, stateItm;

    const Skeleton*                skeleton=nullptr;

    WeaponState                    fgtMode=WeaponState::NoWeapon;
    AnimationSolver                solver;
    std::unique_ptr<Pose>          skInst;
  };


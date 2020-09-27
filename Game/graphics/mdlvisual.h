#pragma once

#include <zenload/zCModelScript.h>
#include <Tempest/Matrix4x4>

#include "game/constants.h"
#include "pfxobjects.h"
#include "meshobjects.h"
#include "animationsolver.h"
#include "visualfx.h"

class Serialize;
class Npc;
class Item;

class MdlVisual final {
  public:
    MdlVisual();
    MdlVisual(const MdlVisual&)=delete;

    void                           save(Serialize& fout, const Npc& npc) const;
    void                           save(Serialize& fout, const Interactive& mob) const;
    void                           load(Serialize& fin,  Npc& npc);
    void                           load(Serialize& fin,  Interactive &mob);

    void                           setPos(float x,float y,float z);
    void                           setPos(const Tempest::Matrix4x4 &m);
    void                           setTarget(const Tempest::Vec3& p);
    void                           setVisual(const Skeleton *visual);
    void                           setYTranslationEnable(bool e);
    void                           setVisualBody(MeshObjects::Mesh &&h, MeshObjects::Mesh &&body, World& owner, int32_t version);
    void                           syncAttaches();

    bool                           hasOverlay(const Skeleton *sk) const;
    void                           addOverlay(const Skeleton *sk, uint64_t time);
    void                           delOverlay(const char*     sk);
    void                           delOverlay(const Skeleton *sk);

    void                           setArmour     (MeshObjects::Mesh&& body, World& owner, const int32_t version);
    void                           setSword      (MeshObjects::Mesh&& sword);
    void                           setRangeWeapon(MeshObjects::Mesh&& bow);
    void                           setAmmoItem   (MeshObjects::Mesh&& ammo, const char* bone);
    void                           setMagicWeapon(PfxObjects::Emitter&& spell);
    void                           setSlotItem   (MeshObjects::Mesh&& itm, const char *bone);
    void                           setStateItem  (MeshObjects::Mesh&& itm, const char *bone);
    void                           clearSlotItem (const char *bone);
    bool                           setFightMode(const ZenLoad::EFightMode mode);

    void                           startParticleEffect(World& owner, const VisualFx& vfx, SpellFxKey key);
    void                           startParticleEffect(PfxObjects::Emitter&& pfx, int32_t slot, const char* bone, uint64_t timeUntil);
    void                           stopParticleEffect(int32_t slot);

    bool                           setToFightMode(const WeaponState ws);
    void                           updateWeaponSkeleton(const Item *sword, const Item *bow);

    const Pose&                    pose() const { return *skInst; }
    bool                           updateAnimation(Npc* npc, World& world);
    void                           processLayers  (World& world, int comb);
    auto                           mapBone(const size_t boneId) const -> Tempest::Vec3;
    auto                           mapWeaponBone() const -> Tempest::Vec3;

    bool                           isStanding() const;

    bool                           isAnimExist(const char* name) const;
    const Animation::Sequence*     startAnimAndGet(const char* name, uint64_t tickCount);
    const Animation::Sequence*     startAnimAndGet(Npc &npc, const char* name, int comb, bool forceAnim, BodyState bs);
    const Animation::Sequence*     startAnimAndGet(Npc &npc, AnimationSolver::Anim a, int comb,
                                                   WeaponState st, WalkBit wlk, bool noInterupt);
    bool                           startAnim(Npc &npc, WeaponState st);
    bool                           startAnimItem(Npc &npc, const char* scheme, int state);
    bool                           startAnimSpell(Npc &npc, const char* scheme);
    bool                           startAnimDialog(Npc &npc);
    void                           stopDlgAnim();
    void                           stopAnim(Npc &npc, const char *ani);
    bool                           stopItemStateAnim(Npc &npc);
    void                           stopWalkAnim(Npc &npc);
    void                           setRotation(Npc &npc, int dir);
    void                           interrupt();
    WeaponState                    fightMode() const { return fgtMode; }
    Tempest::Vec3                  displayPosition() const;
    const Tempest::Matrix4x4&      position() const { return pos; }
    float                          viewDirection() const;

    const Animation::Sequence*     continueCombo(Npc& npc, AnimationSolver::Anim a, WeaponState st, WalkBit wlk);
    uint32_t                       comboLength() const;

  private:
    template<class View>
    struct Attach {
      size_t      boneId=size_t(-1);
      View        view;
      const char* bone=nullptr;
      };
    using MeshAttach = Attach<MeshObjects::Mesh>;
    using PfxAttach  = Attach<PfxObjects::Emitter>;

    struct PfxSlot : PfxAttach {
      uint64_t timeUntil=0;
      int      id=0;
      };

    void implSetBody(MeshObjects::Mesh&& body, World& owner, const int32_t version);
    void setSlotAttachment(MeshObjects::Mesh&& itm, const char *bone);

    void bind(MeshAttach& slot, MeshObjects::Mesh&&   itm, const char *bone);
    void bind(PfxAttach&  slot, PfxObjects::Emitter&& itm, const char *bone);

    template<class View>
    void bind(Attach<View>& slot, const char *bone);
    template<class View>
    void rebindAttaches(Attach<View>& mesh,const Skeleton& from,const Skeleton& to);
    template<class View>
    void syncAttaches(Attach<View>& mesh);

    void rebindAttaches(const Skeleton& from,const Skeleton& to);

    Tempest::Matrix4x4             pos;
    Tempest::Vec3                  targetPos;
    MeshObjects::Mesh              view;

    MeshAttach                     head, sword, bow;
    MeshAttach                     ammunition, stateItm;
    std::vector<MeshAttach>        item;
    std::vector<MeshAttach>        attach;

    std::vector<PfxSlot>           effects;
    PfxAttach                      pfx;

    const Skeleton*                skeleton=nullptr;

    WeaponState                    fgtMode=WeaponState::NoWeapon;
    AnimationSolver                solver;
    std::unique_ptr<Pose>          skInst;
  };


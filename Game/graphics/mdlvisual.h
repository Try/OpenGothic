#pragma once

#include <zenload/zCModelScript.h>
#include <Tempest/Matrix4x4>

#include "graphics/mesh/animationsolver.h"
#include "graphics/pfx/pfxobjects.h"
#include "game/constants.h"
#include "meshobjects.h"
#include "effect.h"

class Serialize;
class Npc;
class Item;

class MdlVisual final {
  public:
    MdlVisual();
    MdlVisual(const MdlVisual&)=delete;
    ~MdlVisual();

    void                           save(Serialize& fout, const Npc& npc) const;
    void                           save(Serialize& fout, const Interactive& mob) const;
    void                           load(Serialize& fin,  Npc& npc);
    void                           load(Serialize& fin,  Interactive &mob);

    void                           setPos(float x,float y,float z);
    void                           setPos(const Tempest::Matrix4x4 &m);
    void                           setTarget(const Tempest::Vec3& p);
    void                           setVisual(const Skeleton *visual);
    void                           setYTranslationEnable(bool e);
    void                           setVisualBody(MeshObjects::Mesh &&body, World& owner);
    void                           setVisualBody(MeshObjects::Mesh &&h, MeshObjects::Mesh &&body, World& owner, int32_t version);
    void                           syncAttaches();
    const Skeleton*                visualSkeleton() const;

    bool                           hasOverlay(const Skeleton *sk) const;
    void                           addOverlay(const Skeleton *sk, uint64_t time);
    void                           delOverlay(const char*     sk);
    void                           delOverlay(const Skeleton *sk);
    void                           clearOverlays();

    void                           setArmour     (MeshObjects::Mesh&& body, World& owner);
    void                           setBody       (MeshObjects::Mesh&& body, World& owner, const int32_t version);
    void                           setSword      (MeshObjects::Mesh&& sword);
    void                           setRangeWeapon(MeshObjects::Mesh&& bow);
    void                           setAmmoItem   (MeshObjects::Mesh&& ammo, const char* bone);
    void                           setMagicWeapon(Effect&& spell, World& owner);
    void                           setSlotItem   (MeshObjects::Mesh&& itm, const char *bone);
    void                           setStateItem  (MeshObjects::Mesh&& itm, const char *bone);
    void                           clearSlotItem (const char *bone);
    bool                           setFightMode  (const ZenLoad::EFightMode mode);
    void                           dropWeapon    (Npc& owner);

    void                           startEffect (World& owner, Effect&& pfx, int32_t slot);
    void                           stopEffect   (const VisualFx& vfx);
    void                           stopEffect  (int32_t slot);
    void                           setEffectKey(World& owner, SpellFxKey key, int32_t keyLvl=0);
    void                           setNpcEffect(World& owner, Npc& npc, const Daedalus::ZString& s, Daedalus::GEngineClasses::C_Npc::ENPCFlag flags);

    bool                           setToFightMode(const WeaponState ws);
    void                           updateWeaponSkeleton(const Item *sword, const Item *bow);

    const Pose&                    pose() const { return *skInst; }
    bool                           updateAnimation(Npc* npc, World& world);
    void                           processLayers  (World& world);
    auto                           mapBone(const size_t boneId) const -> Tempest::Vec3;
    auto                           mapWeaponBone() const -> Tempest::Vec3;

    bool                           isStanding() const;

    bool                           isAnimExist(const char* name) const;
    const Animation::Sequence*     startAnimAndGet(const char* name, uint64_t tickCount);
    const Animation::Sequence*     startAnimAndGet(Npc &npc, const char* name, uint8_t comb, bool forceAnim, BodyState bs);
    const Animation::Sequence*     startAnimAndGet(Npc &npc, AnimationSolver::Anim a, uint8_t comb,
                                                   WeaponState st, WalkBit wlk, bool noInterupt);
    bool                           startAnim(Npc &npc, WeaponState st);
    bool                           startAnimItem(Npc &npc, const char* scheme, int state);
    bool                           startAnimSpell(Npc &npc, const char* scheme, bool invest);
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

    Bounds                         bounds() const;

  private:
    template<class View>
    struct Attach {
      size_t      boneId=size_t(-1);
      View        view;
      const char* bone=nullptr;
      };
    using MeshAttach = Attach<MeshObjects::Mesh>;
    using PfxAttach  = Attach<Effect>;

    struct PfxSlot {
      Effect      view;
      uint64_t    timeUntil=0;
      int         id=0;
      };

    void implSetBody(MeshObjects::Mesh&& body, World& owner, const int32_t version);
    void setSlotAttachment(MeshObjects::Mesh&& itm, const char *bone);

    void bind(MeshAttach& slot, MeshObjects::Mesh&&   itm, const char *bone);
    void bind(PfxAttach&  slot, Effect&& itm, const char *bone);

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
    PfxSlot                        pfx;

    Daedalus::ZString              hnpcVisualName;
    bool                           hnpcFlagGhost = false;
    PfxSlot                        hnpcVisual;

    const Skeleton*                skeleton=nullptr;

    WeaponState                    fgtMode=WeaponState::NoWeapon;
    AnimationSolver                solver;
    std::unique_ptr<Pose>          skInst;
  };


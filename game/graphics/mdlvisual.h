#pragma once

#include <Tempest/Matrix4x4>

#include "graphics/mesh/animationsolver.h"
#include "graphics/pfx/pfxobjects.h"
#include "game/constants.h"
#include "meshobjects.h"
#include "effect.h"

class Serialize;
class ObjVisual;
class Npc;
class Item;

class MdlVisual final {
  public:
    MdlVisual();
    MdlVisual(const MdlVisual&)=delete;
    MdlVisual(MdlVisual&&) = default;
    MdlVisual& operator = (MdlVisual&&) = default;
    ~MdlVisual();

    void                           save(Serialize& fout, const Npc& npc) const;
    void                           save(Serialize& fout, const Interactive& mob) const;
    void                           load(Serialize& fin,  Npc& npc);
    void                           load(Serialize& fin,  Interactive &mob);

    void                           setVisual(const Skeleton *visual);

    void                           setObjMatrix(const Tempest::Matrix4x4 &m, bool syncAttach = false);

    void                           setHeadRotation(float dx, float dz);
    Tempest::Vec2                  headRotation() const;
    void                           setYTranslationEnable(bool e);
    void                           setVisualBody(World& owner, MeshObjects::Mesh&& body);
    void                           setVisualBody(Npc& npc, MeshObjects::Mesh&& h, MeshObjects::Mesh&& body, int32_t version);
    void                           syncAttaches();
    const Skeleton*                visualSkeleton() const;
    std::string_view               visualSkeletonScheme() const;

    bool                           hasOverlay(const Skeleton*  sk) const;
    void                           addOverlay(const Skeleton*  sk, uint64_t time);
    void                           delOverlay(std::string_view sk);
    void                           delOverlay(const Skeleton*  sk);
    void                           clearOverlays();

    void                           setArmour     (Npc& npc, MeshObjects::Mesh&& armour);
    void                           setBody       (Npc& npc, MeshObjects::Mesh&& body, const int32_t version);
    void                           setSword      (MeshObjects::Mesh&& sword);
    void                           setRangeWeapon(MeshObjects::Mesh&& bow);
    void                           setShield     (MeshObjects::Mesh&& shield);
    void                           setAmmoItem   (MeshObjects::Mesh&& ammo, std::string_view bone);
    void                           setSlotItem   (MeshObjects::Mesh&& itm,  std::string_view bone);
    void                           setStateItem  (MeshObjects::Mesh&& itm,  std::string_view bone);
    void                           clearSlotItem (std::string_view bone);
    bool                           setFightMode  (zenkit::MdsFightMode mode);
    void                           dropWeapon    (Npc& owner);

    void                           setMagicWeapon(Effect&& spell, World& owner);
    void                           setMagicWeaponKey(World& owner, SpellFxKey key, int32_t keyLvl=0);

    void                           startEffect (World& owner, Effect&& pfx, int32_t slot, bool noSlot);
    void                           stopEffect  (const VisualFx& vfx);
    void                           stopEffect  (int32_t slot);
    void                           setNpcEffect(World& owner, Npc& npc, std::string_view s, zenkit::NpcFlag flags);
    void                           setFatness  (float f);
    void                           emitBlockEffect(Npc& dst, Npc& src);

    bool                           setToFightMode(const WeaponState ws);
    void                           updateWeaponSkeleton(const Item *sword, const Item *bow);

    void                           setTorch(bool t, World& owner);
    bool                           isUsingTorch() const;

    const Pose&                    pose() const { return *skInst; }
    bool                           updateAnimation(Npc* npc, World& world, uint64_t dt);
    void                           processLayers  (World& world);
    bool                           processEvents(World& world, uint64_t &barrier, Animation::EvCount &ev);
    auto                           mapBone(const size_t boneId) const -> Tempest::Vec3;
    auto                           mapWeaponBone() const -> Tempest::Vec3;
    auto                           mapHeadBone() const -> Tempest::Vec3;

    bool                           isStanding() const;

    bool                           isAnimExist(std::string_view name) const;
    const Animation::Sequence*     startAnimAndGet(std::string_view name, uint64_t tickCount, bool forceAnim = false);
    const Animation::Sequence*     startAnimAndGet(Npc& npc, std::string_view      name, uint8_t comb, BodyState bs);
    const Animation::Sequence*     startAnimAndGet(Npc& npc, AnimationSolver::Anim    a, uint8_t comb, WeaponState st, WalkBit wlk);

    bool                           startAnim      (Npc& npc, WeaponState st);
    const Animation::Sequence*     startAnimItem  (Npc& npc, std::string_view scheme, int state);
    const Animation::Sequence*     startAnimSpell (Npc& npc, std::string_view scheme, bool invest);
    bool                           startAnimDialog(Npc& npc);
    void                           startMMAnim    (Npc& npc, std::string_view anim, std::string_view node);
    void                           startFaceAnim  (Npc& npc, std::string_view anim, float intensity, uint64_t duration);
    void                           stopDlgAnim    (Npc& npc);
    void                           stopAnim       (Npc& npc, std::string_view anim);
    bool                           stopItemStateAnim(Npc &npc);
    bool                           hasAnim        (std::string_view scheme) const;
    void                           stopWalkAnim   (Npc &npc);
    void                           setAnimRotate  (Npc &npc, int dir);
    void                           setAnimWhirl   (Npc &npc, int dir);

    void                           interrupt();
    WeaponState                    fightMode() const { return fgtMode; }
    Tempest::Vec3                  displayPosition() const;
    const Tempest::Matrix4x4&      transform() const { return pos; }
    float                          viewDirection() const;

    const Animation::Sequence*     continueCombo(Npc& npc, AnimationSolver::Anim a, BodyState bs, WeaponState st, WalkBit wlk);
    uint16_t                       comboLength() const;

    Bounds                         bounds() const;

  private:
    template<class View>
    struct Attach {
      size_t           boneId=size_t(-1);
      View             view;
      std::string_view bone;
      };
    using MeshAttach = Attach<MeshObjects::Mesh>;
    using PfxAttach  = Attach<Effect>;

    struct PfxSlot {
      Effect      view;
      uint64_t    timeUntil=0;
      int         id=0;
      bool        noSlot = false;
      };

    struct TorchSlot {
      std::unique_ptr<ObjVisual> view;
      size_t                     boneId=size_t(-1);
      };

    void implSetBody(Npc* npc, World& world, MeshObjects::Mesh&& body, const int32_t version);
    void setSlotAttachment(MeshObjects::Mesh&& itm, std::string_view bone);

    void bind(MeshAttach& slot, MeshObjects::Mesh&&   itm, std::string_view bone);
    void bind(PfxAttach&  slot, Effect&& itm, std::string_view bone);

    template<class View>
    void bind(Attach<View>& slot, std::string_view bone);
    template<class View>
    void syncAttaches(Attach<View>& mesh);

    template<class View>
    void rebindAttaches(Attach<View>& mesh, const Skeleton& to);
    void rebindAttaches(const Skeleton& to);

    Tempest::Matrix4x4             pos;
    MeshObjects::Mesh              view;

    MeshAttach                     head, sword, bow, shield;
    MeshAttach                     ammunition, stateItm;
    std::vector<MeshAttach>        item;
    std::vector<MeshAttach>        attach;

    std::vector<PfxSlot>           effects;
    PfxSlot                        pfx;

    TorchSlot                      torch;

    std::string                    hnpcVisualName;
    bool                           hnpcFlagGhost = false;
    PfxSlot                        hnpcVisual;

    const Skeleton*                skeleton=nullptr;

    WeaponState                    fgtMode=WeaponState::NoWeapon;
    AnimationSolver                solver;
    std::unique_ptr<Pose>          skInst;
  };


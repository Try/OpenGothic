#pragma once

#include "world/objects/pfxemitter.h"
#include "world/objects/globalfx.h"
#include "world/objects/sound.h"
#include "graphics/lightgroup.h"
#include "graphics/meshobjects.h"
#include "graphics/visualfx.h"

#include "game/constants.h"

#include <memory>

class World;
class Npc;
class Pose;
class Skeleton;

class Effect final {
  public:
    Effect() = default;
    Effect(Effect&&) = default;
    Effect(PfxEmitter&& pfx, std::string_view node);
    Effect(const VisualFx& vfx, World& owner, const Npc& src,           SpellFxKey key = SpellFxKey::Count);
    Effect(const VisualFx& vfx, World& owner, const Tempest::Vec3& pos, SpellFxKey key = SpellFxKey::Count);
    ~Effect();

    Effect&  operator = (Effect&&) = default;
    bool     is(const VisualFx& vfx) const;
    const VisualFx* handle() const { return root; }

    void     tick(uint64_t dt);

    void     setActive(bool e);
    void     setLooped(bool l);

    void     setTarget   (const Npc* npc);
    void     setOrigin   (Npc* npc);

    void     setObjMatrix(const Tempest::Matrix4x4& mt);
    void     setKey      (World& owner, SpellFxKey key, int32_t keyLvl=0);
    void     setMesh     (const MeshObjects::Mesh* mesh);
    void     setBullet   (Bullet* b, World& owner);
    void     setSpellId  (int32_t splId, World& owner);

    uint64_t effectPrefferedTime() const;
    bool     isAlive() const;
    void     setPhysicsDisable();

    void     bindAttaches  (const Pose& pose, const Skeleton& to);
    static void onCollide  (World& owner, const VisualFx* root, const Tempest::Vec3& pos, Npc* npc, Npc* other, int32_t splId);

  private:
    void               syncAttaches(const Tempest::Matrix4x4& pos);
    void               syncAttachesSingle(const Tempest::Matrix4x4& inPos);
    void               setupLight(World& owner);
    void               setupPfx(World& owner);
    void               setupSfx(World& owner);
    void               setupCollision(World& owner);

    const VisualFx::Key*  key  = nullptr;
    const VisualFx*       root = nullptr;

    PfxEmitter            pfx;
    Sound                 sfx;
    GlobalFx              gfx;
    LightGroup::Light     light;

    Npc*                  origin = nullptr;
    const Npc*            target = nullptr;

    Bullet*               bullet = nullptr;
    int32_t               splId  = 0;

    std::string_view      nodeSlot;
    size_t                boneId   = size_t(-1);
    const Skeleton*       skeleton = nullptr;
    const Pose*           pose     = nullptr;

    const MeshObjects::Mesh* meshEmitter = nullptr;

    Tempest::Matrix4x4    pos;
    Tempest::Vec3         selfRotation;

    bool                  active    = false;
    bool                  looped    = false;
    bool                  noPhysics = false;

    std::unique_ptr<Effect> next;
  };


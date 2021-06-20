#pragma once

#include "world/objects/pfxemitter.h"
#include "world/objects/globalfx.h"
#include "world/objects/sound.h"
#include "graphics/lightgroup.h"
#include "graphics/meshobjects.h"
#include "graphics/visualfx.h"

#include "game/constants.h"

#include <memory>
#include <daedalus/ZString.h>

class World;
class Npc;
class Pose;
class Skeleton;

class Effect final {
  public:
    Effect() = default;
    Effect(Effect&&) = default;
    Effect(PfxEmitter&& pfx, const char* node);
    Effect(const VisualFx& vfx, World& owner, const Npc& src,           SpellFxKey key = SpellFxKey::Count);
    Effect(const VisualFx& vfx, World& owner, const Tempest::Vec3& pos, SpellFxKey key = SpellFxKey::Count);
    ~Effect();

    Effect&  operator = (Effect&&) = default;
    bool     is(const VisualFx& vfx) const;

    void     setActive(bool e);
    void     setLooped(bool l);

    void     setTarget   (const Tempest::Vec3& tg);
    void     setObjMatrix(Tempest::Matrix4x4& mt);
    void     setPosition (const Tempest::Vec3& pos);
    void     setKey      (World& owner, SpellFxKey key, int32_t keyLvl=0);
    void     setMesh     (const MeshObjects::Mesh* mesh);

    uint64_t effectPrefferedTime() const;
    bool     isAlive() const;

    void     bindAttaches  (const Pose& pose, const Skeleton& to);
    void     onCollide     (World& owner, const Tempest::Vec3& pos, Npc* npc);

  private:
    void               syncAttaches(const Tempest::Matrix4x4& pos);
    void               syncAttachesSingle(const Tempest::Matrix4x4& inPos);
    void               setupLight(World& owner);
    void               setupPfx(World& owner);
    void               setupSfx(World& owner);

    const VisualFx::Key*  key  = nullptr;
    const VisualFx*       root = nullptr;

    PfxEmitter            pfx;
    Sound                 sfx;
    GlobalFx              gfx;
    LightGroup::Light     light;

    const char*           nodeSlot = nullptr;
    size_t                boneId   = size_t(-1);

    const Skeleton*       skeleton    = nullptr;
    const Pose*           pose        = nullptr;

    const MeshObjects::Mesh* meshEmitter = nullptr;

    Tempest::Matrix4x4    pos;
    bool                  looped = false;

    std::unique_ptr<Effect> next;
  };


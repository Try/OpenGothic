#pragma once

#include "world/objects/pfxemitter.h"
#include "world/objects/globalfx.h"
#include "world/objects/sound.h"
#include "graphics/lightgroup.h"
#include "graphics/meshobjects.h"

#include "game/constants.h"

#include <memory>
#include <daedalus/ZString.h>

class VisualFx;
class World;
class Npc;
class Pose;
class Skeleton;

namespace Daedalus {
namespace GEngineClasses {
struct C_ParticleFXEmitKey;
}
}

class Effect final {
  public:
    Effect() = default;
    Effect(Effect&&) = default;
    Effect(PfxEmitter&& pfxOnly, const char* node);
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

    void     bindAttaches  (const Pose& pose, const Skeleton& to);
    void     onCollide     (World& owner, const Tempest::Vec3& pos, Npc* npc);

    std::unique_ptr<Effect> takeNext();

  private:
    enum LightPreset : uint8_t {
      NoPreset = 0,
      JUSTWHITE,
      WHITEBLEND,
      AURA,
      REDAMBIENCE,
      FIRESMALL,
      CATACLYSM,
      };

    void               syncAttaches(const Tempest::Matrix4x4& pos);
    void               syncAttachesSingle(const Tempest::Matrix4x4& inPos);
    static LightPreset toPreset(const Daedalus::ZString& str);

    using Key = Daedalus::GEngineClasses::C_ParticleFXEmitKey;

    const Key*            key  = nullptr;

    const VisualFx*       root = nullptr;
    PfxEmitter            visual;
    Sound                 sfx;
    GlobalFx              gfx;
    LightGroup::Light     light;

    const char*           nodeSlot = nullptr;
    size_t                boneId   = size_t(-1);

    const Skeleton*       skeleton    = nullptr;
    const Pose*           pose        = nullptr;

    const MeshObjects::Mesh* meshEmitter = nullptr;

    Tempest::Matrix4x4    pos;

    std::unique_ptr<Effect> next;
  };


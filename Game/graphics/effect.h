#pragma once

#include "pfxobjects.h"
#include "game/constants.h"

class VisualFx;
class Npc;

namespace Daedalus {
namespace GEngineClasses {
struct C_ParticleFXEmitKey;
}
}

class Effect final {
  public:
    Effect() = default;
    Effect(PfxObjects::Emitter&& pfxOnly, const char* node);
    Effect(const VisualFx& vfx, World& owner, const Npc& src,           SpellFxKey key = SpellFxKey::Count);
    Effect(const VisualFx& vfx, World& owner, const Tempest::Vec3& pos, SpellFxKey key = SpellFxKey::Count);

    void     setActive(bool e);
    void     setLooped(bool l);

    void     setTarget   (const Tempest::Vec3& tg);
    void     setObjMatrix(Tempest::Matrix4x4& mt);
    void     setPosition (const Tempest::Vec3& pos);
    void     setKey      (World& owner, SpellFxKey key);

    uint64_t effectPrefferedTime() const;

    void     bindAttaches  (const Pose& pose, const Skeleton& to);
    void     onCollide     (World& owner, const Tempest::Vec3& pos, Npc* npc);

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

    void               syncAttaches(const Tempest::Matrix4x4& pos, bool topLevel=true);
    void               syncAttachesSingle(const Tempest::Matrix4x4& inPos, bool topLevel);
    static LightPreset toPreset(const Daedalus::ZString& str);

    using Key = Daedalus::GEngineClasses::C_ParticleFXEmitKey;

    const Key*          key  = nullptr;

    const VisualFx*     root = nullptr;
    PfxObjects::Emitter visual;
    LightGroup::Light   light;
    const char*         nodeSlot = nullptr;
    size_t              boneId   = size_t(-1);

    const Skeleton*     skeleton = nullptr;
    const Pose*         pose     = nullptr;

    Tempest::Matrix4x4  pos;

    std::unique_ptr<Effect> next;
  };


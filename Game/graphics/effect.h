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

    void     setTarget   (const Tempest::Vec3& tg);
    void     setObjMatrix(Tempest::Matrix4x4& mt);
    void     setKey      (World& owner, const Tempest::Vec3& pos, SpellFxKey key);

    uint64_t effectPrefferedTime() const;

    void     bindAttaches  (const Skeleton& to);
    void     rebindAttaches(const Skeleton& from, const Skeleton& to);
    void     syncAttaches  (const Pose& pose, const Tempest::Matrix4x4& pos);
    void     onCollide     (World& owner, const Tempest::Vec3& pos, bool isDyn);

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

    static LightPreset toPreset(const Daedalus::ZString& str);

    using Key = Daedalus::GEngineClasses::C_ParticleFXEmitKey;

    const Key*          key  = nullptr;

    const VisualFx*     root = nullptr;
    PfxObjects::Emitter visual;
    LightGroup::Light   light;
    const char*         nodeSlot = nullptr;
    size_t              boneId   = size_t(-1);
    std::unique_ptr<Effect> next;
  };


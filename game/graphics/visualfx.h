#pragma once

#include <phoenix/vm.hh>
#include <phoenix/ext/daedalus_classes.hh>

#include <Tempest/Point>
#include <optional>

#include "game/constants.h"
#include "graphics/pfx/pfxobjects.h"

class World;

class VisualFx final {
  public:
    VisualFx(const phoenix::c_fx_base& src, phoenix::vm& tmpVm, std::string_view name);

    enum Collision : uint8_t {
      NoCollision = 0,
      Collide     = 1,
      Create      = 1<<1,
      CreateOnce  = 1<<2,
      NoResp      = 1<<3,
      CreateQuad  = 1<<4,
      };

    enum class CollisionAlign : uint8_t {
      Normal = 0,
      Trajectory,
      };

    enum Trajectory : uint8_t {
      TrajectoryNone = 0,
      Target         = 1<<0,
      Line           = 1<<1,
      Spline         = 1<<2,
      Random         = 1<<3,
      Fixed          = 1<<4,
      Follow         = 1<<5,
      };

    enum class LoopMode : uint8_t {
      LoopModeNone = 0,
      PinPong,
      PinPongOnce,
      Halt
      };

    enum class EaseFunc : uint8_t {
      Linear,
      };


    using OptVec3       = std::optional<Tempest::Vec3>;
    using OptTrajectory = std::optional<Trajectory>;

    class Key {
      public:
        Key() = default;
        Key(const phoenix::c_particle_fx_emit_key& k);

        // vars which influence all particles all time
        const ParticleFx* visName = nullptr;
        float             visSizeScale=0.f;
        float             scaleDuration=0.f;     // time to reach full scale at this key for relevant vars (size, alpha, etc.)

        float             pfx_ppsValue        = 0.f;
        bool              pfx_ppsIsSmoothChg  = false;  // changes pps smoothing of pfx if set to 1 and pfx pps scale keys are set
        bool              pfx_ppsIsLoopingChg = false;  // changes looping of pfx if set to 1
        float             pfx_scTime=0.f;
        OptVec3           pfx_flyGravity;

        // vars which influence particles at creation level only
        OptVec3           pfx_shpDim;
        bool              pfx_shpIsVolumeChg = false;    // changes volume rendering of pfx if set to 1
        float             pfx_shpScaleFPS=0.f;
        float             pfx_shpDistribWalkSpeed=0.f;
        OptVec3           pfx_shpOffsetVec;
        std::string       pfx_shpDistribType_S;
        std::string       pfx_dirMode_S;
        std::string       pfx_dirFOR_S;
        std::string       pfx_dirModeTargetFOR_S;
        std::string       pfx_dirModeTargetPos_S;
        float             pfx_velAvg=0.f;
        float             pfx_lspPartAvg=0.f;
        float             pfx_visAlphaStart=0.f;

        std::string       lightPresetName;
        float             lightRange=0.f;
        std::string       sfxID;
        int               sfxIsAmbient=0;
        const VisualFx*   emCreateFXID = nullptr;

        float             emFlyGravity=0.f;
        OptVec3           emSelfRotVel;
        OptTrajectory     emTrjMode;
        float             emTrjEaseVel=0.f;
        bool              emCheckCollision=0;
        uint64_t          emFXLifeSpan=0;
      private:
      };

    const char*           dbgName = "";
    std::string           visName_S;  // ParticleFx ?
    Tempest::Vec2         visSize;
    float                 visAlpha                 = 0.f;
    Material::AlphaFunc   visAlphaBlendFunc        = Material::AlphaFunc::Solid;
    float                 visTexAniFPS             = 0.f;
    bool                  visTexAniIsLooping       = false;

    Trajectory            emTrjMode                = TrajectoryNone;
    std::string           emTrjOriginNode;
    std::string           emTrjTargetNode;
    float                 emTrjTargetRange         = 0.f;
    float                 emTrjTargetAzi           = 0.f;
    float                 emTrjTargetElev          = 0.f;
    int32_t               emTrjNumKeys             = 0;
    int32_t               emTrjNumKeysVar          = 0;
    float                 emTrjAngleElevVar        = 0.f;
    float                 emTrjAngleHeadVar        = 0.f;
    float                 emTrjKeyDistVar          = 0.f;
    LoopMode              emTrjLoopMode            = LoopMode::LoopModeNone;
    EaseFunc              emTrjEaseFunc            = EaseFunc::Linear;
    float                 emTrjEaseVel             = 0.f;
    float                 emTrjDynUpdateDelay      = 0.f;
    bool                  emTrjDynUpdateTargetOnly = false;

    const VisualFx*       emFXCreate               = nullptr;
    std::string           emFXInvestOrigin;
    std::string           emFXInvestTarget;
    uint64_t              emFXTriggerDelay         = 0;
    bool                  emFXCreatedOwnTrj        = false;
    Collision             emActionCollDyn          = Collision::NoResp;
    Collision             emActionCollStat         = Collision::NoResp;
    const VisualFx*       emFXCollStat             = nullptr;
    const VisualFx*       emFXCollDyn              = nullptr;
    const VisualFx*       emFXCollDynPerc          = nullptr;
    CollisionAlign        emFXCollStatAlign        = CollisionAlign::Normal;
    CollisionAlign        emFXCollDynAlign         = CollisionAlign::Normal;
    uint64_t              emFXLifeSpan             = 0;

    bool                  emCheckCollision        = false;
    bool                  emAdjustShpToOrigin     = false;
    uint64_t              emInvestNextKeyDuration = 0;
    float                 emFlyGravity            = 0.f;
    Tempest::Vec3         emSelfRotVel;
    std::string           userString[phoenix::c_fx_base::user_string_count];
    std::string           lightPresetName;
    std::string           sfxID;
    bool                  sfxIsAmbient            = false;
    bool                  sendAssessMagic         = false;
    float                 secsPerDamage           = 0.f;

    uint64_t              effectPrefferedTime() const;
    bool                  isMeshEmmiter() const { return emTrjOriginNode=="="; }

    PfxEmitter            visual(World& owner) const;
    const Key*            key(SpellFxKey type, int32_t keyLvl=0) const;

  private:
    static Trajectory     loadTrajectory    (std::string_view str);
    static LoopMode       loadLoopmode      (std::string_view str);
    static EaseFunc       loadEaseFunc      (std::string_view str);
    static CollisionAlign loadCollisionAlign(std::string_view str);

    static Collision      strToColision(std::string_view s);

    Key                   keys  [int(SpellFxKey::Count)];
    bool                  hasKey[int(SpellFxKey::Count)] = {};
    std::vector<Key>      investKeys;
  };


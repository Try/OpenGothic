#pragma once

#include <daedalus/DaedalusStdlib.h>
#include <Tempest/Point>

#include "game/constants.h"
#include "graphics/pfx/pfxobjects.h"

class World;

class VisualFx final {
  public:
    VisualFx(Daedalus::GEngineClasses::CFx_Base&& src, Daedalus::DaedalusVM& tmpVm, const char* name);

    enum Collision : uint8_t {
      NoCollide  = 0,
      Collide    = 1,
      Create     = 1<<1,
      CreateOnce = 1<<2,
      NoResp     = 1<<3,
      CreateQuad = 1<<4,
      };

    enum class CollisionAlign : uint8_t {
      Normal = 0,
      Trajectory,
      };

    enum class Trajectory : uint8_t {
      None = 0,
      Target,
      Line,
      Spline,
      Random
      };

    enum class LoopMode : uint8_t {
      None = 0,
      PinPong,
      PinPongOnce,
      Halt
      };

    enum class EaseFunc : uint8_t {
      Linear,
      };

    class Key {
      public:
        Key() = default;
        Key(Daedalus::GEngineClasses::C_ParticleFXEmitKey&& k);

        enum OverrideBit : uint64_t {
          pfx_flyGravity_Override = 1 << 0,
          pfx_shpDim_Override = 1 << 1,
          };

        // vars which influence all particles all time
        Daedalus::ZString visName;
        float             visSizeScale=0.f;
        float             scaleDuration=0.f;     // time to reach full scale at this key for relevant vars (size, alpha, etc.)

        float             pfx_ppsValue        = 0.f;
        bool              pfx_ppsIsSmoothChg  = false;  // changes pps smoothing of pfx if set to 1 and pfx pps scale keys are set
        bool              pfx_ppsIsLoopingChg = false;  // changes looping of pfx if set to 1
        float             pfx_scTime=0.f;
        Tempest::Vec3     pfx_flyGravity;

        // vars which influence particles at creation level only
        Tempest::Vec3     pfx_shpDim;
        bool              pfx_shpIsVolumeChg = false;    // changes volume rendering of pfx if set to 1
        float             pfx_shpScaleFPS=0.f;
        float             pfx_shpDistribWalkSpeed=0.f;
        Tempest::Vec3     pfx_shpOffsetVec;
        Daedalus::ZString pfx_shpDistribType_S;
        Daedalus::ZString pfx_dirMode_S;
        Daedalus::ZString pfx_dirFOR_S;
        Daedalus::ZString pfx_dirModeTargetFOR_S;
        Daedalus::ZString pfx_dirModeTargetPos_S;
        float             pfx_velAvg=0.f;
        float             pfx_lspPartAvg=0.f;
        float             pfx_visAlphaStart=0.f;

        Daedalus::ZString lightPresetName;
        float             lightRange=0.f;
        Daedalus::ZString sfxID;
        int               sfxIsAmbient=0;
        Daedalus::ZString emCreateFXID;

        float             emFlyGravity=0.f;
        Tempest::Vec3     emSelfRotVel;
        Daedalus::ZString emTrjMode_S;
        float             emTrjEaseVel=0.f;
        bool              emCheckCollision=0;
        uint64_t          emFXLifeSpan=0;
      private:
      };

    Daedalus::ZString     visName_S;  // ParticleFx ?
    Tempest::Vec2	        visSize;
    float	                visAlpha                 = 0.f;
    Material::AlphaFunc	  visAlphaBlendFunc        = Material::AlphaFunc::Solid;
    float                 visTexAniFPS             = 0.f;
    bool                  visTexAniIsLooping       = false;

    Trajectory	          emTrjMode                = Trajectory::None;
    std::string	          emTrjOriginNode;
    std::string           emTrjTargetNode;
    float                 emTrjTargetRange         = 0.f;
    float	                emTrjTargetAzi           = 0.f;
    float                 emTrjTargetElev          = 0.f;
    int32_t		            emTrjNumKeys             = 0;
    int32_t		            emTrjNumKeysVar          = 0;
    float	                emTrjAngleElevVar        = 0.f;
    float	                emTrjAngleHeadVar        = 0.f;
    float	                emTrjKeyDistVar          = 0.f;
    LoopMode              emTrjLoopMode            = LoopMode::None;
    EaseFunc              emTrjEaseFunc            = EaseFunc::Linear;
    float     	          emTrjEaseVel             = 0.f;
    float	                emTrjDynUpdateDelay      = 0.f;
    bool                  emTrjDynUpdateTargetOnly = false;

    const VisualFx*   	  emFXCreate               = nullptr;
    std::string           emFXInvestOrigin;
    std::string   	      emFXInvestTarget;
    uint64_t              emFXTriggerDelay         = 0;
    bool                  emFXCreatedOwnTrj        = false;
    Collision             emActionCollDyn          = Collision::NoResp; // CREATE, BOUNCE, CREATEONCE, NORESP, COLLIDE
    Collision             emActionCollStat         = Collision::NoResp; // CREATE, BOUNCE, CREATEONCE, NORESP, COLLIDE, CREATEQUAD
    const VisualFx*       emFXCollStat             = nullptr;
    const VisualFx* 	    emFXCollDyn              = nullptr;
    const VisualFx*       emFXCollDynPerc          = nullptr;
    CollisionAlign        emFXCollStatAlign        = CollisionAlign::Normal;
    CollisionAlign        emFXCollDynAlign         = CollisionAlign::Normal;
    uint64_t              emFXLifeSpan             = 0;

    bool  	              emCheckCollision        = false;
    bool                  emAdjustShpToOrigin     = false;
    uint64_t              emInvestNextKeyDuration = 0;
    float	                emFlyGravity            = 0.f;
    Tempest::Vec3         emSelfRotVel;
    Daedalus::ZString     userString[Daedalus::GEngineClasses::VFX_NUM_USERSTRINGS];
    Daedalus::ZString     lightPresetName;
    Daedalus::ZString     sfxID;
    bool    	            sfxIsAmbient            = false;
    bool    		          sendAssessMagic         = false;
    float	                secsPerDamage           = 0.f;

    uint64_t              effectPrefferedTime() const;
    bool                  isMeshEmmiter() const { return emTrjOriginNode=="="; }

    PfxEmitter            visual(World& owner) const;
    const Key&            key(SpellFxKey type, int32_t keyLvl=0) const;

  private:
    static Trajectory     loadTrajectory    (const Daedalus::ZString& str);
    static LoopMode       loadLoopmode      (const Daedalus::ZString& str);
    static EaseFunc       loadEaseFunc      (const Daedalus::ZString& str);
    static CollisionAlign loadCollisionAlign(const Daedalus::ZString& str);

    static Collision      strToColision(const char* s);

    Key                   keys[int(SpellFxKey::Count)];
    std::vector<Key>      investKeys;
  };


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
    std::string           sfxID;
    bool    	            sfxIsAmbient            = false;
    bool    		          sendAssessMagic         = false;
    float	                secsPerDamage           = 0.f;

    uint64_t                                             effectPrefferedTime() const;
    bool                                                 isMeshEmmiter() const { return emTrjOriginNode=="="; }

    PfxEmitter                                           visual(World& owner) const;
    const Daedalus::GEngineClasses::C_ParticleFXEmitKey& key(SpellFxKey type, int32_t keyLvl=0) const;

  private:
    static Trajectory                                    loadTrajectory    (const Daedalus::ZString& str);
    static LoopMode                                      loadLoopmode      (const Daedalus::ZString& str);
    static EaseFunc                                      loadEaseFunc      (const Daedalus::ZString& str);
    static CollisionAlign                                loadCollisionAlign(const Daedalus::ZString& str);

    static Collision                                     strToColision(const char* s);

    Daedalus::GEngineClasses::C_ParticleFXEmitKey              keys[int(SpellFxKey::Count)];
    std::vector<Daedalus::GEngineClasses::C_ParticleFXEmitKey> investKeys;
  };


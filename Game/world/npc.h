#pragma once

#include "graphics/meshobjects.h"
#include "graphics/pose.h"
#include "graphics/animation.h"
#include "graphics/mdlvisual.h"
#include "game/gametime.h"
#include "game/movealgo.h"
#include "game/inventory.h"
#include "game/fightalgo.h"
#include "game/perceptionmsg.h"
#include "game/gamescript.h"
#include "physics/dynamicworld.h"
#include "fplock.h"
#include "waypath.h"

#include <cstdint>
#include <string>
#include <deque>

#include <daedalus/DaedalusVM.h>

class Interactive;
class WayPoint;

class Npc final {
  public:
    enum ProcessPolicy : uint8_t {
      Player,
      AiNormal,
      AiFar,
      AiFar2
      };

    enum MoveCode : uint8_t {
      MV_FAILED,
      MV_OK,
      MV_CORRECT
      };

    using JumpCode = MoveAlgo::JumpCode;

    enum PercType : uint8_t {
      PERC_ASSESSPLAYER       = 1,
      PERC_ASSESSENEMY        = 2,
      PERC_ASSESSFIGHTER      = 3,
      PERC_ASSESSBODY         = 4,
      PERC_ASSESSITEM         = 5,
      PERC_ASSESSMURDER       = 6,
      PERC_ASSESSDEFEAT       = 7,
      PERC_ASSESSDAMAGE       = 8,
      PERC_ASSESSOTHERSDAMAGE = 9,
      PERC_ASSESSTHREAT       = 10,
      PERC_ASSESSREMOVEWEAPON = 11,
      PERC_OBSERVEINTRUDER    = 12,
      PERC_ASSESSFIGHTSOUND   = 13,
      PERC_ASSESSQUIETSOUND   = 14,
      PERC_ASSESSWARN         = 15,
      PERC_CATCHTHIEF         = 16,
      PERC_ASSESSTHEFT        = 17,
      PERC_ASSESSCALL         = 18,
      PERC_ASSESSTALK         = 19,
      PERC_ASSESSGIVENITEM    = 20,
      PERC_ASSESSFAKEGUILD    = 21,
      PERC_MOVEMOB            = 22,
      PERC_MOVENPC            = 23,
      PERC_DRAWWEAPON         = 24,
      PERC_OBSERVESUSPECT     = 25,
      PERC_NPCCOMMAND         = 26,
      PERC_ASSESSMAGIC        = 27,
      PERC_ASSESSSTOPMAGIC    = 28,
      PERC_ASSESSCASTER       = 29,
      PERC_ASSESSSURPRISE     = 30,
      PERC_ASSESSENTERROOM    = 31,
      PERC_ASSESSUSEMOB       = 32,
      PERC_Count
      };

    enum Talent : uint8_t {
      TALENT_UNKNOWN            = 0,
      TALENT_1H                 = 1,
      TALENT_2H                 = 2,
      TALENT_BOW                = 3,
      TALENT_CROSSBOW           = 4,
      TALENT_PICKLOCK           = 5,
      TALENT_MAGE               = 7,
      TALENT_SNEAK              = 8,
      TALENT_REGENERATE         = 9,
      TALENT_FIREMASTER         = 10,
      TALENT_ACROBAT            = 11,
      TALENT_PICKPOCKET         = 12,
      TALENT_SMITH              = 13,
      TALENT_RUNES              = 14,
      TALENT_ALCHEMY            = 15,
      TALENT_TAKEANIMALTROPHY   = 16,
      TALENT_FOREIGNLANGUAGE    = 17,
      TALENT_WISPDETECTOR       = 18,
      TALENT_C                  = 19,
      TALENT_D                  = 20,
      TALENT_E                  = 21,

      TALENT_MAX_G1             = 12,
      TALENT_MAX_G2             = 22
      };

    enum Attribute : uint8_t {
      ATR_HITPOINTS      = 0,
      ATR_HITPOINTSMAX   = 1,
      ATR_MANA           = 2,
      ATR_MANAMAX        = 3,
      ATR_STRENGTH       = 4,
      ATR_DEXTERITY      = 5,
      ATR_REGENERATEHP   = 6,
      ATR_REGENERATEMANA = 7,
      ATR_MAX
      };

    enum SpellCode : int32_t {
      SPL_DONTINVEST                  = 0,
      SPL_RECEIVEINVEST               = 1,
      SPL_SENDCAST                    = 2,
      SPL_SENDSTOP                    = 3,
      SPL_NEXTLEVEL                   = 4,
      SPL_STATUS_CANINVEST_NO_MANADEC = 8,
      SPL_FORCEINVEST		              = 1 << 16
      };

    enum Protection : uint8_t {
      PROT_BARRIER = 0,
      PROT_BLUNT   = 1,
      PROT_EDGE    = 2,
      PROT_FIRE    = 3,
      PROT_FLY     = 4,
      PROT_MAGIC   = 5,
      PROT_POINT   = 6,
      PROT_FALL    = 7,
      PROT_MAX     = 8
      };

    enum class State : uint32_t {
      INVALID     = 0,
      ANSWER      = 1,
      DEAD        = 2,
      UNCONSCIOUS = 3,
      FADEAWAY    = 4,
      FOLLOW      = 5,
      Count       = 6
      };

    enum GoToHint : uint8_t {
      GT_No,
      GT_Way,
      GT_NextFp,
      GT_Enemy,
      };

    enum HitSound : uint8_t {
      HS_NoSound = 0,
      HS_Dead    = 1
      };

    using Anim = AnimationSolver::Anim;

    Npc(World &owner, size_t instance, const Daedalus::ZString& waypoint);
    Npc(World &owner, Serialize& fin);
    Npc(const Npc&)=delete;
    ~Npc();

    void save(Serialize& fout);
    void load(Serialize& fout);

    bool setPosition (float x,float y,float z);
    bool setPosition (const std::array<float,3>& pos);
    void setDirection(float x,float y,float z);
    void setDirection(const std::array<float,3>& pos);
    void setDirection(float rotation);
    void clearSpeed();
    static float angleDir(float x,float z);
    bool resetPositionToTA();

    void stopDlgAnim();

    void setProcessPolicy(ProcessPolicy t);
    auto processPolicy() const -> ProcessPolicy { return aiPolicy; }

    bool isPlayer() const;
    void setWalkMode(WalkBit m);
    auto walkMode() const { return wlkMode; }
    void tick(uint64_t dt);
    bool startClimb(JumpCode code);

    auto world() -> World&;

    std::array<float,3> position() const;
    std::array<float,3> cameraBone() const;
    float               collisionRadius() const;
    float               rotation() const;
    float               rotationRad() const;
    float               translateY() const;
    float               centerY() const;
    Npc*                lookAtTarget() const;

    float               qDistTo(float x,float y,float z) const;
    float               qDistTo(const WayPoint* p) const;
    float               qDistTo(const Npc& p) const;
    float               qDistTo(const Interactive& p) const;

    void updateAnimation();
    void updateTransform();

    const char*         displayName() const;
    std::array<float,3> displayPosition() const;
    void setVisual    (const char *visual);
    void setVisual    (const Skeleton *visual);
    bool hasOverlay   (const char*     sk) const;
    bool hasOverlay   (const Skeleton* sk) const;
    void addOverlay   (const char*     sk, uint64_t time);
    void addOverlay   (const Skeleton *sk, uint64_t time);
    void delOverlay   (const char*     sk);
    void delOverlay   (const Skeleton *sk);

    void setVisualBody (int32_t headTexNr,int32_t teethTexNr,int32_t bodyVer,int32_t bodyColor,const std::string& body,const std::string& head);
    void updateArmour  ();
    void setSword      (MeshObjects::Mesh&& sword);
    void setRangeWeapon(MeshObjects::Mesh&& bow);
    void setMagicWeapon(PfxObjects::Emitter&& spell);
    void setSlotItem   (MeshObjects::Mesh&& itm,const char* slot);
    void setStateItem  (MeshObjects::Mesh&& itm,const char* slot);
    void setAmmoItem   (MeshObjects::Mesh&& itm,const char* slot);
    void clearSlotItem (const char* slot);
    void setPhysic     (DynamicWorld::Item&& item);
    void setFatness    (float f);
    void setScale      (float x,float y,float z);

    bool setAnim(Anim a);
    void setAnimRotate(int rot);
    bool setAnimItem(const char* scheme);
    void stopAnim(const std::string& ani);
    bool isFinishingMove() const;

    ZMath::float3 animMoveSpeed(uint64_t dt) const;

    bool          isJumpAnim() const;
    bool          isFlyAnim() const;
    bool          isFaling() const;
    bool          isSlide() const;
    bool          isInAir() const;
    bool          isStanding() const;

    void     setTalentSkill(Talent t,int32_t lvl);
    int32_t  talentSkill(Talent t) const;

    void     setTalentValue(Talent t,int32_t lvl);
    int32_t  talentValue(Talent t) const;
    int32_t  hitChanse(Talent t) const;

    void     setRefuseTalk(uint64_t milis);
    bool     isRefuseTalk() const;

    int32_t  mageCycle() const;
    int32_t  attribute (Attribute a) const;
    void     changeAttribute(Attribute a, int32_t val, bool allowUnconscious);
    int32_t  protection(Protection p) const;
    void     changeProtection(Protection p,int32_t val);

    uint32_t instanceSymbol() const;
    uint32_t guild() const;
    bool     isMonster() const;
    void     setTrueGuild(int32_t g);
    int32_t  trueGuild() const;
    int32_t  magicCyrcle() const;
    int32_t  level() const;
    int32_t  experience() const;
    int32_t  experienceNext() const;
    int32_t  learningPoints() const;

    void      setAttitude(Attitude att);
    Attitude  attitude() const { return permAttitude; }

    bool      isFriend() const;

    void      setTempAttitude(Attitude att);
    Attitude  tempAttitude() const { return tmpAttitude; }

    void      startDialog(Npc& other);
    bool      startState(size_t id, const Daedalus::ZString& wp);
    bool      startState(size_t id, const Daedalus::ZString& wp, gtime endTime, bool noFinalize);
    void      clearState(bool noFinalize);
    BodyState bodyState() const;
    BodyState bodyStateMasked() const;

    void setToFightMode(const size_t item);
    void setToFistMode();

    bool closeWeapon(bool noAnim);
    bool drawWeaponFist();
    bool drawWeaponMele();
    bool drawWeaponBow();
    bool drawMage(uint8_t slot);
    bool drawSpell(int32_t spell);
    auto weaponState() const -> WeaponState;

    bool canFinish(Npc &oth);
    void fistShoot();
    void blockFist();
    bool finishingMove();
    void swingSword();
    void swingSwordL();
    void swingSwordR();
    void blockSword();
    bool castSpell();
    bool aimBow();
    bool shootBow();
    bool hasAmunition() const;

    bool isEnemy(const Npc& other) const;
    bool isDead() const;
    bool isUnconscious() const;
    bool isDown() const;
    bool isTalk() const;
    bool isPrehit() const;
    bool isImmortal() const;

    void setPerceptionTime   (uint64_t time);
    void setPerceptionEnable (PercType t, size_t fn);
    void setPerceptionDisable(PercType t);

    bool     perceptionProcess(Npc& pl, float quadDist);
    bool     perceptionProcess(Npc& pl, Npc *victum, float quadDist, PercType perc);
    bool     hasPerc(PercType perc) const;
    uint64_t percNextTime() const;

    Interactive* interactive() const { return currentInteract; }
    bool     setInteraction(Interactive* id, bool quick=false);
    void     quitIneraction();
    bool     isState(size_t stateFn) const;
    bool     wasInState(size_t stateFn) const;
    uint64_t stateTime() const;
    void     setStateTime(int64_t time);

    void     addRoutine(gtime s, gtime e, uint32_t callback, const WayPoint* point);
    void     excRoutine(size_t callback);
    void     multSpeed(float s);

    MoveCode testMove  (const std::array<float,3>& pos, std::array<float,3> &fallback, float speed);
    bool     tryMove   (const std::array<float,3> &dp);

    JumpCode tryJump(const std::array<float,3>& pos);
    float    clampHeight(Anim a) const;
    bool     hasCollision() const { return physic.hasCollision(); }

    std::vector<GameScript::DlgChoise> dialogChoises(Npc &player, const std::vector<uint32_t> &except, bool includeImp);

    Daedalus::GEngineClasses::C_Npc* handle(){ return  &hnpc; }

    auto     inventory() const -> const Inventory& { return invent; }
    size_t   hasItem    (size_t id) const;
    Item*    getItem    (size_t id);
    Item*    addItem    (size_t id, uint32_t amount);
    Item*    addItem    (std::unique_ptr<Item>&& i);
    void     delItem    (size_t id, uint32_t amount);
    void     useItem    (size_t item, bool force=false);
    void     setCurrentItem(size_t item);
    void     unequipItem(size_t item);
    void     addItem    (size_t id, Interactive& chest,uint32_t count=1);
    void     addItem    (size_t id, Npc& from, uint32_t count=1);
    void     moveItem   (size_t id, Interactive& to,uint32_t count=1);
    void     sellItem   (size_t id, Npc& to,uint32_t count=1);
    void     buyItem    (size_t id, Npc& from,uint32_t count=1);
    void     clearInventory();
    Item*    currentArmour();
    Item*    currentMeleWeapon();
    Item*    currentRangeWeapon();

    bool     lookAt(float dx, float dz, bool anim, uint64_t dt);
    auto     playAnimByName(const Daedalus::ZString& name, BodyState bs) -> const Animation::Sequence*;

    bool     checkGoToNpcdistance(const Npc& other);
    void     aiLookAt(Npc* other);
    void     aiStopLookAt();
    void     aiRemoveWeapon();
    void     aiTurnToNpc(Npc *other);
    void     aiGoToNpc  (Npc *other);
    void     aiGoToNextFp(const Daedalus::ZString& fp);
    void     aiStartState(uint32_t stateFn, int behavior, Npc *other, const Daedalus::ZString& wp);
    void     aiPlayAnim(const Daedalus::ZString& ani);
    void     aiPlayAnimBs(const Daedalus::ZString& ani, BodyState bs);
    void     aiWait(uint64_t dt);
    void     aiStandup();
    void     aiStandupQuick();
    void     aiGoToPoint(const WayPoint &to);
    void     aiEquipArmor(int32_t id);
    void     aiEquipBestArmor();
    void     aiEquipBestMeleWeapon();
    void     aiEquipBestRangeWeapon();
    void     aiUseMob(const Daedalus::ZString& name,int st);
    void     aiUseItem(int32_t id);
    void     aiUseItemToState(int32_t id,int32_t state);
    void     aiTeleport(const WayPoint& to);
    void     aiReadyMeleWeapon();
    void     aiReadyRangeWeapon();
    void     aiReadySpell(int32_t spell, int32_t mana);
    void     aiAtack();
    void     aiFlee();
    void     aiDodge();
    void     aiUnEquipWeapons();
    void     aiUnEquipArmor();
    void     aiProcessInfo(Npc& other);
    void     aiOutput(Npc &to, const Daedalus::ZString& text, int order);
    void     aiOutputSvm(Npc &to, const Daedalus::ZString& text, int order);
    void     aiOutputSvmOverlay(Npc &to, const Daedalus::ZString& text, int order);
    void     aiStopProcessInfo();
    void     aiContinueRoutine();
    void     aiAlignToFp();
    void     aiAlignToWp();
    void     aiSetNpcsToState(size_t func, int32_t radius);
    void     aiSetWalkMode(WalkBit w);
    void     aiFinishingMove(Npc& other);

    bool     isAiQueueEmpty() const;
    void     clearAiQueue();

    auto     currentWayPoint() const -> const WayPoint* { return currentFp; }
    void     attachToPoint(const WayPoint* p);
    GoToHint moveHint() const { return go2.flag; }
    void     clearGoTo();

    bool     canSeeNpc(const Npc& oth,bool freeLos) const;
    bool     canSeeNpc(float x,float y,float z,bool freeLos) const;
    auto     canSenseNpc(const Npc& oth,bool freeLos, float extRange=0.f) const -> SensesBit;
    auto     canSenseNpc(float x,float y,float z,bool freeLos, float extRange=0.f) const -> SensesBit;

    void     setTarget(Npc* t);
    Npc*     target();

    void     clearNearestEnemy();
    int32_t  lastHitSpellId() const { return lastHitSpell; }

    void     setOther(Npc* ot);

    bool     haveOutput() const;
    void     setAiOutputBarrier(uint64_t dt);

    bool     doAttack(Anim anim);
    void     takeDamage(Npc& other,const Bullet* b);
    void     emitDlgSound(const char* sound);
    void     emitSoundEffect(const char* sound, float range, bool freeSlot);
    void     emitSoundGround(const char* sound, float range, bool freeSlot);

  private:
    struct Routine final {
      gtime           start;
      gtime           end;
      uint32_t        callback=0;
      const WayPoint* point=nullptr;
      };

    enum Action:uint32_t {
      AI_None  =0,
      AI_LookAt,
      AI_StopLookAt,
      AI_RemoveWeapon,
      AI_TurnToNpc,
      AI_GoToNpc,
      AI_GoToNextFp,
      AI_GoToPoint,
      AI_StartState,
      AI_PlayAnim,
      AI_PlayAnimBs,
      AI_Wait,
      AI_StandUp,
      AI_StandUpQuick,
      AI_EquipArmor,
      AI_EquipBestArmor,
      AI_EquipMelee,
      AI_EquipRange,
      AI_UseMob,
      AI_UseItem,
      AI_UseItemToState,
      AI_Teleport,
      AI_DrawWeaponMele,
      AI_DrawWeaponRange,
      AI_DrawSpell,
      AI_Atack,
      AI_Flee,
      AI_Dodge,
      AI_UnEquipWeapons,
      AI_UnEquipArmor,
      AI_Output,
      AI_OutputSvm,
      AI_OutputSvmOverlay,
      AI_ProcessInfo,
      AI_StopProcessInfo,
      AI_ContinueRoutine,
      AI_AlignToFp,
      AI_AlignToWp,
      AI_SetNpcsToState,
      AI_SetWalkMode,
      AI_FinishingMove,
      };

    enum TransformBit : uint8_t {
      TR_Pos  =1,
      TR_Rot  =1<<1,
      TR_Scale=1<<2,
      };

    struct AiAction final {
      Action            act   =AI_None;
      Npc*              target=nullptr;
      const WayPoint*   point =nullptr;
      size_t            func  =0;
      int               i0    =0;
      int               i1    =0;
      Daedalus::ZString s0;
      };

    struct AiState final {
      size_t   funcIni =0;
      size_t   funcLoop=0;
      size_t   funcEnd =0;
      uint64_t sTime   =0;
      gtime    eTime   =gtime::endOfTime();
      bool     started =false;
      uint64_t loopNextTime=0;
      const char* hint="";
      };

    struct Perc final {
      size_t func = size_t(-1);
      };

    struct GoTo final {
      GoToHint         flag = GoToHint::GT_No;
      Npc*             npc  = nullptr;
      const WayPoint*  wp   = nullptr;

      void                         save(Serialize& fout) const;
      void                         load(Serialize&  fin);

      bool                         empty() const;
      void                         clear();
      void                         set(Npc* to, GoToHint hnt = GoToHint::GT_Way);
      void                         set(const WayPoint* to, GoToHint hnt = GoToHint::GT_Way);
      };

    void                           updateWeaponSkeleton();
    void                           tickTimedEvt(Animation::EvCount &ev);
    void                           updatePos();
    bool                           setViewPosition(const std::array<float,3> &pos);

    int                            aiOutputOrderId() const;
    bool                           performOutput(const AiAction &ai);

    const Routine&                 currentRoutine() const;
    gtime                          endTime(const Routine& r) const;

    bool                           implLookAt (uint64_t dt);
    bool                           implLookAt (const Npc& oth, uint64_t dt);
    bool                           implLookAt (const Npc& oth, bool noAnim, uint64_t dt);
    bool                           implLookAt (float dx, float dz, bool noAnim, uint64_t dt);
    bool                           implGoTo   (uint64_t dt);
    bool                           implGoTo   (uint64_t dt, float destDist);
    bool                           implAtack  (uint64_t dt);
    bool                           implAiTick (uint64_t dt);
    void                           implAiWait (uint64_t dt);
    void                           implAniWait(uint64_t dt);
    void                           implFaiWait(uint64_t dt);
    void                           tickRoutine();
    void                           nextAiAction(uint64_t dt);
    void                           commitDamage();
    void                           takeDamage(Npc& other);
    Npc*                           updateNearestEnemy();
    Npc*                           updateNearestBody();
    bool                           checkHealth(bool onChange, bool forceKill);
    void                           onNoHealth(bool death, HitSound sndMask);
    bool                           hasAutoroll() const;

    void                           save(Serialize& fout,Daedalus::GEngineClasses::C_Npc& hnpc) const;
    void                           load(Serialize& fin, Daedalus::GEngineClasses::C_Npc& hnpc);

    void                           save(Serialize& fout,const Daedalus::GEngineClasses::C_Npc::ENPCFlag& flg) const;
    void                           load(Serialize& fin, Daedalus::GEngineClasses::C_Npc::ENPCFlag&       flg);

    void                           saveAiState(Serialize& fout) const;
    void                           loadAiState(Serialize& fin);

    World&                         owner;
    Daedalus::GEngineClasses::C_Npc hnpc={};
    float                          x=0.f;
    float                          y=0.f;
    float                          z=0.f;
    float                          angle=0.f;
    float                          sz[3]={1.f,1.f,1.f};

    // visual props (cache)
    uint8_t                        durtyTranform=0;
    std::array<float,3>            groundNormal;

    // visual props
    std::string                    body,head;
    int32_t                        vHead=0, vTeeth=0, vColor =0;
    int32_t                        bdColor=0;
    MdlVisual                      visual;

    DynamicWorld::Item             physic;

    WalkBit                        wlkMode                 =WalkBit::WM_Run;
    int32_t                        trGuild                 =GIL_NONE;
    int32_t                        talentsSk[TALENT_MAX_G2]={};
    int32_t                        talentsVl[TALENT_MAX_G2]={};
    uint64_t                       refuseTalkMilis         =0;

    // attitude
    Attitude                       permAttitude=ATT_NULL;
    Attitude                       tmpAttitude =ATT_NULL;

    // perception
    uint64_t                       perceptionTime    =0;
    uint64_t                       perceptionNextTime=0;
    Perc                           perception[PERC_Count];

    // inventory
    Inventory                      invent;

    // last hit
    Npc*                           lastHit      = nullptr;
    char                           lastHitType  = 'A';
    int32_t                        lastHitSpell = 0;

    // ai state
    uint64_t                       aniWaitTime=0;
    uint64_t                       waitTime=0;
    uint64_t                       faiWaitTime=0;
    uint64_t                       aiOutputBarrier=0;
    ProcessPolicy                  aiPolicy=ProcessPolicy::AiNormal;
    AiState                        aiState;
    size_t                         aiPrevState=0;
    std::deque<AiAction>           aiActions;
    std::vector<Routine>           routines;

    Interactive*                   currentInteract=nullptr;
    Npc*                           currentOther   =nullptr;
    Npc*                           currentLookAt  =nullptr;
    Npc*                           currentTarget  =nullptr;
    Npc*                           nearestEnemy   =nullptr;
    AiOuputPipe*                   outputPipe     =nullptr;

    GoTo                           go2;
    const WayPoint*                currentFp      =nullptr;
    FpLock                         currentFpLock;
    WayPath                        wayPath;

    MoveAlgo                       mvAlgo;
    FightAlgo                      fghAlgo;
    uint64_t                       lastEventTime=0;

  friend class MoveAlgo;
  };

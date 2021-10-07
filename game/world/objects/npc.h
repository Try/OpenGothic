#pragma once

#include "graphics/mesh/pose.h"
#include "graphics/mesh/animation.h"
#include "graphics/meshobjects.h"
#include "graphics/mdlvisual.h"
#include "game/gametime.h"
#include "game/movealgo.h"
#include "game/inventory.h"
#include "game/fightalgo.h"
#include "game/perceptionmsg.h"
#include "game/gamescript.h"
#include "physics/dynamicworld.h"
#include "world/aiqueue.h"
#include "world/fplock.h"
#include "world/waypath.h"

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

    using JumpStatus = MoveAlgo::JumpStatus;

    enum GoToHint : uint8_t {
      GT_No,
      GT_Way,
      GT_NextFp,
      GT_EnemyA,
      GT_Item,
      GT_Point,
      GT_EnemyG,
      GT_Flee,
      };

    enum HitSound : uint8_t {
      HS_NoSound = 0,
      HS_Dead    = 1
      };

    enum CastState : uint8_t {
      CS_NoCast      = 0,
      CS_Finalize    = 1,

      CS_Invest_0    = 16,
      CS_Invest_1    = 17,
      CS_Invest_2    = 19,
      CS_Invest_3    = 20,
      CS_Invest_4    = 21,
      CS_Invest_5    = 22,
      CS_Invest_6    = 23,
      CS_Invest_Last = 31,
      CS_Cast_0      = 32,
      CS_Cast_1      = 33,
      CS_Cast_2      = 34,
      CS_Cast_3      = 35,
      CS_Cast_Last   = 47,
      };

    using Anim = AnimationSolver::Anim;

    Npc(World &owner, size_t instance, std::string_view waypoint);
    Npc(World &owner, Serialize& fin);
    Npc(const Npc&)=delete;
    ~Npc();

    void       save(Serialize& fout);
    void       load(Serialize& fout);
    void       postValidate();

    bool       setPosition (float x,float y,float z);
    bool       setPosition (const Tempest::Vec3& pos);
    void       setDirection(float x,float y,float z);
    void       setDirection(float rotation);
    void       setDirectionY(float rotation);
    void       setRunAngle  (float angle);

    auto       transform()  const -> Tempest::Matrix4x4;
    auto       position()   const -> Tempest::Vec3;
    auto       cameraBone(bool isFirstPerson = false) const -> Tempest::Vec3;
    float      collisionRadius() const;
    float      rotation() const;
    float      rotationRad() const;
    float      rotationY() const;
    float      rotationYRad() const;
    float      runAngle() const { return runAng; }
    Bounds     bounds() const;

    void       stopDlgAnim();
    void       clearSpeed();
    bool       resetPositionToTA();

    void       setProcessPolicy(ProcessPolicy t);
    auto       processPolicy() const -> ProcessPolicy { return aiPolicy; }

    bool       isPlayer() const;
    void       setWalkMode(WalkBit m);
    auto       walkMode() const { return wlkMode; }
    void       tick(uint64_t dt);
    void       tickAnimationTags();
    bool       startClimb(JumpStatus jump);

    auto       world() -> World&;

    float      translateY() const;
    float      centerY() const;
    Npc*       lookAtTarget() const;
    auto       portalName() -> std::string_view;
    auto       formerPortalName() -> std::string_view;

    float      qDistTo(float x,float y,float z) const;
    float      qDistTo(const WayPoint* p) const;
    float      qDistTo(const Npc& p) const;
    float      qDistTo(const Interactive& p) const;

    void       updateAnimation(uint64_t dt);
    void       updateTransform();

    std::string_view displayName() const;
    auto       displayPosition() const -> Tempest::Vec3;
    void       setVisual    (std::string_view visual);
    void       setVisual    (const Skeleton*  visual);
    bool       hasOverlay   (std::string_view sk) const;
    bool       hasOverlay   (const Skeleton*  sk) const;
    void       addOverlay   (std::string_view sk, uint64_t time);
    void       addOverlay   (const Skeleton*  sk, uint64_t time);
    void       delOverlay   (std::string_view sk);
    void       delOverlay   (const Skeleton*  sk);

    bool       toogleTorch();
    bool       isUsingTorch() const;

    void       setVisualBody (int32_t headTexNr,int32_t teethTexNr,
                              int32_t bodyVer,int32_t bodyColor,
                              const std::string& body,const std::string& head);
    void       updateArmour  ();
    void       setSword      (MeshObjects::Mesh&& sword);
    void       setRangeWeapon(MeshObjects::Mesh&& bow);
    void       setMagicWeapon(Effect&& spell);
    void       setSlotItem   (MeshObjects::Mesh&& itm, std::string_view slot);
    void       setStateItem  (MeshObjects::Mesh&& itm, std::string_view slot);
    void       setAmmoItem   (MeshObjects::Mesh&& itm, std::string_view slot);
    void       clearSlotItem (std::string_view slot);
    void       setPhysic     (DynamicWorld::NpcItem&& item);
    void       setFatness    (float f);
    void       setScale      (float x,float y,float z);

    bool       setAnim(Anim a);
    auto       setAnimAngGet(Anim a) -> const Animation::Sequence*;
    auto       setAnimAngGet(Anim a, uint8_t comb) -> const Animation::Sequence*;
    void       setAnimRotate(int rot);
    bool       setAnimItem(std::string_view scheme, int state);
    void       stopAnim(std::string_view ani);
    void       startFaceAnim(std::string_view anim, float intensity, uint64_t duration);
    bool       stopItemStateAnim();
    bool       hasAnim(std::string_view scheme) const;
    bool       isFinishingMove() const;

    auto       animMoveSpeed(uint64_t dt) const -> Tempest::Vec3;

    bool       isJumpAnim() const;
    bool       isFlyAnim() const;
    bool       isFaling() const;
    bool       isSlide() const;
    bool       isInAir() const;
    bool       isStanding() const;
    bool       isSwim() const;
    bool       isDive() const;
    bool       isCasting() const;

    void       setTalentSkill(Talent t,int32_t lvl);
    int32_t    talentSkill(Talent t) const;

    void       setTalentValue(Talent t,int32_t lvl);
    int32_t    talentValue(Talent t) const;
    int32_t    hitChanse(Talent t) const;

    void       setRefuseTalk(uint64_t milis);
    bool       isRefuseTalk() const;

    int32_t    mageCycle() const;
    bool       canSneak() const;
    int32_t    attribute (Attribute a) const;
    void       changeAttribute(Attribute a, int32_t val, bool allowUnconscious);
    int32_t    protection(Protection p) const;
    void       changeProtection(Protection p, int32_t val);

    uint32_t   instanceSymbol() const;
    uint32_t   guild() const;
    bool       isMonster() const;
    void       setTrueGuild(int32_t g);
    int32_t    trueGuild() const;
    int32_t    magicCyrcle() const;
    int32_t    level() const;
    int32_t    experience() const;
    int32_t    experienceNext() const;
    int32_t    learningPoints() const;
    int32_t    diveTime() const;

    void      setAttitude(Attitude att);
    Attitude  attitude() const { return permAttitude; }

    bool      isFriend() const;

    void      setTempAttitude(Attitude att);
    Attitude  tempAttitude() const { return tmpAttitude; }

    void      startDialog(Npc& other);
    bool      startState(ScriptFn id, const Daedalus::ZString& wp);
    bool      startState(ScriptFn id, const Daedalus::ZString& wp, gtime endTime, bool noFinalize);
    void      clearState(bool noFinalize);
    BodyState bodyState() const;
    BodyState bodyStateMasked() const;

    void      setToFightMode(const size_t item);
    void      setToFistMode();

    void      aiPush(AiQueue::AiAction&& a);

    bool      canSwitchWeapon() const;
    bool      closeWeapon(bool noAnim);
    bool      drawWeaponFist();
    bool      drawWeaponMele();
    bool      drawWeaponBow();
    bool      drawMage(uint8_t slot);
    bool      drawSpell(int32_t spell);
    auto      weaponState() const -> WeaponState;

    bool      canFinish(Npc &oth);
    void      fistShoot();
    bool      blockFist();
    bool      finishingMove();
    void      swingSword();
    bool      swingSwordL();
    bool      swingSwordR();
    bool      blockSword();
    bool      beginCastSpell();
    void      endCastSpell();
    void      setActiveSpellInfo(int32_t info);
    int32_t   activeSpellLevel() const;
    bool      castSpell();
    bool      aimBow();
    bool      shootBow(Interactive* focOverride = nullptr);
    bool      hasAmunition() const;

    bool      isEnemy(const Npc& other) const;
    bool      isDead() const;
    bool      isUnconscious() const;
    bool      isDown() const;
    bool      isAtack() const;
    bool      isTalk() const;

    bool      isAtackAnim() const;
    bool      isPrehit() const;
    bool      isImmortal() const;

    void      setPerceptionTime   (uint64_t time);
    void      setPerceptionEnable (PercType t, size_t fn);
    void      setPerceptionDisable(PercType t);

    bool      perceptionProcess(Npc& pl);
    bool      perceptionProcess(Npc& pl, Npc *victum, float quadDist, PercType perc);
    bool      hasPerc(PercType perc) const;
    uint64_t  percNextTime() const;

    auto      interactive() const -> Interactive* { return currentInteract; }
    auto      detectedMob() const -> Interactive*;
    bool      setInteraction(Interactive* id, bool quick=false);
    void      quitIneraction();

    bool      isState   (ScriptFn stateFn) const;
    bool      isRoutine (ScriptFn stateFn) const;
    bool      wasInState(ScriptFn stateFn) const;
    uint64_t  stateTime() const;
    void      setStateTime(int64_t time);

    void      addRoutine(gtime s, gtime e, uint32_t callback, const WayPoint* point);
    void      excRoutine(size_t callback);
    void      multSpeed(float s);

    bool      testMove(const Tempest::Vec3& pos);
    bool      tryMove(const Tempest::Vec3& dp);
    bool      tryMove     (const Tempest::Vec3& dp, DynamicWorld::CollisionTest& out);
    bool      tryTranslate(const Tempest::Vec3& to);
    bool      tryTranslate(const Tempest::Vec3&         to,
                           DynamicWorld::CollisionTest& out);

    JumpStatus tryJump();
    bool      hasCollision() const { return physic.hasCollision(); }

    void      startDive();
    void      transformBack();

    auto      dialogChoises(Npc &player, const std::vector<uint32_t> &except, bool includeImp) -> std::vector<GameScript::DlgChoise>;

    auto      handle() -> Daedalus::GEngineClasses::C_Npc* { return  &hnpc; }

    auto      inventory() const -> const Inventory& { return invent; }
    size_t    hasItem    (size_t id) const;
    Item*     getItem    (size_t id);
    Item*     addItem    (size_t id, size_t amount);
    Item*     addItem    (std::unique_ptr<Item>&& i);
    Item*     takeItem   (Item& i);
    void      onWldItemRemoved(const Item& itm);
    void      delItem    (size_t id, uint32_t amount);
    void      useItem    (size_t item, bool force=false);
    void      setCurrentItem(size_t item);
    void      unequipItem(size_t item);
    void      addItem    (size_t id, Interactive& chest, size_t count=1);
    void      addItem    (size_t id, Npc& from,          size_t count=1);
    void      moveItem   (size_t id, Interactive& to,    size_t count=1);
    void      sellItem   (size_t id, Npc& to,            size_t count=1);
    void      buyItem    (size_t id, Npc& from,          size_t count=1);
    void      dropItem   (size_t id,                     size_t count=1);
    void      clearInventory();
    Item*     currentArmour();
    Item*     currentMeleWeapon();
    Item*     currentRangeWeapon();
    auto      mapWeaponBone() const -> Tempest::Vec3;
    auto      mapBone(std::string_view bone) const -> Tempest::Vec3;

    bool      turnTo  (float dx, float dz, bool anim, uint64_t dt);
    bool      rotateTo(float dx, float dz, float speed, bool anim, uint64_t dt);
    auto      playAnimByName(std::string_view name, BodyState bs) -> const Animation::Sequence*;

    bool      checkGoToNpcdistance(const Npc& other);

    bool      isAiQueueEmpty() const;
    bool      isAiBusy() const;
    void      clearAiQueue();

    bool      isInState(ScriptFn fn) const;

    auto      currentWayPoint() const -> const WayPoint* { return currentFp; }
    void      attachToPoint(const WayPoint* p);
    GoToHint  moveHint() const { return go2.flag; }
    void      clearGoTo();
    void      stopWalking();

    bool      canSeeNpc(const Npc& oth,bool freeLos) const;
    bool      canSeeNpc(float x,float y,float z,bool freeLos) const;
    auto      canSenseNpc(const Npc& oth,bool freeLos, float extRange=0.f) const -> SensesBit;
    auto      canSenseNpc(float x,float y,float z,bool freeLos,bool isNoisy,float extRange=0.f) const -> SensesBit;

    void      setTarget(Npc* t);
    Npc*      target() const;

    void      clearNearestEnemy();
    int32_t   lastHitSpellId() const { return lastHitSpell; }

    void      setOther(Npc* ot);
    void      setVictum(Npc* ot);

    bool      haveOutput() const;
    void      setAiOutputBarrier(uint64_t dt, bool overlay);

    bool      doAttack(Anim anim);
    void      commitSpell();
    void      takeDamage(Npc& other, const Bullet* b);
    void      takeDamage(Npc& other, const Bullet* b, const VisualFx* vfx, int32_t splId);

    void      emitSoundEffect(std::string_view sound, float range, bool freeSlot);
    void      emitSoundGround(std::string_view sound, float range, bool freeSlot);
    void      emitSoundSVM   (std::string_view sound);

    void      startEffect(Npc& to, const VisualFx& vfx);
    void      stopEffect (const VisualFx& vfx);
    void      runEffect  (Effect&& e);

  private:
    struct Routine final {
      gtime           start;
      gtime           end;
      ScriptFn        callback;
      const WayPoint* point=nullptr;
      };

    enum TransformBit : uint8_t {
      TR_Pos  =1,
      TR_Rot  =1<<1,
      TR_Scale=1<<2,
      };

    struct AiState final {
      ScriptFn funcIni;
      ScriptFn funcLoop;
      ScriptFn funcEnd;
      uint64_t sTime   =0;
      gtime    eTime   =gtime::endOfTime();
      bool     started =false;
      uint64_t loopNextTime=0;
      const char* hint="";
      };

    struct Perc final {
      ScriptFn func;
      };

    struct GoTo final {
      GoToHint         flag = GoToHint::GT_No;
      Npc*             npc  = nullptr;
      const WayPoint*  wp   = nullptr;
      Tempest::Vec3    pos  = {};

      void             save(Serialize& fout) const;
      void             load(Serialize&  fin);
      Tempest::Vec3    target() const;

      bool             empty() const;
      void             clear();
      void             set(Npc* to, GoToHint hnt = GoToHint::GT_Way);
      void             set(const WayPoint* to, GoToHint hnt = GoToHint::GT_Way);
      void             set(const Item* to);
      void             set(const Tempest::Vec3& to);
      void             setFlee();
      };

    void      updateWeaponSkeleton();
    void      tickTimedEvt(Animation::EvCount &ev);
    void      tickRegen(int32_t& v,const int32_t max,const int32_t chg, const uint64_t dt);
    void      updatePos();
    void      setViewPosition(const Tempest::Vec3& pos);
    bool      tickCast();

    int       aiOutputOrderId() const;
    bool      performOutput(const AiQueue::AiAction &ai);

    auto      currentRoutine() const -> const Routine&;
    gtime     endTime(const Routine& r) const;

    bool      implPointAt(const Tempest::Vec3& to);
    bool      implLookAt (uint64_t dt);
    bool      implLookAt (float dx, float dy, float dz, uint64_t dt);
    bool      implTurnTo (const Npc& oth, uint64_t dt);
    bool      implTurnTo (const Npc& oth, bool noAnim, uint64_t dt);
    bool      implTurnTo (float dx, float dz, bool noAnim, uint64_t dt);
    bool      implGoTo   (uint64_t dt);
    bool      implGoTo   (uint64_t dt, float destDist);
    bool      implAtack  (uint64_t dt);
    void      adjustAtackRotation(uint64_t dt);
    bool      implAiTick (uint64_t dt);
    void      implAiWait (uint64_t dt);
    void      implAniWait(uint64_t dt);
    void      implFaiWait(uint64_t dt);
    void      implSetFightMode(const Animation::EvCount& ev);
    bool      implAiFlee(uint64_t dt);

    void      tickRoutine();
    void      nextAiAction(AiQueue& queue, uint64_t dt);
    void      commitDamage();
    Npc*      updateNearestEnemy();
    Npc*      updateNearestBody();
    bool      checkHealth(bool onChange, bool forceKill);
    void      onNoHealth(bool death, HitSound sndMask);
    bool      hasAutoroll() const;
    void      stopWalkAnimation();
    void      takeDamage(Npc& other, const Bullet* b, const CollideMask bMask, int32_t splId, bool isSpell);

    void      dropTorch(bool burnout = false);

    void      saveAiState(Serialize& fout) const;
    void      loadAiState(Serialize& fin);
    static float angleDir(float x,float z);

    uint8_t   calcAniComb() const;

    bool               isAlignedToGround() const;
    Tempest::Vec3      groundNormal() const;
    Tempest::Matrix4x4 mkPositionMatrix() const;

    World&                         owner;
    Daedalus::GEngineClasses::C_Npc hnpc={};
    float                          x=0.f;
    float                          y=0.f;
    float                          z=0.f;
    float                          angle    = 0.f;
    float                          angleY   = 0.f;
    float                          runAng   = 0.f;
    float                          sz[3]={1.f,1.f,1.f};

    // visual props (cache)
    uint8_t                        durtyTranform=0;
    Tempest::Vec3                  lastGroundNormal;

    // visual props
    std::string                    body,head;
    int32_t                        vHead=0, vTeeth=0, vColor=0;
    int32_t                        bdColor=0;
    MdlVisual                      visual;

    DynamicWorld::NpcItem          physic;

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
    Npc*                           lastHit          = nullptr;
    char                           lastHitType      = 'A';
    int32_t                        lastHitSpell     = 0;

    // spell cast
    CastState                      castLevel        = CS_NoCast;
    size_t                         currentSpellCast = size_t(-1);
    uint64_t                       castNextTime     = 0;
    int32_t                        spellInfo        = 0;

    // transform-backshape
    struct TransformBack;
    std::unique_ptr<TransformBack> transformSpl;

    // ai state
    uint64_t                       aniWaitTime=0;
    uint64_t                       waitTime=0;
    uint64_t                       faiWaitTime=0;
    uint64_t                       outWaitTime=0;

    uint64_t                       aiOutputBarrier=0;
    ProcessPolicy                  aiPolicy=ProcessPolicy::AiNormal;
    AiState                        aiState;
    ScriptFn                       aiPrevState;
    AiQueue                        aiQueue;
    AiQueue                        aiQueueOverlay;
    std::vector<Routine>           routines;

    Interactive*                   currentInteract=nullptr;
    Npc*                           currentOther   =nullptr;
    Npc*                           currentVictum  =nullptr;

    Npc*                           currentLookAt  =nullptr;
    Npc*                           currentTarget  =nullptr;
    Npc*                           nearestEnemy   =nullptr;
    AiOuputPipe*                   outputPipe     =nullptr;

    Tempest::Vec3                  moveMobCacheKey={std::numeric_limits<float>::infinity(),0.f,0.f};
    Interactive*                   moveMob        =nullptr;

    GoTo                           go2;
    const WayPoint*                currentFp      =nullptr;
    FpLock                         currentFpLock;
    WayPath                        wayPath;

    MoveAlgo                       mvAlgo;
    FightAlgo                      fghAlgo;
    uint64_t                       lastEventTime=0;

    Sound                          sfxWeapon;

  friend class MoveAlgo;
  };

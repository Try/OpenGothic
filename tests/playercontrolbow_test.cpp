#include "game/playercontrol.h"
#include "game/gamepadplayerinput.h"

#include <gtest/gtest.h>
#include <Tempest/Vec>

#include <array>

// The input handlers, full PlayerControl::implMove and Npc aim/shoot/ammunition
// bodies below are generated from production sources. Only the surrounding
// world and animation readiness are controlled; no routing/shot code is copied.
class DialogMenu { public: bool isActive() const { return false; } };
class InventoryMenu {};
class Interactive {
  public:
    bool needToLockpick(const Npc&) const { return false; }
    bool isLadder() const { return false; }
    Tempest::Vec3 position() const { return {}; }
};
class Item {
  public:
    static constexpr uint8_t NSLOT=255;
    struct Handle { int32_t munition=1; } data;
    const Handle& handle() const { return data; }
    bool isCrossbow() const { return crossbow; }
    std::string_view description() const { return "spell"; }
    bool crossbow=false;
};
class Inventory {
  public:
    Item weapon, arrow;
    int ammunition=5;
    Item* activeWeapon() { return &weapon; }
    const Item* activeWeapon() const { return &weapon; }
    Item* getItem(size_t id) { return id==1 && ammunition>0 ? &arrow : nullptr; }
    int itemCount(size_t id) const { return id==1 ? ammunition : 0; }
    void delItem(size_t, int count, Npc&) { ammunition-=count; }
    uint8_t currentSpellSlot() const { return 255; }
    Item* currentSpell(uint8_t) const { return nullptr; }
};
class Bullet {
  public:
    void setOrigin(Npc*) {}
    void setDamage(int) {}
    void setHitChance(float) {}
    void setCritChance(float) {}
};
class DamageCalculator { public: static int rangeDamageValue(const Npc&) { return 1; } };
struct MoveAlgo {
  struct JumpStatus { int anim=0; float height=0; };
};
class Script {
  public:
    struct GuildValues { float jumpup_height[GIL_MAX]={}; } guild;
    const GuildValues& guildVal() const { return guild; }
    void playerHotKeyScreenMap(Npc&) {}
    void playerHotLameHeal(Npc&) {}
    void playerHotLamePotion(Npc&) {}
};
struct ScriptFn {};
class World;
class Npc {
  public:
    enum Anim { Idle, NoAnim, AimBow, Attack, Move, MoveL, MoveR, MoveBack, Jump, JumpUp };
    enum BeginCastResult { BC_No, BC_NoMana, BC_Cast, BC_Invest };
    enum class Animation { Idle, AimTransition, Ready, ShotLoop, Reload } animation=Animation::Idle;
    struct Attributes { std::array<int,8> attribute={}; std::array<int,5> hitchance={}; } attributes;
    Attributes* hnpc=&attributes;
    int talentsVl[TALENT_MAX_G2]={};
    struct Visual { void setAnimRotate(Npc&, int) {} } visual;
    World& owner;
    Npc* currentTarget=nullptr;
    Npc(World& world):owner(world) {}
    Inventory invent;
    WeaponState weapon=WeaponState::Bow;
    BodyState body=BS_STAND;
    int aimDelay=3, remaining=0, casts=0, melee=0, moves=0;
    int attackRequests=0, idleRequests=0, reloadRequests=0;
    bool aiBusy=false;
    WeaponState weaponState() const { return weapon; }
    BodyState bodyStateMasked() const { return body; }
    Inventory& inventory() { return invent; }
    Interactive* interactive() const { return nullptr; }
    Item* currentRangedWeapon() { return &invent.weapon; }
    Item* currentMeleeWeapon() { return nullptr; }
    Npc* target() const { return nullptr; }
    bool canFinish(const Npc&) const { return false; }
    bool isFinishingMove() const { return false; }
    bool hasState(int state) const { return (int(body)&state)!=0; }
    bool isAttackAnim() const { return animation==Animation::Reload; }
    bool canSwitchWeapon() const { return body==BS_STAND; }
    bool closeWeapon(bool) { weapon=WeaponState::NoWeapon; return true; }
    bool drawWeaponMelee() { weapon=WeaponState::W1H; return true; }
    bool drawWeaponFist() { weapon=WeaponState::Fist; return true; }
    bool drawWeaponBow() { weapon=WeaponState::Bow; return true; }
    bool drawMage(uint8_t) { weapon=WeaponState::Mage; return true; }
    bool isMonster() const { return false; }
    bool isInState(ScriptFn) const { return true; }
    bool isRotationAllowed() const { return true; }
    bool isAiQueueEmpty() const { return !aiBusy; }
    float rotation() const { return 0; }
    float rotationY() const { return 0; }
    Tempest::Vec3 position() const { return {}; }
    Tempest::Vec3 centerPosition() const { return {}; }
    void turnTo(float,float,bool,uint64_t) {}
    void setAnimRotate(int) {}
    void setDirectionY(float) {}
    void setDirection(float) {}
    bool isFalling() const { return false; }
    bool isSlide() const { return false; }
    bool isInAir() const { return false; }
    bool isJump() const { return false; }
    bool isJumpUp() const { return false; }
    bool isDive() const { return false; }
    bool isSwim() const { return false; }
    bool isInWater() const { return false; }
    bool isStanding() const { return body==BS_STAND; }
    bool isCasting() const { return false; }
    WalkBit walkMode() const { return WalkBit(0); }
    Guild guild() const { return GIL_HUMAN; }
    MoveAlgo::JumpStatus tryJump() const { return {}; }
    void startClimb(MoveAlgo::JumpStatus) {}
    void startDive() {}
    void transformBack() {}
    int attribute(Attribute) const { return 10; }
    BeginCastResult beginCastSpell() { ++casts; return BC_Cast; }
    void endCastSpell(bool) {}
    void fistShoot() { ++melee; }
    void swingSword() { ++melee; }
    bool swingSwordL() { ++melee; return true; }
    bool swingSwordR() { ++melee; return true; }
    void blockFist() {}
    bool blockSword() { return true; }
    void finishingMove() {}
    bool setAnim(Anim anim) {
      if(anim==Anim::AimBow) {
        if(animation==Animation::Idle) {
          animation=Animation::AimTransition;
          remaining=aimDelay;
          body=BS_AIMNEAR;
        }
        if(animation==Animation::ShotLoop) {
          ++reloadRequests;
          animation=Animation::Reload;
          remaining=aimDelay*3;
          body=BS_HIT;
        }
        return true;
      }
      if(anim==Anim::Attack) {
        ++attackRequests;
        if(animation!=Animation::Ready)
          return false;
        animation=Animation::ShotLoop;
        remaining=0;
        body=BS_HIT;
        return true;
      }
      if(anim==Anim::Idle) {
        ++idleRequests;
        animation=Animation::Idle;
        body=BS_STAND;
      }
      if(anim==Anim::Move)
        ++moves;
      return true;
    }
    void advanceAnimation() {
      if((animation==Animation::AimTransition || animation==Animation::Reload) && --remaining<=0) {
        animation=Animation::Ready;
        body=BS_AIMNEAR;
      }
    }
    bool aimBow();
    bool shootBow(Interactive*);
    bool hasAmmunition() const;
};
class World {
  public:
    Npc npc{*this};
    Script scripts;
    Bullet bullet;
    int shots=0;
    bool isCutsceneLock() const { return false; }
    Npc* player() { return &npc; }
    Script& script() { return scripts; }
    Bullet& shootBullet(Item&,Npc&,Npc*,Interactive*) { ++shots; return bullet; }
};
class Camera {
  public:
    bool isFree() const { return false; }
    bool isFirstPerson() const { return false; }
    void setFirstPerson(bool) {}
};
class Gothic {
  public:
    static Gothic& inst() { static Gothic game; return game; }
    World* world() { return &w; }
    Camera* camera() { return &c; }
    bool isMarvinEnabled() const { return false; }
    struct Version { int game=2; } versionInfo;
    const Version& version() const { return versionInfo; }
    void onPrint(std::string_view) {}
    World w;
    Camera c;
};
PlayerControl::PlayerControl(DialogMenu& dialog, InventoryMenu& inventory):dlg(dialog),inv(inventory) {}
PlayerControl::~PlayerControl() = default;
void PlayerControl::toggleWalkMode() {}
void PlayerControl::toggleSneakMode() {}
void PlayerControl::marvinO() {}
void PlayerControl::processPickLock(Npc&,Interactive&,KeyCodec::Action) {}
void PlayerControl::implMoveMobsi(Npc&,uint64_t) {}
void PlayerControl::moveFocus(FocusAction) {}
void PlayerControl::setAnimRotate(Npc&,float,int,bool,uint64_t) {}
void PlayerControl::processAutoRotate(Npc&,float&,uint64_t) {}
void PlayerControl::assignRunAngle(Npc&,float,uint64_t) {}

#include "playercontrol-bow-bodies.inc"

namespace {
using Source=PlayerControl::InputSource;
constexpr auto primary=KeyCodec::Mapping::Primary;
constexpr auto pad=PlayerControl::InputState::owner(Source::Gamepad,false);
class PlayerControlBow : public testing::Test {
  protected:
    DialogMenu dialog;
    InventoryMenu inventory;
    PlayerControl control{dialog,inventory};
    World& world=Gothic::inst().w;
    Npc& npc=world.npc;
    void SetUp() override { reset(); }
    void reset(bool g2=false, WeaponState weapon=WeaponState::Bow) {
      control.clearInput();
      control.g2Ctrl=g2;
      world.shots=0;
      npc.invent.ammunition=5;
      npc.weapon=weapon;
      npc.body=BS_STAND;
      npc.animation=Npc::Animation::Idle;
      npc.remaining=0;
      npc.aimDelay=3;
      npc.casts=npc.melee=npc.moves=npc.attackRequests=npc.idleRequests=npc.reloadRequests=0;
      npc.aiBusy=false;
    }
    void down(KeyCodec::Action action,Source source=Source::Gamepad,KeyCodec::Mapping mapping=primary) {
      control.onKeyPressed(action,Tempest::Event::K_NoKey,mapping,source);
    }
    void up(KeyCodec::Action action,Source source=Source::Gamepad,KeyCodec::Mapping mapping=primary) {
      control.onKeyReleased(action,mapping,source);
    }
    void tick(unsigned count=1) {
      while(count-->0) {
        control.implMove(16,0,0);
        npc.advanceAnimation();
      }
    }
};
}

TEST_F(PlayerControlBow, DirectG1ShotWaitsForActualAimAndEmitsExactlyOncePerHold) {
  for(auto weapon:{WeaponState::Bow,WeaponState::CBow})
    for(int delay:{1,3,10}) {
      reset(false,weapon);
      npc.aimDelay=delay;
      down(KeyCodec::ActionGeneric);
      tick(1);
      EXPECT_EQ(world.shots,0);
      EXPECT_TRUE(control.actrl[0].hasOrigin(pad));
      tick(40);
      EXPECT_EQ(world.shots,1);
      EXPECT_EQ(npc.invent.ammunition,4);
      EXPECT_FALSE(control.actrl[0].hasOrigin(pad));
      EXPECT_EQ(npc.moves,0);
      up(KeyCodec::ActionGeneric);
      down(KeyCodec::ActionGeneric);
      tick(40);
      EXPECT_EQ(world.shots,2);
      EXPECT_EQ(npc.invent.ammunition,3);
    }
}

TEST_F(PlayerControlBow, ReleaseOrSourceCancelBeforeAimReadyPreventsLateShot) {
  for(bool cancel:{false,true}) {
    reset();
    npc.aimDelay=10;
    down(KeyCodec::ActionGeneric);
    tick();
    if(cancel)
      control.cancelInput(Source::Gamepad);
    else
      up(KeyCodec::ActionGeneric);
    tick(30);
    EXPECT_EQ(world.shots,0);
    EXPECT_EQ(npc.invent.ammunition,5);
    EXPECT_FALSE(control.actrl[0]);
  }
}

TEST_F(PlayerControlBow, DirectionDoesNotTurnThePendingDirectShotIntoCombatMove) {
  reset();
  down(KeyCodec::Forward);
  down(KeyCodec::ActionGeneric);
  EXPECT_TRUE(control.actrl[0]);
  EXPECT_FALSE(control.actrl[5]);
  tick(30);
  EXPECT_EQ(world.shots,1);
  EXPECT_EQ(npc.invent.ammunition,4);
  // The actual physical movement owner survives; the attack never synthesizes
  // ctrl[Forward] through ActMove while waiting for the animation.
  EXPECT_TRUE(control.input.held(KeyCodec::Forward));
  EXPECT_FALSE(control.ctrl[KeyCodec::Forward]);
}

TEST_F(PlayerControlBow, EmptyAmmunitionCannotLeaveADeferredShot) {
  reset();
  npc.invent.ammunition=0;
  down(KeyCodec::ActionGeneric);
  tick(5);
  npc.invent.ammunition=5;
  tick(30);
  EXPECT_EQ(world.shots,0);
  EXPECT_FALSE(control.actrl[0]);
}

TEST_F(PlayerControlBow, WeaponRequestAndContextResetCancelOnlyThePendingShot) {
  for(bool context:{false,true}) {
    reset();
    npc.aimDelay=10;
    down(KeyCodec::ActionGeneric);
    tick();
    if(context)
      control.clearInput();
    else
      down(KeyCodec::WeaponMele,Source::Keyboard);
    EXPECT_FALSE(control.actrl[0].hasOrigin(pad));
    tick(30);
    EXPECT_EQ(world.shots,0);
  }
}

TEST_F(PlayerControlBow, G2StillRepeatsWhileHeldAndMeleeAndMagicKeepTheirConsumption) {
  reset(true);
  down(KeyCodec::ActionGeneric);
  tick(40);
  EXPECT_GT(world.shots,1);
  reset(false,WeaponState::W1H);
  down(KeyCodec::ActionGeneric);
  tick(10);
  EXPECT_EQ(npc.melee,1);
  reset(false,WeaponState::Mage);
  down(KeyCodec::ActionGeneric);
  tick(10);
  EXPECT_EQ(npc.casts,1);
}

TEST_F(PlayerControlBow, NativeShootBooleanDoesNotProveProjectileInInvalidBodyState) {
  reset();
  npc.body=BS_RUN;
  EXPECT_TRUE(npc.shootBow(nullptr));
  EXPECT_EQ(world.shots,0);
  EXPECT_EQ(npc.invent.ammunition,5);
  npc.body=BS_RUN;
  down(KeyCodec::ActionGeneric);
  tick(40);
  EXPECT_EQ(world.shots,1);
  EXPECT_EQ(npc.invent.ammunition,4);
}

TEST_F(PlayerControlBow, EquivalentPadOwnersSharePendingWithoutReplayingAfterSuccess) {
  reset();
  npc.aimDelay=10;
  down(KeyCodec::ActionGeneric);
  tick();
  down(KeyCodec::ActionGeneric,Source::Gamepad,KeyCodec::Mapping::Secondary);
  up(KeyCodec::ActionGeneric);
  tick(40);
  EXPECT_EQ(world.shots,1);
  down(KeyCodec::ActionGeneric); // secondary is still held after successful shot
  tick(40);
  EXPECT_EQ(world.shots,1);
  up(KeyCodec::ActionGeneric);
  up(KeyCodec::ActionGeneric,Source::Gamepad,KeyCodec::Mapping::Secondary);
}

TEST_F(PlayerControlBow, MixedKeyboardCombinationDoesNotConsumePendingPadContribution) {
  reset();
  down(KeyCodec::ActionGeneric,Source::Keyboard);
  down(KeyCodec::Forward,Source::Keyboard);
  down(KeyCodec::ActionGeneric);
  tick();
  EXPECT_TRUE(control.actrl[0].hasOrigin(pad));
  up(KeyCodec::ActionGeneric,Source::Keyboard);
  up(KeyCodec::Forward,Source::Keyboard);
  tick(40);
  EXPECT_EQ(world.shots,1);
  EXPECT_EQ(npc.moves,0);
}

TEST_F(PlayerControlBow, LosingAmmoWhileAimingCannotRearmOnLaterRefill) {
  reset();
  npc.aimDelay=10;
  down(KeyCodec::ActionGeneric);
  tick();
  npc.invent.ammunition=0;
  tick();
  npc.invent.ammunition=5;
  tick(40);
  EXPECT_EQ(world.shots,0);
  EXPECT_FALSE(control.actrl[0]);
}

TEST_F(PlayerControlBow, HeldDirectG1ActionRunsNativeReloadWithoutAnotherShot) {
  for(auto weapon:{WeaponState::Bow,WeaponState::CBow}) {
    reset(false,weapon);
    npc.aimDelay=3;
    down(KeyCodec::ActionGeneric);
    tick(4);
    ASSERT_EQ(world.shots,1);
    EXPECT_EQ(npc.animation,Npc::Animation::ShotLoop);
    tick();
    EXPECT_EQ(npc.reloadRequests,1);
    EXPECT_EQ(npc.animation,Npc::Animation::Reload);
    tick(20);
    EXPECT_EQ(npc.animation,Npc::Animation::Ready);
    EXPECT_EQ(world.shots,1);
    EXPECT_EQ(npc.invent.ammunition,4);
    up(KeyCodec::ActionGeneric);
  }
}

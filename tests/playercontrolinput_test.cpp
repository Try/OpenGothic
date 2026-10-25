
#include "game/playercontrol.h"
#include "game/gamepadplayerinput.h"
#include <gtest/gtest.h>
#include <string_view>

// Only the world boundary is stubbed. The PlayerControl layout, ActionInput,
// GamepadPlayerInput, onKeyPressed/onKeyReleased/handleMovementAction bodies
// below are read directly from the current production source without edits.
class DialogMenu {};
class InventoryMenu {};
class Item { public: static constexpr uint8_t NSLOT=255; };
class Inventory {
 public:
  uint8_t currentSpellSlot() const { return 255; }
  Item* currentSpell(uint8_t) const { return nullptr; }
};
class Interactive {
 public:
  bool picklock=false, ladder=false;
  bool needToLockpick(const Npc&) const { return picklock; }
  bool isLadder() const { return ladder; }
};
class Npc {
 public:
  WeaponState weapon=WeaponState::W1H;
  Interactive* interaction=nullptr;
  WeaponState weaponState() const { return weapon; }
  Inventory& inventory() { return inv; }
  Interactive* interactive() const { return interaction; }
  Item* currentRangedWeapon() const { return nullptr; }
  Npc* target() const { return nullptr; }
  bool canFinish(const Npc&) const { return false; }
  bool isAttackAnim() const { return false; }
  bool hasState(int) const { return false; }
 private: Inventory inv;
};
class Camera {
 public:
  bool isFree() const { return false; }
  bool isFirstPerson() const { return false; }
  void setFirstPerson(bool) {}
};
class Script {
 public:
  int maps=0, heals=0, potions=0;
  void playerHotKeyScreenMap(Npc&) { ++maps; }
  void playerHotLameHeal(Npc&) { ++heals; }
  void playerHotLamePotion(Npc&) { ++potions; }
};
class World {
 public:
  bool isCutsceneLock() const { return false; }
  Npc* player() { return &npc; }
  Script& script() { return scripts; }
 private: Npc npc; Script scripts;
};
class Gothic {
 public:
  static Gothic& inst() { static Gothic value; return value; }
  World* world() { return &w; }
  Camera* camera() { return &c; }
  bool isMarvinEnabled() const { return false; }
 public: World w; Camera c;
  int walks=0, sneaks=0, picks=0;
};
PlayerControl::PlayerControl(DialogMenu& dlg, InventoryMenu& inv):dlg(dlg),inv(inv) {}
PlayerControl::~PlayerControl() = default;
void PlayerControl::toggleWalkMode() { ++Gothic::inst().walks; }
void PlayerControl::toggleSneakMode() { ++Gothic::inst().sneaks; }
void PlayerControl::marvinO() {}
void PlayerControl::processPickLock(Npc&, Interactive&, KeyCodec::Action) { ++Gothic::inst().picks; }

// Generated from exact production source ranges by CMake. No input algorithm is
// reproduced in this fixture; only the game-world boundary above is replaced.
#include "playercontrol-input-bodies.inc"

namespace {
using Source = PlayerControl::InputSource;
using State = PlayerControl::InputState;
using InputAction = KeyCodec::Action;
constexpr auto primary = KeyCodec::Mapping::Primary;
constexpr auto secondary = KeyCodec::Mapping::Secondary;
constexpr auto key = Tempest::Event::K_NoKey;
constexpr auto keyboard = State::owner(Source::Keyboard, false);
constexpr auto pad = State::owner(Source::Gamepad, false);

class PlayerControlInput : public testing::Test {
 protected:
  DialogMenu dialog;
  InventoryMenu inventory;
  PlayerControl control{dialog, inventory};
  void SetUp() override { reset(); }
  void reset(bool g2=false, WeaponState weapon=WeaponState::W1H) {
    control.clearInput();
    control.g2Ctrl=g2;
    auto& game=Gothic::inst();
    game.w=World{};
    game.w.player()->weapon=weapon;
    game.walks=game.sneaks=game.picks=0;
  }
  void down(InputAction action, Source source, KeyCodec::Mapping mapping=primary) {
    control.onKeyPressed(action,key,mapping,source);
  }
  void up(InputAction action, Source source, KeyCodec::Mapping mapping=primary) {
    control.onKeyReleased(action,mapping,source);
  }
};
}

TEST_F(PlayerControlInput, G1DirectButtonsHaveIndependentKeyboardRolesInEitherOrder) {
  for(auto weapon : {WeaponState::Fist, WeaponState::W1H, WeaponState::W2H})
    for(auto action : {KeyCodec::ActionGeneric, KeyCodec::Parade, KeyCodec::ActionLeft, KeyCodec::ActionRight})
      for(bool padFirst : {false,true})
        for(bool padUpFirst : {false,true}) {
          const int combat=GamepadPlayerInput::combat(action,weapon,false,false,false);
          if(combat<0)
            continue;
          SCOPED_TRACE(testing::Message()<<int(weapon)<<'/'<<int(action)<<'/'<<padFirst<<'/'<<padUpFirst);
          reset(false,weapon);
          down(action,padFirst?Source::Gamepad:Source::Keyboard);
          down(action,padFirst?Source::Keyboard:Source::Gamepad);
          ASSERT_TRUE(control.actrl[combat]);
          EXPECT_TRUE(control.actrl[combat].hasOrigin(pad));
          EXPECT_FALSE(control.actrl[combat].hasOrigin(keyboard));
          up(action,padUpFirst?Source::Gamepad:Source::Keyboard);
          EXPECT_EQ(bool(control.actrl[combat]), !padUpFirst);
          up(action,padUpFirst?Source::Keyboard:Source::Gamepad);
          EXPECT_FALSE(control.actrl[combat]);
        }
}

TEST_F(PlayerControlInput, G1DirectionsDoNotTransferCombinationToPadMovement) {
  const InputAction actions[]={KeyCodec::Forward,KeyCodec::Back,KeyCodec::Left,KeyCodec::Right,KeyCodec::RotateL,KeyCodec::RotateR};
  const int combats[]={0,1,2,3,2,3};
  for(size_t i=0;i<6;++i)
    for(bool padFirst : {false,true})
      for(bool padUpFirst : {false,true}) {
        reset();
        down(KeyCodec::ActionGeneric,Source::Keyboard);
        down(actions[i],padFirst?Source::Gamepad:Source::Keyboard);
        down(actions[i],padFirst?Source::Keyboard:Source::Gamepad);
        ASSERT_TRUE(control.actrl[combats[i]]);
        EXPECT_TRUE(control.actrl[combats[i]].hasOrigin(keyboard));
        EXPECT_FALSE(control.actrl[combats[i]].hasOrigin(pad));
        up(actions[i],padUpFirst?Source::Gamepad:Source::Keyboard);
        EXPECT_EQ(bool(control.actrl[combats[i]]),padUpFirst);
        up(actions[i],padUpFirst?Source::Keyboard:Source::Gamepad);
        EXPECT_FALSE(control.actrl[combats[i]]);
      }
}

TEST_F(PlayerControlInput, IndependentDirectAttackDoesNotInheritCombinationDependency) {
  for(bool padFirst : {false,true})
    for(bool cancel : {false,true}) {
      reset();
      if(padFirst)
        down(KeyCodec::ActionGeneric,Source::Gamepad);
      down(KeyCodec::ActionGeneric,Source::Keyboard);
      down(KeyCodec::Forward,Source::Keyboard);
      if(!padFirst)
        down(KeyCodec::ActionGeneric,Source::Gamepad);
      // A pad press after Forward selects native ActMove, which independently
      // requires that direction. A prior pad press selects the direct attack.
      const int direct=padFirst?0:5;
      ASSERT_TRUE(control.actrl[direct].hasOrigin(pad));
      ASSERT_TRUE(control.actrl[0].hasOrigin(keyboard));
      if(cancel)
        control.cancelInput(Source::Keyboard);
      else
        up(KeyCodec::ActionGeneric,Source::Keyboard);
      EXPECT_EQ(bool(control.actrl[direct]),!cancel || padFirst);
      if(!cancel)
        up(KeyCodec::Forward,Source::Keyboard);
      EXPECT_EQ(bool(control.actrl[direct]),padFirst);
      EXPECT_FALSE(control.actrl[0].hasOrigin(keyboard));
      up(KeyCodec::ActionGeneric,Source::Gamepad);
      EXPECT_FALSE(control.actrl[direct]);
    }
}

TEST_F(PlayerControlInput, DirectSideAttackAndBlockSurviveLosingAnotherOwnersModifier) {
  const InputAction direction[]={KeyCodec::Back,KeyCodec::Left,KeyCodec::Right};
  const InputAction direct[]={KeyCodec::Parade,KeyCodec::ActionLeft,KeyCodec::ActionRight};
  for(size_t i=0;i<3;++i)
    for(bool padFirst : {false,true})
      for(bool cancel : {false,true}) {
        reset();
        if(padFirst)
          down(direct[i],Source::Gamepad);
        down(KeyCodec::ActionGeneric,Source::Keyboard);
        down(direction[i],Source::Keyboard);
        if(!padFirst)
          down(direct[i],Source::Gamepad);
        const int combat=int(i)+1;
        ASSERT_TRUE(control.actrl[combat].hasOrigin(keyboard));
        ASSERT_TRUE(control.actrl[combat].hasOrigin(pad));
        if(cancel)
          control.cancelInput(Source::Keyboard);
        else
          up(KeyCodec::ActionGeneric,Source::Keyboard);
        EXPECT_TRUE(control.actrl[combat]);
        EXPECT_TRUE(control.actrl[combat].hasOrigin(pad));
        EXPECT_FALSE(control.actrl[combat].hasOrigin(keyboard));
        up(direct[i],Source::Gamepad);
        EXPECT_FALSE(control.actrl[combat]);
      }
}

TEST_F(PlayerControlInput, PadGenericCanSupplyAndShareKeyboardCombinationModifier) {
  reset();
  down(KeyCodec::ActionGeneric,Source::Gamepad);
  down(KeyCodec::Left,Source::Keyboard);
  ASSERT_TRUE(control.actrl[2]);
  down(KeyCodec::ActionGeneric,Source::Keyboard);
  up(KeyCodec::ActionGeneric,Source::Gamepad);
  EXPECT_TRUE(control.actrl[2]);
  up(KeyCodec::ActionGeneric,Source::Keyboard);
  EXPECT_FALSE(control.actrl[2]);
}

TEST_F(PlayerControlInput, EquivalentOwnersSharePendingCommandsWithoutReplayingConsumedCommands) {
  for(bool g2 : {false,true})
    for(auto action : {KeyCodec::ActionGeneric,KeyCodec::Parade,KeyCodec::ActionLeft,KeyCodec::ActionRight})
      for(bool consumed : {false,true}) {
        reset(g2);
        down(action,Source::Gamepad);
        const int combat=GamepadPlayerInput::combat(action,WeaponState::W1H,false,false,false);
        ASSERT_TRUE(control.actrl[combat]);
        if(consumed)
          control.actrl[combat]=false;
        down(action,g2?Source::Keyboard:Source::Gamepad,secondary);
        EXPECT_EQ(bool(control.actrl[combat]),!consumed);
        up(action,Source::Gamepad);
        EXPECT_EQ(bool(control.actrl[combat]),!consumed);
      }
}

TEST_F(PlayerControlInput, ContextsAndOneShotActionsRetainRawDeduplication) {
  for(bool padFirst : {false,true}) {
    for(bool consumed : {false,true}) {
      reset(false,WeaponState::NoWeapon);
      down(KeyCodec::ActionGeneric,padFirst?Source::Gamepad:Source::Keyboard);
      if(consumed)
        control.actrl[4]=false;
      down(KeyCodec::ActionGeneric,padFirst?Source::Keyboard:Source::Gamepad);
      EXPECT_EQ(bool(control.actrl[4]),!consumed);
    }
    reset();
    down(KeyCodec::Walk,padFirst?Source::Gamepad:Source::Keyboard);
    down(KeyCodec::Walk,padFirst?Source::Keyboard:Source::Gamepad);
    EXPECT_EQ(Gothic::inst().walks,1);
    reset();
    down(KeyCodec::Weapon,padFirst?Source::Gamepad:Source::Keyboard);
    for(auto& command:control.wctrl)
      command=false;
    down(KeyCodec::Weapon,padFirst?Source::Keyboard:Source::Gamepad);
    for(auto& command:control.wctrl)
      EXPECT_FALSE(command);
    reset();
    Interactive inter;
    inter.picklock=true;
    Gothic::inst().w.player()->interaction=&inter;
    down(KeyCodec::Left,padFirst?Source::Gamepad:Source::Keyboard);
    down(KeyCodec::Left,padFirst?Source::Keyboard:Source::Gamepad);
    EXPECT_EQ(Gothic::inst().picks,1);
    reset();
    inter.picklock=false;
    inter.ladder=true;
    Gothic::inst().w.player()->interaction=&inter;
    down(KeyCodec::Forward,padFirst?Source::Gamepad:Source::Keyboard);
    control.ctrl[KeyCodec::Forward]=false;
    down(KeyCodec::Forward,padFirst?Source::Keyboard:Source::Gamepad);
    EXPECT_FALSE(control.ctrl[KeyCodec::Forward]);
  }
}

TEST_F(PlayerControlInput, ReleaseEffectsRemainSingleAndRawReturnKeepsInteractionBlockers) {
  for(auto action : {KeyCodec::Map,KeyCodec::Heal,KeyCodec::Potion}) {
    reset();
    down(action,Source::Keyboard);
    down(action,Source::Gamepad);
    up(action,Source::Keyboard);
    auto& script=Gothic::inst().w.script();
    EXPECT_EQ(script.maps+script.heals+script.potions,0);
    up(action,Source::Gamepad);
    EXPECT_EQ(script.maps+script.heals+script.potions,1);
  }
  for(bool interacting : {false,true}) {
    reset();
    Interactive ladder;
    ladder.ladder=true;
    if(interacting)
      Gothic::inst().w.player()->interaction=&ladder;
    down(KeyCodec::Forward,Source::Keyboard);
    control.onKeyPressed(KeyCodec::Forward,Tempest::Event::K_Return,secondary,Source::Keyboard);
    EXPECT_EQ(bool(control.ctrl[KeyCodec::K_ENTER]),!interacting);
  }
}

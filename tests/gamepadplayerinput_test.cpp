#include "game/gamepadplayerinput.h"
#include "utils/actioninput.h"

#include <gtest/gtest.h>

using PadInput = GamepadPlayerInput;

TEST(GamepadPlayerInput, AttackRetainsNativeChargeAndFinisherSelection) {
  for(auto weapon : {WeaponState::Fist, WeaponState::W1H, WeaponState::W2H,
                     WeaponState::Bow, WeaponState::CBow, WeaponState::Mage}) {
    EXPECT_EQ(PadInput::combat(KeyCodec::ActionGeneric,weapon,false,false,false), PadInput::Forward);
    EXPECT_EQ(PadInput::combat(KeyCodec::ActionGeneric,weapon,true,false,true), PadInput::Move);
    }
  EXPECT_EQ(PadInput::combat(KeyCodec::ActionGeneric,WeaponState::W1H,true,true,false), PadInput::Kill);
  EXPECT_EQ(PadInput::combat(KeyCodec::ActionGeneric,WeaponState::NoWeapon,true,false,false), PadInput::None);
  }

TEST(GamepadPlayerInput, ParryRequiresMeleeAndSideAttacksRespectChargeRun) {
  for(auto weapon : {WeaponState::NoWeapon, WeaponState::Fist, WeaponState::W1H,
                     WeaponState::W2H, WeaponState::Bow, WeaponState::CBow, WeaponState::Mage}) {
    const bool melee = weapon==WeaponState::Fist || weapon==WeaponState::W1H || weapon==WeaponState::W2H;
    EXPECT_EQ(PadInput::combat(KeyCodec::Parade,weapon,false,false,false), melee ? PadInput::Back : PadInput::None);
    EXPECT_EQ(PadInput::combat(KeyCodec::ActionLeft,weapon,false,false,true), PadInput::None);
    EXPECT_EQ(PadInput::combat(KeyCodec::ActionRight,weapon,false,false,true), PadInput::None);
    EXPECT_EQ(PadInput::combat(KeyCodec::ActionLeft,weapon,false,false,false), weapon==WeaponState::NoWeapon ? PadInput::None : PadInput::Left);
    EXPECT_EQ(PadInput::combat(KeyCodec::ActionRight,weapon,false,false,false), weapon==WeaponState::NoWeapon ? PadInput::None : PadInput::Right);
    }
  }

TEST(GamepadPlayerInput, MovementDoesNotBecomeDirectCombat) {
  for(auto action : {KeyCodec::Forward, KeyCodec::Back, KeyCodec::Left, KeyCodec::Right,
                     KeyCodec::RotateL, KeyCodec::RotateR, KeyCodec::Jump})
    EXPECT_EQ(PadInput::combat(action,WeaponState::W1H,true,false,false), PadInput::None);
  }

TEST(GamepadPlayerInput, EmptyAndSingleSpellDoNotSelectMissingSlot) {
  for(int slot=0; slot<256; ++slot) {
    EXPECT_EQ(PadInput::nextSpell(0,uint8_t(slot),false),0);
    EXPECT_EQ(PadInput::nextSpell(0,uint8_t(slot),true),0);
    for(int index=0; index<8; ++index) {
      EXPECT_EQ(PadInput::nextSpell(uint8_t(1u<<index),uint8_t(slot),false),index+3);
      EXPECT_EQ(PadInput::nextSpell(uint8_t(1u<<index),uint8_t(slot),true),index+3);
      }
    }
  }

TEST(GamepadPlayerInput, SpellCycleSkipsHolesAndWrapsBothWays) {
  constexpr uint8_t occupied = (1u<<0) | (1u<<3) | (1u<<7);
  uint8_t slot=3;
  for(auto expected : {6,10,3,6}) {
    slot=PadInput::nextSpell(occupied,slot,false);
    EXPECT_EQ(slot,expected);
    }
  for(auto expected : {3,10,6,3}) {
    slot=PadInput::nextSpell(occupied,slot,true);
    EXPECT_EQ(slot,expected);
    }
  EXPECT_EQ(PadInput::nextSpell(occupied,255,false),3);
  EXPECT_EQ(PadInput::nextSpell(occupied,255,true),10);
  EXPECT_EQ(PadInput::nextSpell(occupied,7,false),10);
  EXPECT_EQ(PadInput::nextSpell(occupied,7,true),6);
  }

namespace {
using WeaponInput = ActionInput<KeyCodec::Last, KeyCodec::Idle>;
constexpr auto padOwner = WeaponInput::owner(WeaponInput::Source::Gamepad, false);
constexpr auto padMask = WeaponInput::sourceMask(WeaponInput::Source::Gamepad);
constexpr auto keyOwner = WeaponInput::owner(WeaponInput::Source::Keyboard, false);
}

TEST(GamepadPlayerInput, LaterWeaponGroupReplacesPendingSpellAndItsCyclePivot) {
  for(size_t group=0; group<11; ++group) {
    WeaponInput::Command pending[11];
    PadInput::requestWeapon(pending, 6, KeyCodec::WeaponMageNext, padOwner, padMask);
    EXPECT_EQ(PadInput::pendingSpell(pending,3,padMask,255),6);
    pending[6] = !false; // canSwitchWeapon/draw unavailable: retain pending request
    PadInput::requestWeapon(pending, group, KeyCodec::WeaponMele, padOwner, padMask);
    for(size_t weapon=0; weapon<11; ++weapon)
      EXPECT_EQ(bool(pending[weapon]),weapon==group);
    EXPECT_EQ(PadInput::pendingSpell(pending,3,padMask,255),group>=3 ? group : 255);
    // Once the selected group is consumed, there is no second controller
    // request to draw an old spell on a later tick and no separate stale hint.
    pending[group] = false;
    for(auto& command : pending)
      EXPECT_FALSE(command);
    EXPECT_EQ(PadInput::pendingSpell(pending,3,padMask,255),255);
    }
  }

TEST(GamepadPlayerInput, WeaponReplacementRetainsKeyboardOrigins) {
  WeaponInput::Command pending[11];
  PadInput::requestWeapon(pending,6,KeyCodec::WeaponMage6,keyOwner,0);
  PadInput::requestWeapon(pending,6,KeyCodec::WeaponMageNext,padOwner,padMask);
  PadInput::requestWeapon(pending,1,KeyCodec::WeaponMele,padOwner,padMask);
  EXPECT_TRUE(pending[6]);
  EXPECT_TRUE(pending[6].hasOrigin(keyOwner));
  EXPECT_FALSE(pending[6].hasOrigin(padMask));
  EXPECT_TRUE(pending[1].hasOrigin(padMask));
  EXPECT_EQ(PadInput::pendingSpell(pending,3,padMask,255),255);
  pending[1].cancel(padMask);
  EXPECT_FALSE(pending[1]);
  EXPECT_TRUE(pending[6]);
  }

TEST(GamepadPlayerInput, RepeatedCycleUsesOnlyLivePendingRequest) {
  WeaponInput::Command pending[11];
  constexpr uint8_t occupied = (1u<<0) | (1u<<3) | (1u<<7);
  for(auto expected : {3,6,10,3}) {
    auto pivot = PadInput::pendingSpell(pending,3,padMask,255);
    auto selected = PadInput::nextSpell(occupied,pivot,false);
    EXPECT_EQ(selected,expected);
    PadInput::requestWeapon(pending,selected,KeyCodec::WeaponMageNext,padOwner,padMask);
    EXPECT_EQ(PadInput::pendingSpell(pending,3,padMask,255),expected);
    }
  // Source cancellation and consumption both remove the actual pivot.
  for(auto& command : pending)
    command.cancel(padMask);
  EXPECT_EQ(PadInput::pendingSpell(pending,3,padMask,6),6);
  EXPECT_EQ(PadInput::nextSpell(occupied,PadInput::pendingSpell(pending,3,padMask,6),false),10);
  }

TEST(GamepadPlayerInput, SharedKeyboardWeaponReplacesOldPadRequestWithoutReplay) {
  WeaponInput::Command pending[11];
  PadInput::requestWeapon(pending,6,KeyCodec::WeaponMageNext,padOwner,padMask);
  PadInput::requestWeapon(pending,1,KeyCodec::WeaponMele,keyOwner,0);
  // The duplicate physical-action path shares a pending keyboard request,
  // after cancelling the previous request of the newly joining controller.
  PadInput::cancelWeaponRequests(pending,padMask);
  for(auto& command : pending)
    command.share(KeyCodec::WeaponMele,padOwner);
  EXPECT_FALSE(pending[6]);
  EXPECT_TRUE(pending[1].hasOrigin(keyOwner));
  EXPECT_TRUE(pending[1].hasOrigin(padOwner));
  pending[1]=false;
  PadInput::cancelWeaponRequests(pending,padMask);
  for(auto& command : pending)
    command.share(KeyCodec::WeaponMele,padOwner);
  EXPECT_FALSE(pending[1]);
  }

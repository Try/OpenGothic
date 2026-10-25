#include "utils/actioninput.h"

#include <gtest/gtest.h>

namespace {
using Input = ActionInput<12, 10>;
using Source = Input::Source;
constexpr size_t Forward = 0;
constexpr size_t Map = 1;
constexpr size_t Heal = 2;
constexpr size_t Potion = 3;
constexpr size_t Attack = 4;
constexpr size_t Weapon = 5;
constexpr size_t Escape = 6;
constexpr size_t Inventory = 7;
constexpr size_t Log = 8;
constexpr size_t Status = 9;
constexpr size_t Idle = 10;
constexpr size_t Enter = 11;
constexpr auto keyboard = Input::owner(Source::Keyboard, false);
constexpr auto secondary = Input::owner(Source::Keyboard, true);
constexpr auto gamepad = Input::owner(Source::Gamepad, false);
}

TEST(ActionInput, EveryPairOfOwnersSurvivesEitherReleaseOrder) {
  for(size_t first=0; first<Input::NumOwners; ++first) {
    for(size_t second=0; second<Input::NumOwners; ++second) {
      if(first==second)
        continue;
      Input input;
      auto a = Input::Mask(1u<<first);
      auto b = Input::Mask(1u<<second);
      EXPECT_TRUE(input.press(Forward, a));
      EXPECT_FALSE(input.press(Forward, b));
      EXPECT_FALSE(input.release(Forward, a));
      EXPECT_TRUE(input.held(Forward));
      EXPECT_TRUE(input.release(Forward, b));
      EXPECT_FALSE(input.held(Forward));
      EXPECT_FALSE(input.release(Forward, b));
      }
    }
  }

TEST(ActionInput, CancelHasNoReleaseEdgeForMapHealOrPotion) {
  for(auto action : {Map, Heal, Potion}) {
    Input input;
    EXPECT_TRUE(input.press(action, gamepad));
    input.cancel(Input::sourceMask(Source::Gamepad));
    // The release handler must not dispatch an effect after reset/disconnect.
    EXPECT_FALSE(input.release(action, gamepad));
    EXPECT_FALSE(input.held(action));
    }
  }

TEST(ActionInput, CancelPreservesKeyboardPrimaryAndSecondary) {
  Input input;
  input.press(Forward, keyboard);
  input.press(Forward, secondary);
  input.press(Forward, gamepad);
  input.cancel(Input::sourceMask(Source::Gamepad));
  EXPECT_FALSE(input.release(Forward, keyboard));
  EXPECT_TRUE(input.held(Forward, secondary));
  EXPECT_TRUE(input.release(Forward, secondary));
  }

TEST(ActionInput, PendingCombatAndWeaponKeepTheirOtherOrigins) {
  Input::Command combat, weapon;
  combat.issue(Attack, keyboard);
  combat.issue(Forward, gamepad);
  weapon.issue(Weapon, keyboard);
  weapon.share(Weapon, gamepad);
  combat.cancel(Input::sourceMask(Source::Gamepad));
  weapon.cancel(Input::sourceMask(Source::Gamepad));
  EXPECT_TRUE(combat);
  EXPECT_TRUE(weapon);
  combat.cancel(Input::sourceMask(Source::Keyboard));
  weapon.cancel(Input::sourceMask(Source::Keyboard));
  EXPECT_FALSE(combat);
  EXPECT_FALSE(weapon);
  }

TEST(ActionInput, ConsumedCommandsCannotBeRevivedBySharingOrCancellation) {
  Input input;
  Input::Command command;
  EXPECT_TRUE(input.press(Attack, keyboard));
  command.issue(Attack, keyboard);
  command = false; // gameplay tick consumes the command, physical hold persists
  EXPECT_FALSE(input.press(Attack, keyboard));
  EXPECT_FALSE(input.press(Attack, gamepad));
  command.share(Attack, gamepad);
  EXPECT_FALSE(command);
  input.cancel(Input::sourceMask(Source::Gamepad));
  command.cancel(Input::sourceMask(Source::Gamepad));
  EXPECT_FALSE(command);
  EXPECT_TRUE(input.held(Attack, keyboard));
  EXPECT_TRUE(input.release(Attack, keyboard));
  EXPECT_TRUE(input.press(Attack, keyboard));
  command.issue(Attack, keyboard);
  EXPECT_TRUE(command);
  }

TEST(ActionInput, PendingWeaponCanOutliveReleaseAndCancelByOriginalSource) {
  Input input;
  Input::Command weapon;
  input.press(Weapon, gamepad);
  weapon.issue(Weapon, gamepad);
  EXPECT_TRUE(input.release(Weapon, gamepad));
  EXPECT_TRUE(weapon);
  weapon = !false; // gameplay retries an unavailable weapon switch
  weapon.cancel(Input::sourceMask(Source::Gamepad));
  EXPECT_FALSE(weapon);
  }

TEST(ActionInput, SharingUsesActionAsWellAsSource) {
  Input::Command command;
  command.issue(Weapon, keyboard);
  command.share(Attack, gamepad);
  command.cancel(Input::sourceMask(Source::Keyboard));
  EXPECT_FALSE(command);
  }

TEST(ActionInput, MergePreservesIndependentCombatOrigins) {
  Input::Command command, generic;
  command.issue(Attack, keyboard);
  generic.issue(Forward, gamepad);
  command.merge(generic);
  command.cancel(Input::sourceMask(Source::Gamepad));
  EXPECT_TRUE(command);
  command = false;
  command.merge(generic);
  command.cancel(Input::sourceMask(Source::Gamepad));
  EXPECT_FALSE(command);
  }

TEST(ActionInput, G1CombinationRequiresTriggerAndModifierInEitherSourceOrder) {
  for(bool reverse : {false, true}) {
    const auto modifier = reverse ? gamepad : keyboard;
    const auto trigger = reverse ? keyboard : gamepad;
    Input input;
    Input::Command attack;
    input.press(Attack, modifier); // ActionGeneric in G1
    input.press(Forward, trigger);
    attack.issue(Forward, trigger);
    attack.require(Attack, input.owners(Attack));
    // A third source has no role in this combination.
    attack.cancel(Input::sourceMask(Source::Touch));
    EXPECT_TRUE(attack);
    attack.cancel(modifier);
    EXPECT_FALSE(attack);

    attack.issue(Forward, trigger);
    attack.require(Attack, input.owners(Attack));
    attack.cancel(trigger);
    EXPECT_FALSE(attack);
    }
  }

TEST(ActionInput, NewModifierOwnerPreservesExistingG1CombinationWithoutReplay) {
  Input::Command attack;
  attack.issue(Forward, secondary);
  attack.require(Attack, keyboard);
  attack.share(Attack, gamepad);
  attack.cancel(keyboard);
  EXPECT_TRUE(attack);
  attack.cancel(gamepad);
  EXPECT_FALSE(attack);
  attack.share(Attack, keyboard);
  EXPECT_FALSE(attack);
  }

TEST(ActionInput, TickCopiesOriginsWhenConvertingCombatMoveIntoForward) {
  Input::Command combatMove, forward;
  combatMove.issue(Attack, keyboard);
  forward = combatMove; // ctrl[Forward] = actrl[ActMove]
  combatMove = false;
  forward.cancel(Input::sourceMask(Source::Gamepad));
  EXPECT_TRUE(forward);
  forward.cancel(Input::sourceMask(Source::Keyboard));
  EXPECT_FALSE(forward);
  }

TEST(ActionInput, CombatMoveCopiedToForwardKeepsRequiredPhysicalDirection) {
  Input::Command combatMove, forward;
  combatMove.issue(Attack, keyboard);
  combatMove.require(Forward, gamepad);
  forward = combatMove;
  combatMove = false;
  EXPECT_TRUE(forward);
  forward.cancel(Input::sourceMask(Source::Gamepad));
  EXPECT_FALSE(forward);
  }

TEST(ActionInput, MergeCannotReviveOriginsFromCancelledCombination) {
  Input::Command command, replacement;
  command.issue(Attack, keyboard);
  command.require(Forward, gamepad);
  command.cancel(gamepad);
  EXPECT_FALSE(command);
  replacement.issue(Weapon, gamepad);
  command.merge(replacement);
  command.cancel(gamepad);
  EXPECT_FALSE(command);
  }

TEST(ActionInput, ReleaseOnlyMenusFireOnceOnLastValidOwnerInBothOrders) {
  for(auto action : {Escape, Inventory, Log, Status}) {
    for(bool keyboardFirst : {false, true}) {
      Input input;
      const auto first = keyboardFirst ? keyboard : gamepad;
      const auto second = keyboardFirst ? gamepad : keyboard;
      EXPECT_TRUE(input.press(action, first));
      EXPECT_FALSE(input.press(action, second));
      EXPECT_FALSE(input.release(action, first));
      EXPECT_TRUE(input.release(action, second));
      EXPECT_FALSE(input.release(action, first));
      EXPECT_FALSE(input.release(action, second));
      }
    }
  }

TEST(ActionInput, CancelledMenuLateReleaseCannotOpenUiOrConsumeKeyboardOwner) {
  for(auto action : {Escape, Inventory, Log, Status}) {
    Input input;
    input.press(action, keyboard);
    input.press(action, gamepad);
    input.cancel(Input::sourceMask(Source::Gamepad));
    EXPECT_FALSE(input.release(action, gamepad));
    EXPECT_TRUE(input.held(action, keyboard));
    EXPECT_TRUE(input.release(action, keyboard));
    input.press(action, gamepad);
    input.clear();
    EXPECT_FALSE(input.release(action, gamepad));
    }
  }

TEST(ActionInput, UnboundHoldCannotSuppressRawReturnInEitherReleaseOrder) {
  for(bool returnFirst : {false, true}) {
    Input input;
    Input::Command transform;
    // Both the unrelated unbound key and default Return translate to Idle.
    // These are the same ownership operations used by PlayerControl before
    // handling the raw Return command.
    EXPECT_TRUE(input.press(Idle, keyboard));
    EXPECT_FALSE(input.held(Idle, keyboard));
    EXPECT_TRUE(input.press(Idle, keyboard));
    transform.issue(Enter, keyboard);
    EXPECT_TRUE(transform);
    // Neither Idle release has an action edge or consumes transformBack.
    // The order corresponds to Return/H or H/Return respectively.
    const std::array<const char*, 2> releases = returnFirst ?
        std::array<const char*, 2>{"Return", "H"} :
        std::array<const char*, 2>{"H", "Return"};
    for(auto key : releases) {
      SCOPED_TRACE(key);
      EXPECT_FALSE(input.release(Idle, keyboard));
      EXPECT_TRUE(transform);
      }
    transform = false; // tick executes transformBack once
    EXPECT_FALSE(transform);
    }
  }

TEST(ActionInput, RawReturnRemainsCancellableByItsSourceWithoutIdleOwnership) {
  Input input;
  Input::Command transform;
  input.press(Idle, keyboard);
  transform.issue(Enter, keyboard);
  input.cancel(Input::sourceMask(Source::Gamepad));
  transform.cancel(Input::sourceMask(Source::Gamepad));
  EXPECT_TRUE(transform);
  input.cancel(Input::sourceMask(Source::Keyboard));
  transform.cancel(Input::sourceMask(Source::Keyboard));
  EXPECT_FALSE(input.release(Idle, keyboard));
  EXPECT_FALSE(transform);
  }

TEST(ActionInput, GameplayCommandResetKeepsMenuReleaseButContextResetCancelsIt) {
  for(auto action : {Escape, Inventory, Log, Status}) {
    Input input;
    Input::Command command;
    input.press(action, keyboard);
    command.issue(action, keyboard);
    command = false; // clearCommands during the intermediate gameplay frame
    EXPECT_FALSE(command);
    EXPECT_TRUE(input.release(action, keyboard));

    input.press(action, keyboard);
    command.issue(action, keyboard);
    input.clear(); // clearInput during context/focus/loading cancellation
    command = false;
    EXPECT_FALSE(input.release(action, keyboard));
    }
  }

TEST(ActionInput, GameplayResetConsumesCommandWithoutRearmingHeldPhysicalInput) {
  Input input;
  Input::Command command;
  input.press(Attack, keyboard);
  command.issue(Attack, keyboard);
  command = false;
  EXPECT_TRUE(input.held(Attack, keyboard));
  EXPECT_FALSE(input.press(Attack, keyboard));
  command.share(Attack, keyboard);
  EXPECT_FALSE(command);
  EXPECT_TRUE(input.release(Attack, keyboard));
  EXPECT_TRUE(input.press(Attack, keyboard));
  command.issue(Attack, keyboard);
  EXPECT_TRUE(command);
  }

TEST(ActionInput, RemappedSecondaryReturnSharesGameplayButKeepsRawCommand) {
  for(bool returnFirst : {false, true}) {
    Input input;
    Input::Command movement, transform;
    EXPECT_TRUE(input.press(Forward, keyboard)); // W primary
    movement.issue(Forward, keyboard);
    movement = false; // gameplay consumed the command before Return down
    EXPECT_FALSE(input.press(Forward, secondary)); // Return secondary
    movement.share(Forward, secondary);
    // The duplicate mapped-action path must issue the independent raw command.
    transform.issue(Enter, secondary);
    EXPECT_FALSE(movement);
    EXPECT_TRUE(transform);
    const auto first = returnFirst ? secondary : keyboard;
    const auto second = returnFirst ? keyboard : secondary;
    EXPECT_FALSE(input.release(Forward, first));
    movement.cancel(first);
    EXPECT_FALSE(movement);
    EXPECT_TRUE(transform);
    EXPECT_TRUE(input.release(Forward, second));
    movement.cancel(second);
    EXPECT_FALSE(movement);
    EXPECT_TRUE(transform);
    transform = false;
    EXPECT_FALSE(transform);
    }
  }

TEST(ActionInput, UnrelatedReleaseFromSameOwnerPreservesHeldCombat) {
  for(auto owner : {keyboard, secondary, gamepad}) {
    Input input;
    Input::Command block;
    input.press(Attack, owner); // L2 / block
    block.issue(Attack, owner);
    for(auto unrelated : {Forward, Map, Weapon}) {
      input.press(unrelated, owner); // e.g. Jump or the Walk toggle
      EXPECT_TRUE(input.release(unrelated, owner));
      block.release(unrelated, owner);
      EXPECT_TRUE(block);
      EXPECT_TRUE(input.held(Attack, owner));
      }
    EXPECT_TRUE(input.release(Attack, owner));
    block.release(Attack, owner);
    EXPECT_FALSE(block);
    }
  }

TEST(ActionInput, PhysicalReleaseRequiresTheLastTriggerAndModifier) {
  for(bool modifierFirst : {false, true}) {
    Input::Command attack;
    attack.issue(Forward, keyboard);
    attack.share(Forward, gamepad);
    attack.require(Attack, keyboard);
    attack.share(Attack, gamepad);
    const auto firstAction = modifierFirst ? Attack : Forward;
    const auto otherAction = modifierFirst ? Forward : Attack;
    attack.release(firstAction, keyboard);
    EXPECT_TRUE(attack);
    attack.release(otherAction, keyboard);
    EXPECT_TRUE(attack); // both actions still belong to the controller
    attack.release(firstAction, gamepad);
    EXPECT_FALSE(attack); // loss of final trigger OR final required modifier
    attack.share(firstAction, keyboard);
    EXPECT_FALSE(attack); // release cannot rearm a consumed/cancelled command
    }
  }

TEST(ActionInput, PhysicalReleaseDoesNotRemoveAnotherActionBySameOwner) {
  Input::Command combined;
  combined.issue(Forward, gamepad);
  combined.issue(Attack, gamepad);
  combined.release(Forward, gamepad);
  EXPECT_TRUE(combined);
  combined.release(Attack, gamepad);
  EXPECT_FALSE(combined);
  }

TEST(ActionInput, PhysicalModifierReleaseCancelsSameOwnerCombination) {
  Input::Command attack;
  attack.issue(Forward, gamepad);
  attack.require(Attack, gamepad);
  attack.release(Attack, gamepad);
  EXPECT_FALSE(attack); // the surviving Forward origin is insufficient
  }

TEST(ActionInput, SourceCancelStillRemovesAllRemainingActionsAfterSelectiveRelease) {
  Input::Command attack;
  attack.issue(Forward, gamepad);
  attack.issue(Attack, gamepad);
  attack.issue(Weapon, keyboard);
  attack.release(Forward, gamepad);
  EXPECT_TRUE(attack);
  attack.cancel(Input::sourceMask(Source::Gamepad));
  EXPECT_TRUE(attack); // keyboard weapon origin survives
  attack.release(Weapon, keyboard);
  EXPECT_FALSE(attack);
  }

TEST(ActionInput, IndependentOriginDoesNotInheritAnotherOwnersDependency) {
  for(bool directFirst : {false, true}) {
    Input::Command command;
    if(directFirst)
      command.issue(Attack, gamepad);
    command.issue(Forward, keyboard);
    command.require(Attack, secondary, keyboard);
    if(!directFirst)
      command.issue(Attack, gamepad);
    command.release(Attack, secondary);
    EXPECT_TRUE(command);
    EXPECT_FALSE(command.hasOrigin(keyboard));
    EXPECT_TRUE(command.hasOrigin(gamepad));
    command.release(Attack, gamepad);
    EXPECT_FALSE(command);
    }
  }

TEST(ActionInput, SemanticSharingSeparatesOriginsButKeepsModifierOwners) {
  Input::Command command;
  command.issue(Forward, keyboard);
  command.require(Attack, keyboard);
  command.share(Forward, gamepad, keyboard); // equivalent trigger
  command.share(Forward, secondary, secondary); // different role
  EXPECT_TRUE(command.hasOrigin(gamepad));
  EXPECT_FALSE(command.hasOrigin(secondary));
  command.share(Attack, secondary, secondary); // still a valid raw modifier
  command.release(Attack, keyboard);
  EXPECT_TRUE(command);
  command.release(Attack, secondary);
  EXPECT_FALSE(command); // both equivalent triggers require that modifier
  }

TEST(ActionInput, ReleasedOwnerDependenciesCannotLeakIntoLaterIndependentIssue) {
  Input::Command command;
  command.issue(Forward, keyboard);
  command.require(Attack, secondary);
  command.issue(Weapon, gamepad);
  command.release(Forward, keyboard);
  command.issue(Weapon, keyboard);
  command.release(Attack, secondary);
  EXPECT_TRUE(command.hasOrigin(keyboard));
  EXPECT_TRUE(command.hasOrigin(gamepad));
  }

TEST(ActionInput, MergeKeepsDependenciesScopedToItsOriginalTrigger) {
  Input::Command combination, direct;
  combination.issue(Forward, keyboard);
  combination.require(Attack, secondary);
  direct.issue(Attack, gamepad);
  for(bool reverse : {false, true}) {
    auto command = reverse ? direct : combination;
    command.merge(reverse ? combination : direct);
    command.cancel(secondary);
    EXPECT_FALSE(command.hasOrigin(keyboard));
    EXPECT_TRUE(command.hasOrigin(gamepad));
    command.cancel(gamepad);
    EXPECT_FALSE(command);
    }
  }

#include "utils/gamepadinput.h"
#include "utils/actioninput.h"

#include <gtest/gtest.h>
#include <cmath>
#include <limits>

namespace {
using Input = GamepadInput;
using Pad = Tempest::Gamepad;
using Button = Pad::Button;
using Type = Input::Command::Type;
using EventType = Pad::Event::Type;
using Context = Input::Context;

Input input(Context context) {
  Input result;
  result.selectDevice(1, {});
  result.setContext({context, 1}, {});
  return result;
  }

Pad::Event edge(Button button, bool down, Pad::DeviceId id = 1) {
  return {down ? EventType::ButtonDown : EventType::ButtonUp, id, button};
  }

Pad::State held(Button button) {
  Pad::State state;
  state.buttons = uint32_t(1)<<uint8_t(button);
  return state;
  }

TEST(GamepadInput, ShortTapPreservesBothGameplayEdges) {
  auto router = input(Context::Gameplay);
  auto down = router.process(edge(Button::South, true), 0);
  auto up = router.process(edge(Button::South, false), 0);
  EXPECT_EQ(Type::Press, down.type);
  EXPECT_EQ(KeyCodec::ActionGeneric, down.action);
  EXPECT_EQ(Type::Release, up.type);
  EXPECT_EQ(down.action, up.action);
  EXPECT_EQ(Type::None, router.process(edge(Button::South, false), 0).type);
  }

TEST(GamepadInput, DuplicateDownDoesNotToggleAgain) {
  auto router = input(Context::Gameplay);
  EXPECT_EQ(Type::Press, router.process(edge(Button::West, true), 0).type);
  EXPECT_EQ(Type::None, router.process(edge(Button::West, true), 1).type);
  EXPECT_EQ(Type::Release, router.process(edge(Button::West, false), 2).type);
  }

TEST(GamepadInput, ConfirmAndBackNeverRepeatOrGenerateReleaseActions) {
  for(auto button : {Button::South, Button::East, Button::Menu}) {
    auto router = input(Context::Menu);
    EXPECT_EQ(Type::Navigate, router.process(edge(button, true), 0).type);
    EXPECT_EQ(Type::None, router.update(held(button), 5000).type);
    EXPECT_EQ(Type::None, router.process(edge(button, false), 5001).type);
    }
  }

TEST(GamepadInput, SameKindNewScreenCancelsAndRequiresNeutral) {
  auto router = input(Context::Menu);
  EXPECT_EQ(Type::Navigate, router.process(edge(Button::South, true), 0).type);
  EXPECT_EQ(Type::CancelAll, router.setContext({Context::Menu, 2}, held(Button::South)).type);
  EXPECT_EQ(Type::None, router.process(edge(Button::South, true), 1).type);
  EXPECT_EQ(Type::None, router.update(held(Button::South), 5000).type);
  EXPECT_EQ(Type::None, router.process(edge(Button::South, false), 5001).type);
  EXPECT_EQ(Type::Navigate, router.process(edge(Button::South, true), 5002).type);
  }

TEST(GamepadInput, InventoryOperationsAreContextSpecificSinglePresses) {
  const std::pair<Button, KeyCodec::Action> bindings[] = {
    {Button::West, KeyCodec::UiAmount}, {Button::North, KeyCodec::UiAssign},
    {Button::LeftShoulder, KeyCodec::UiPrevious}, {Button::RightShoulder, KeyCodec::UiNext}
    };
  for(const auto& binding : bindings) {
    auto router = input(Context::Inventory);
    auto command = router.process(edge(binding.first, true), 0);
    EXPECT_EQ(Type::Navigate, command.type);
    EXPECT_EQ(binding.second, command.action);
    EXPECT_EQ(Type::None, router.process(edge(binding.first, true), 10).type);
    EXPECT_EQ(Type::None, router.update(held(binding.first), 10000).type);
    EXPECT_EQ(Type::None, router.process(edge(binding.first, false), 10001).type);
    for(auto context : {Context::Menu, Context::Dialog, Context::Document,
                        Context::Video, Context::Chapter, Context::Blocked}) {
      auto other = input(context);
      const auto expected = context==Context::Menu && binding.first==Button::West ? Type::Navigate : Type::None;
      EXPECT_EQ(expected, other.process(edge(binding.first, true), 0).type);
      }
    }
  }

TEST(GamepadInput, InventoryModalTransitionsDiscardHeldOperationsAndNavigation) {
  for(auto button : {Button::South, Button::East, Button::Menu, Button::West,
                     Button::North, Button::LeftShoulder, Button::RightShoulder}) {
    auto router = input(Context::Inventory);
    EXPECT_EQ(Type::Navigate, router.process(edge(button, true), 0).type);
    auto state = held(button);
    state.axes[size_t(Pad::Axis::LeftY)] = 1.f;
    router.update(state, 1);
    // Opening a quantity/slot picker has the same kind, but a new revision.
    EXPECT_EQ(Type::CancelAll, router.setContext({Context::Inventory, 2}, state).type);
    EXPECT_EQ(Type::None, router.process(edge(button, true), 2).type);
    EXPECT_EQ(Type::None, router.update(state, 10000).type);
    EXPECT_EQ(Type::None, router.process(edge(button, false), 10001).type);
    router.update({}, 10002);
    EXPECT_EQ(Type::Navigate, router.process(edge(button, true), 10003).type);
    // Confirming or cancelling the picker must not activate its parent.
    EXPECT_EQ(Type::CancelAll, router.setContext({Context::Inventory, 3}, held(button)).type);
    EXPECT_EQ(Type::None, router.update(held(button), 20000).type);
    EXPECT_EQ(Type::None, router.process(edge(button, false), 20001).type);
    EXPECT_EQ(Type::Navigate, router.process(edge(button, true), 20002).type);
    }
  }

TEST(GamepadInput, SupportedNestedMenuRequiresFreshConfirmAtEachBoundary) {
  auto router = input(Context::Menu);
  for(uint64_t revision=2; revision<=5; ++revision) {
    EXPECT_EQ(Type::Navigate, router.process(edge(Button::South, true), revision*10).type);
    EXPECT_EQ(Type::CancelAll, router.setContext({Context::Menu, revision}, held(Button::South)).type);
    EXPECT_EQ(Type::None, router.update(held(Button::South), revision*1000).type);
    EXPECT_EQ(Type::None, router.process(edge(Button::South, false), revision*1000+1).type);
    }
  }

TEST(GamepadInput, CancelCannotOpenMenuOnRelease) {
  auto router = input(Context::Gameplay);
  EXPECT_EQ(Type::Press, router.process(edge(Button::Menu, true), 0).type);
  const auto reset = router.process({EventType::Reset, 1, Button::South, Pad::Event::ResetReason::FocusLost}, 1);
  EXPECT_EQ(Type::Cancel, reset.type);
  EXPECT_EQ(KeyCodec::Idle, reset.action);
  EXPECT_EQ(Type::None, router.process(edge(Button::Menu, false), 2).type);
  }

TEST(GamepadInput, BlockedModalConsumesAndDoesNotLeakHeldInputOnReturn) {
  auto router = input(Context::Menu);
  router.setContext({Context::Blocked, 2}, {});
  EXPECT_EQ(Type::None, router.process(edge(Button::South, true), 1).type);
  EXPECT_EQ(Type::None, router.process(edge(Button::DpadDown, true), 2).type);
  auto state = held(Button::South);
  state.buttons |= held(Button::DpadDown).buttons;
  router.setContext({Context::Menu, 3}, state);
  EXPECT_EQ(Type::None, router.update(state, 8000).type);
  EXPECT_EQ(Type::None, router.process(edge(Button::South, false), 8001).type);
  EXPECT_EQ(Type::None, router.process(edge(Button::DpadDown, false), 8002).type);
  EXPECT_EQ(Type::Navigate, router.process(edge(Button::DpadDown, true), 8003).type);
  }

TEST(GamepadInput, RepeatsUseWallClockWithoutCatchupBursts) {
  auto router = input(Context::Menu);
  EXPECT_EQ(KeyCodec::Forward, router.process(edge(Button::DpadUp, true), 100).action);
  const auto state = held(Button::DpadUp);
  EXPECT_EQ(Type::None, router.update(state, 499).type);
  EXPECT_EQ(Type::Navigate, router.update(state, 500).type);
  EXPECT_EQ(Type::None, router.update(state, 619).type);
  EXPECT_EQ(Type::Navigate, router.update(state, 620).type);
  EXPECT_EQ(Type::Navigate, router.update(state, 10000).type);
  EXPECT_EQ(Type::None, router.update(state, 10000).type);
  EXPECT_EQ(Type::None, router.update(state, 10119).type);
  EXPECT_EQ(Type::Navigate, router.update(state, 10120).type);
  }

TEST(GamepadInput, OppositeDirectionsCancelAndReleaseResumesRemaining) {
  auto router = input(Context::Menu);
  EXPECT_EQ(KeyCodec::Forward, router.process(edge(Button::DpadUp, true), 0).action);
  EXPECT_EQ(Type::None, router.process(edge(Button::DpadDown, true), 1).type);
  auto state = held(Button::DpadUp);
  state.buttons |= held(Button::DpadDown).buttons;
  EXPECT_EQ(Type::None, router.update(state, 1000).type);
  EXPECT_EQ(KeyCodec::Back, router.process(edge(Button::DpadUp, false), 1001).action);
  }

TEST(GamepadInput, StickHysteresisAndContextNeutralGate) {
  auto router = input(Context::Menu);
  Pad::State state;
  state.axes[size_t(Pad::Axis::LeftY)] = 0.54f;
  EXPECT_EQ(Type::None, router.update(state, 0).type);
  state.axes[size_t(Pad::Axis::LeftY)] = 0.56f;
  EXPECT_EQ(KeyCodec::Forward, router.update(state, 1).action);
  state.axes[size_t(Pad::Axis::LeftY)] = 0.4f;
  EXPECT_EQ(KeyCodec::Forward, router.update(state, 401).action);
  router.setContext({Context::Menu, 2}, state);
  state.axes[size_t(Pad::Axis::LeftY)] = 1.f;
  EXPECT_EQ(Type::None, router.update(state, 1000).type);
  state.axes[size_t(Pad::Axis::LeftY)] = 0.35f;
  EXPECT_EQ(Type::None, router.update(state, 1001).type);
  state.axes[size_t(Pad::Axis::LeftY)] = -0.56f;
  EXPECT_EQ(KeyCodec::Back, router.update(state, 1002).action);
  }

TEST(GamepadInput, DpadAndStickShareOneNavigationHold) {
  auto router = input(Context::Menu);
  auto state = held(Button::DpadUp);
  state.axes[size_t(Pad::Axis::LeftY)] = 1;
  EXPECT_EQ(KeyCodec::Forward, router.process(edge(Button::DpadUp, true), 0).action);
  EXPECT_EQ(Type::None, router.update(state, 1).type);
  EXPECT_EQ(Type::None, router.process(edge(Button::DpadUp, false), 2).type);
  state.buttons = 0;
  EXPECT_EQ(KeyCodec::Forward, router.update(state, 400).action);
  }

TEST(GamepadInput, OtherDeviceCannotStealOrCancelActiveInput) {
  auto router = input(Context::Gameplay);
  EXPECT_EQ(Type::None, router.process(edge(Button::South, true, 2), 0).type);
  EXPECT_EQ(Type::Press, router.process(edge(Button::South, true), 1).type);
  EXPECT_EQ(Type::None, router.process({EventType::Reset, 2}, 2).type);
  EXPECT_EQ(Type::None, router.process({EventType::Disconnected, 2}, 3).type);
  EXPECT_EQ(Type::Release, router.process(edge(Button::South, false), 4).type);
  EXPECT_EQ(1u, router.device());
  }

TEST(GamepadInput, DisconnectAndOverflowCancelWithoutRelease) {
  for(auto event : {Pad::Event{EventType::Disconnected, 1},
                    Pad::Event{EventType::Reset, 0, Button::South, Pad::Event::ResetReason::Overflow}}) {
    auto router = input(Context::Gameplay);
    router.process(edge(Button::North, true), 0);
    EXPECT_EQ(Type::Cancel, router.process(event, 1).type);
    EXPECT_EQ(Type::None, router.process(edge(Button::North, false), 2).type);
    router.selectDevice(2, held(Button::South));
    EXPECT_EQ(Type::None, router.process(edge(Button::South, true, 2), 3).type);
    EXPECT_EQ(Type::None, router.process(edge(Button::South, false, 2), 4).type);
    EXPECT_EQ(Type::Press, router.process(edge(Button::South, true, 2), 5).type);
    }
  }

TEST(GamepadInput, PassiveScreensOnlyAcceptBack) {
  for(auto context : {Context::Video, Context::Document, Context::Chapter}) {
    auto router = input(context);
    EXPECT_EQ(Type::None, router.process(edge(Button::South, true), 0).type);
    EXPECT_EQ(Type::None, router.process(edge(Button::DpadUp, true), 0).type);
    EXPECT_EQ(KeyCodec::Escape, router.process(edge(Button::East, true), 1).action);
    }
  }

TEST(GamepadInput, TriggerAxesAndInvalidUiValuesDoNotCreateNavigation) {
  auto router = input(Context::Gameplay);
  Pad::State state;
  state.axes[size_t(Pad::Axis::LeftTrigger)] = 1.f;
  state.axes[size_t(Pad::Axis::RightTrigger)] = 1.f;
  EXPECT_EQ(Type::None, router.update(state, 100).type);
  state.axes.fill(0.f);
  router.setContext({Context::Menu, 2}, state);
  state.axes[size_t(Pad::Axis::LeftX)] = std::numeric_limits<float>::quiet_NaN();
  state.axes[size_t(Pad::Axis::LeftY)] = std::numeric_limits<float>::infinity();
  EXPECT_EQ(Type::None, router.update(state, 200).type);
  }

TEST(GamepadInput, ReturnToGameplayPreservesFreshKeyboardBeforeNextFrame) {
  using Holds = ActionInput<KeyCodec::Last, KeyCodec::Idle>;
  const auto keyboard = Holds::owner(Holds::Source::Keyboard, false);
  const auto pad = Holds::owner(Holds::Source::Gamepad, false);
  for(auto from : {Context::Menu, Context::Blocked}) {
    auto router = input(from);
    Holds holds;
    // The menu closes (or focus returns), then W arrives in the same OS batch
    // before MainWindow's next frame observes the context change.
    holds.press(KeyCodec::Forward, keyboard);
    holds.press(KeyCodec::Forward, pad);
    auto command = router.setContext({Context::Gameplay, 2}, {});
    ASSERT_EQ(Type::Cancel, command.type);
    holds.cancel(Holds::sourceMask(Holds::Source::Gamepad));
    EXPECT_TRUE(holds.held(KeyCodec::Forward, keyboard));
    EXPECT_FALSE(holds.held(KeyCodec::Forward, pad));
    EXPECT_TRUE(holds.release(KeyCodec::Forward, keyboard));
    EXPECT_EQ(Type::None, router.setContext({Context::Gameplay, 2}, {}).type);
    }
  }

TEST(GamepadInput, LeavingGameplayCancelsAllOldInputWithoutRelease) {
  for(auto to : {Context::Menu, Context::Blocked, Context::Inventory, Context::Dialog}) {
    auto router = input(Context::Gameplay);
    router.process(edge(Button::Menu, true), 0);
    EXPECT_EQ(Type::CancelAll, router.setContext({to, 2}, held(Button::Menu)).type);
    EXPECT_EQ(Type::None, router.process(edge(Button::Menu, false), 1).type);
    }
  }

TEST(GamepadInput, GameplayBindingsCoverCombatWeaponsAndMovementModes) {
  const std::pair<Button, KeyCodec::Action> bindings[] = {
    {Button::South, KeyCodec::ActionGeneric}, {Button::RightTrigger, KeyCodec::ActionGeneric},
    {Button::East, KeyCodec::Jump}, {Button::West, KeyCodec::Weapon},
    {Button::North, KeyCodec::Inventory}, {Button::LeftTrigger, KeyCodec::Parade},
    {Button::LeftShoulder, KeyCodec::ActionLeft}, {Button::RightShoulder, KeyCodec::ActionRight},
    {Button::LeftThumb, KeyCodec::Walk}, {Button::RightThumb, KeyCodec::Sneak},
    {Button::DpadUp, KeyCodec::WeaponMele}, {Button::DpadDown, KeyCodec::WeaponBow},
    {Button::DpadLeft, KeyCodec::WeaponMagePrev}, {Button::DpadRight, KeyCodec::WeaponMageNext},
    {Button::Menu, KeyCodec::Escape}, {Button::View, KeyCodec::Log}
    };
  for(const auto& binding : bindings) {
    auto router = input(Context::Gameplay);
    const auto command = router.process(edge(binding.first, true), 0);
    EXPECT_EQ(Type::Press, command.type);
    EXPECT_EQ(binding.second, command.action);
    EXPECT_EQ(KeyCodec::Mapping::Primary, command.mapping);
    EXPECT_EQ(Type::Release, router.process(edge(binding.first, false), 1).type);
    }
  }

TEST(GamepadInput, AttackAliasesReleaseOnlyTheLastPhysicalButton) {
  for(bool triggerFirst : {false, true}) {
    for(bool sameReleaseOrder : {false, true}) {
      auto router = input(Context::Gameplay);
      const auto first = triggerFirst ? Button::RightTrigger : Button::South;
      const auto second = triggerFirst ? Button::South : Button::RightTrigger;
      EXPECT_EQ(Type::Press, router.process(edge(first, true), 0).type);
      EXPECT_EQ(Type::None, router.process(edge(second, true), 1).type);
      EXPECT_EQ(Type::None, router.process(edge(sameReleaseOrder ? first : second, false), 2).type);
      const auto release = router.process(edge(sameReleaseOrder ? second : first, false), 3);
      EXPECT_EQ(Type::Release, release.type);
      EXPECT_EQ(KeyCodec::ActionGeneric, release.action);
      }
    }
  }

TEST(GamepadInput, HeldAliasCannotAdoptFreshAttackAcrossContextBoundary) {
  auto router = input(Context::Menu);
  const auto state = held(Button::South);
  router.setContext({Context::Gameplay, 2}, state);
  EXPECT_EQ(Type::Press, router.process(edge(Button::RightTrigger, true), 0).type);
  EXPECT_EQ(Type::None, router.process(edge(Button::South, false), 1).type);
  EXPECT_EQ(Type::Release, router.process(edge(Button::RightTrigger, false), 2).type);
  }

TEST(GamepadInput, MovementUsesHysteresisAndReleasesBeforeDiagonalReversal) {
  auto router = input(Context::Gameplay);
  Pad::State state;
  const auto sample = [&](float x, float y) {
    state.axes[size_t(Pad::Axis::LeftX)] = x;
    state.axes[size_t(Pad::Axis::LeftY)] = y;
    router.update(state, 0);
    return router.movement();
    };
  EXPECT_EQ(Type::None, sample(.54f, .54f)[0].type);
  auto commands = sample(.56f, .56f);
  EXPECT_EQ(Type::Press, commands[0].type);
  EXPECT_EQ(KeyCodec::Right, commands[0].action);
  EXPECT_EQ(KeyCodec::Forward, commands[1].action);
  EXPECT_EQ(KeyCodec::Mapping::Secondary, commands[0].mapping);
  EXPECT_EQ(KeyCodec::Mapping::Secondary, commands[1].mapping);
  EXPECT_EQ(Type::None, sample(.40f, .40f)[0].type);
  commands = sample(-1.f, -1.f);
  EXPECT_EQ(Type::Release, commands[0].type);
  EXPECT_EQ(Type::Release, commands[1].type);
  EXPECT_EQ(KeyCodec::Right, commands[0].action);
  EXPECT_EQ(KeyCodec::Forward, commands[1].action);
  EXPECT_EQ(Type::Press, commands[2].type);
  EXPECT_EQ(Type::Press, commands[3].type);
  EXPECT_EQ(KeyCodec::Left, commands[2].action);
  EXPECT_EQ(KeyCodec::Back, commands[3].action);
  commands = sample(-.35f, -.35f);
  EXPECT_EQ(Type::Release, commands[0].type);
  EXPECT_EQ(Type::Release, commands[1].type);
  EXPECT_EQ(Type::None, commands[2].type);
  }

TEST(GamepadInput, StickReleaseAndCancellationPreserveKeyboardOwnership) {
  using Holds = ActionInput<KeyCodec::Last, KeyCodec::Idle>;
  const auto keyboard = Holds::owner(Holds::Source::Keyboard, false);
  const auto stick = Holds::owner(Holds::Source::Gamepad, true);
  for(bool cancel : {false, true}) {
    auto router = input(Context::Gameplay);
    Holds holds;
    holds.press(KeyCodec::Forward, keyboard);
    Pad::State state;
    state.axes[size_t(Pad::Axis::LeftY)] = 1;
    router.update(state, 0);
    auto command = router.movement()[0];
    ASSERT_EQ(Type::Press, command.type);
    ASSERT_EQ(KeyCodec::Mapping::Secondary, command.mapping);
    holds.press(command.action, stick);
    // A consumed command must not be recreated while the physical stick is held.
    router.update(state, 100);
    EXPECT_EQ(Type::None, router.movement()[0].type);
    if(cancel) {
      ASSERT_EQ(Type::Cancel, router.process({EventType::Reset, 1}, 101).type);
      holds.cancel(Holds::sourceMask(Holds::Source::Gamepad));
      }
    else {
      router.update({}, 101);
      command = router.movement()[0];
      ASSERT_EQ(Type::Release, command.type);
      EXPECT_FALSE(holds.release(command.action, stick));
      }
    EXPECT_TRUE(holds.held(KeyCodec::Forward, keyboard));
    EXPECT_FALSE(holds.held(KeyCodec::Forward, stick));
    }
  }

TEST(GamepadInput, CameraHasRadialDeadzoneBoundedDiagonalAndUpwardPitch) {
  auto router = input(Context::Gameplay);
  Pad::State state;
  state.axes[size_t(Pad::Axis::RightX)] = .2f;
  router.update(state, 0);
  EXPECT_FLOAT_EQ(0.f, router.look(50).yaw);
  state.axes[size_t(Pad::Axis::RightX)] = .6f;
  router.update(state, 1);
  EXPECT_NEAR(-4.5f, router.look(50).yaw, .0001f);
  state.axes[size_t(Pad::Axis::RightX)] = 1.f;
  state.axes[size_t(Pad::Axis::RightY)] = 1.f;
  router.update(state, 2);
  const auto look = router.look(50);
  EXPECT_LT(look.yaw, 0.f);
  EXPECT_GT(look.pitch, 0.f);
  EXPECT_NEAR(1.f, std::hypot(look.yaw/9.f, look.pitch/6.f), .0001f);
  EXPECT_FLOAT_EQ(look.yaw, router.look(10000).yaw);
  EXPECT_FLOAT_EQ(0.f, router.look(0).yaw);
  }

TEST(GamepadInput, CameraIntegratesEqualElapsedTimeAt30And60And120Fps) {
  for(uint64_t fps : {30u, 60u, 120u}) {
    auto router = input(Context::Gameplay);
    Pad::State state;
    state.axes[size_t(Pad::Axis::RightX)] = 1;
    router.update(state, 0);
    float yaw = 0;
    for(uint64_t frame=0; frame<fps; ++frame) {
      const auto dt = (frame+1)*1000/fps - frame*1000/fps;
      yaw += router.look(dt).yaw;
      }
    EXPECT_NEAR(-180.f, yaw, .001f);
    }
  }

TEST(GamepadInput, ContextTransitionRearmsBothSticksIndependently) {
  for(auto context : {Context::Menu, Context::Blocked, Context::Cutscene, Context::Inventory}) {
    auto router = input(Context::Gameplay);
    Pad::State state;
    state.axes[size_t(Pad::Axis::LeftY)] = 1;
    state.axes[size_t(Pad::Axis::RightX)] = 1;
    router.update(state, 0);
    EXPECT_EQ(Type::Press, router.movement()[0].type);
    EXPECT_NE(0.f, router.look(16).yaw);
    router.setContext({context, 2}, state);
    router.update(state, 1);
    EXPECT_EQ(Type::None, router.movement()[0].type);
    EXPECT_EQ(0.f, router.look(16).yaw);
    router.setContext({Context::Gameplay, 3}, state);
    router.update(state, 2);
    EXPECT_EQ(Type::None, router.movement()[0].type);
    EXPECT_EQ(0.f, router.look(16).yaw);
    state.axes[size_t(Pad::Axis::LeftY)] = 0;
    router.update(state, 3);
    state.axes[size_t(Pad::Axis::LeftY)] = 1;
    router.update(state, 4);
    EXPECT_EQ(Type::Press, router.movement()[0].type);
    EXPECT_EQ(0.f, router.look(16).yaw);
    state.axes[size_t(Pad::Axis::RightX)] = 0;
    router.update(state, 5);
    state.axes[size_t(Pad::Axis::RightX)] = 1;
    router.update(state, 6);
    EXPECT_NE(0.f, router.look(16).yaw);
    }
  }

TEST(GamepadInput, InvalidAxesStopMovementAndCamera) {
  for(float invalid : {std::numeric_limits<float>::quiet_NaN(),
                       std::numeric_limits<float>::infinity()}) {
    auto router = input(Context::Gameplay);
    Pad::State state;
    state.axes[size_t(Pad::Axis::LeftX)] = 1;
    state.axes[size_t(Pad::Axis::RightY)] = 1;
    router.update(state, 0);
    router.movement();
    state.axes[size_t(Pad::Axis::LeftX)] = invalid;
    state.axes[size_t(Pad::Axis::RightY)] = invalid;
    router.update(state, 1);
    EXPECT_EQ(Type::Release, router.movement()[0].type);
    EXPECT_EQ(0.f, router.look(50).pitch);
    EXPECT_EQ(0.f, router.look(50).yaw);
    }
  }

TEST(GamepadInput, ResetAndDisconnectDiscardCameraAndMovementWithoutRelease) {
  for(auto type : {EventType::Reset, EventType::Disconnected}) {
    auto router = input(Context::Gameplay);
    Pad::State state;
    state.axes.fill(1.f);
    router.update(state, 0);
    router.movement();
    EXPECT_EQ(Type::Cancel, router.process({type, 1}, 1).type);
    EXPECT_EQ(0.f, router.look(50).yaw);
    EXPECT_EQ(Type::None, router.movement()[0].type);
    router.selectDevice(2, state);
    router.update(state, 2);
    EXPECT_EQ(0.f, router.look(50).yaw);
    EXPECT_EQ(Type::None, router.movement()[0].type);
    }
  }

TEST(GamepadInput, CutscenePreservesMenusButBlocksCombat) {
  auto router = input(Context::Cutscene);
  for(auto button : {Button::South, Button::RightTrigger, Button::LeftTrigger, Button::DpadUp})
    EXPECT_EQ(Type::None, router.process(edge(button, true), 0).type);
  for(auto button : {Button::Menu, Button::North, Button::View}) {
    EXPECT_EQ(Type::Press, router.process(edge(button, true), 1).type);
    EXPECT_EQ(Type::Release, router.process(edge(button, false), 2).type);
    }
  }
}

namespace {
TEST(GamepadInput, ConfigureCancelsOldOwnershipAndGatesHeldButtonsAndSticks) {
  auto router = input(Context::Gameplay);
  EXPECT_EQ(Type::Press, router.process(edge(Button::South, true), 0).type);
  Pad::State state = held(Button::South);
  state.axes[size_t(Pad::Axis::LeftY)] = 1;
  state.axes[size_t(Pad::Axis::RightX)] = 1;
  router.update(state, 1);
  EXPECT_EQ(Type::Press, router.movement()[0].type);
  auto settings = router.settings();
  settings.bindings[size_t(Button::South)] = KeyCodec::Jump;
  EXPECT_EQ(Type::Cancel, router.configure(settings, state).type);
  EXPECT_EQ(settings, router.settings());
  router.update(state, 2);
  EXPECT_EQ(Type::None, router.movement()[0].type);
  EXPECT_EQ(0.f, router.look(50).yaw);
  EXPECT_EQ(Type::None, router.process(edge(Button::South, false), 3).type);
  router.update({}, 4);
  EXPECT_EQ(KeyCodec::Jump, router.process(edge(Button::South, true), 5).action);
  EXPECT_EQ(Type::None, router.configure(settings, state).type);
  EXPECT_EQ(Type::Release, router.process(edge(Button::South, false), 6).type);
  state.buttons = 0;
  router.update(state, 7);
  EXPECT_EQ(Type::Press, router.movement()[0].type);
  EXPECT_NE(0.f, router.look(50).yaw);
  }

TEST(GamepadInput, CustomBindingsShareOwnershipAndKeepReservedControls) {
  auto router = input(Context::Gameplay);
  auto settings = router.settings();
  settings.bindings[size_t(Button::South)] = KeyCodec::Jump;
  settings.bindings[size_t(Button::West)] = KeyCodec::Jump;
  settings.bindings[size_t(Button::East)] = KeyCodec::Idle;
  settings.bindings[size_t(Button::Menu)] = KeyCodec::Idle;
  settings.bindings[size_t(Button::Home)] = KeyCodec::Jump;
  router.configure(settings, {});
  EXPECT_EQ(KeyCodec::Jump, router.process(edge(Button::South, true), 0).action);
  EXPECT_EQ(Type::None, router.process(edge(Button::West, true), 1).type);
  EXPECT_EQ(Type::None, router.process(edge(Button::South, false), 2).type);
  EXPECT_EQ(Type::Release, router.process(edge(Button::West, false), 3).type);
  EXPECT_EQ(Type::None, router.process(edge(Button::East, true), 4).type);
  EXPECT_EQ(KeyCodec::Escape, router.process(edge(Button::Menu, true), 5).action);
  EXPECT_EQ(Type::None, router.process(edge(Button::Home, true), 6).type);
  }

TEST(GamepadInput, CustomMovementHysteresisAndNeutralGateUseConfiguredRelease) {
  for(int deadZone : {5, 70}) {
    auto router = input(Context::Gameplay);
    auto settings = router.settings();
    settings.moveDeadZone = deadZone;
    router.configure(settings, {});
    Pad::State state;
    auto& y = state.axes[size_t(Pad::Axis::LeftY)];
    y = float(deadZone+19)/100.f;
    router.update(state, 0);
    EXPECT_EQ(Type::None, router.movement()[0].type);
    y = float(deadZone+21)/100.f;
    router.update(state, 1);
    EXPECT_EQ(Type::Press, router.movement()[0].type);
    y = float(deadZone+1)/100.f;
    router.update(state, 2);
    EXPECT_EQ(Type::None, router.movement()[0].type);
    y = float(deadZone)/100.f;
    router.update(state, 3);
    EXPECT_EQ(Type::Release, router.movement()[0].type);
    router.reset(state);
    y = 1;
    router.update(state, 4);
    EXPECT_EQ(Type::Press, router.movement()[0].type);
    }
  }

TEST(GamepadInput, CameraSettingsPreserveRadialCurveIndependentSensitivityAndDtClamp) {
  auto router = input(Context::Gameplay);
  auto settings = router.settings();
  settings.lookDeadZone = 0;
  settings.sensitivityX = 300;
  settings.sensitivityY = 25;
  settings.invertY = true;
  router.configure(settings, {});
  Pad::State state;
  state.axes[size_t(Pad::Axis::RightX)] = .1f;
  state.axes[size_t(Pad::Axis::RightY)] = .2f;
  router.update(state, 0);
  EXPECT_NEAR(-2.7f, router.look(50).yaw, .0001f);
  EXPECT_NEAR(-.3f, router.look(50).pitch, .0001f);
  EXPECT_FLOAT_EQ(router.look(50).yaw, router.look(1000).yaw);
  EXPECT_FLOAT_EQ(0.f, router.look(0).pitch);
  settings.lookDeadZone = 50;
  router.configure(settings, {});
  state.axes[size_t(Pad::Axis::RightX)] = .3f;
  state.axes[size_t(Pad::Axis::RightY)] = .4f;
  router.update(state, 1);
  EXPECT_FLOAT_EQ(0.f, router.look(50).yaw);
  state.axes[size_t(Pad::Axis::RightX)] = 1;
  state.axes[size_t(Pad::Axis::RightY)] = 1;
  router.update(state, 2);
  EXPECT_NEAR(1.f, std::hypot(router.look(50).yaw/27.f, router.look(50).pitch/1.5f), .0001f);
  }

TEST(GamepadInput, GameplaySettingsDoNotChangeUiButtonsOrStickThresholds) {
  for(auto context : {Context::Menu, Context::Dialog, Context::Inventory}) {
    auto router = input(context);
    auto settings = router.settings();
    settings.moveDeadZone = 5;
    for(auto button : GamepadSettings::remappableButtons)
      settings.bindings[size_t(button)] = KeyCodec::Idle;
    router.configure(settings, {});
    EXPECT_EQ(KeyCodec::ActionGeneric, router.process(edge(Button::South, true), 0).action);
    EXPECT_EQ(KeyCodec::Escape, router.process(edge(Button::East, true), 1).action);
    const auto square = router.process(edge(Button::West, true), 2);
    EXPECT_EQ(context==Context::Menu ? KeyCodec::UiSettings : context==Context::Inventory ? KeyCodec::UiAmount : KeyCodec::Idle, square.action);
    Pad::State state;
    state.axes[size_t(Pad::Axis::LeftY)] = .54f;
    EXPECT_EQ(Type::None, router.update(state, 3).type);
    state.axes[size_t(Pad::Axis::LeftY)] = .56f;
    EXPECT_EQ(KeyCodec::Forward, router.update(state, 4).action);
    state.axes[size_t(Pad::Axis::LeftY)] = .36f;
    EXPECT_EQ(KeyCodec::Forward, router.update(state, 404).action);
    state.axes[size_t(Pad::Axis::LeftY)] = .35f;
    EXPECT_EQ(Type::None, router.update(state, 405).type);
    }
  }
}

namespace {
TEST(GamepadInput, RemappedStatusHasPairedEdgesInGameplayAndCutsceneButUiStaysFixed) {
  for(auto context : {Context::Gameplay, Context::Cutscene, Context::Menu, Context::Dialog, Context::Inventory}) {
    auto router = input(context);
    auto settings = router.settings();
    settings.bindings[size_t(Button::South)] = KeyCodec::Status;
    router.configure(settings, {});
    const auto down = router.process(edge(Button::South, true), 0);
    const auto up = router.process(edge(Button::South, false), 1);
    if(context==Context::Gameplay || context==Context::Cutscene) {
      EXPECT_EQ(Type::Press, down.type);
      EXPECT_EQ(KeyCodec::Status, down.action);
      EXPECT_EQ(Type::Release, up.type);
      EXPECT_EQ(KeyCodec::Status, up.action);
      }
    else {
      EXPECT_EQ(Type::Navigate, down.type);
      EXPECT_EQ(KeyCodec::ActionGeneric, down.action);
      EXPECT_EQ(Type::None, up.type);
      }
    }
  }
}

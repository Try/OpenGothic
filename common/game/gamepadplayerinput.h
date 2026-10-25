#pragma once

#include "constants.h"
#include "utils/keycodec.h"

#include <cstddef>
#include <cstdint>

// Decisions shared by the controller adapter and its portable regression tests.
struct GamepadPlayerInput final {
  enum Combat : int8_t { None=-1, Forward=0, Back=1, Left=2, Right=3, Move=5, Kill=6 };

  static Combat combat(KeyCodec::Action action, WeaponState weapon, bool forward,
                       bool canFinish, bool running) {
    if(action==KeyCodec::ActionGeneric && weapon!=WeaponState::NoWeapon)
      return canFinish ? Kill : (forward ? Move : Forward);
    const bool melee = weapon==WeaponState::Fist || weapon==WeaponState::W1H || weapon==WeaponState::W2H;
    if(action==KeyCodec::Parade && melee)
      return Back;
    if(weapon!=WeaponState::NoWeapon && !running) {
      if(action==KeyCodec::ActionLeft)
        return Left;
      if(action==KeyCodec::ActionRight)
        return Right;
      }
    return None;
    }

  template<class Command, size_t Count>
  static void cancelWeaponRequests(Command (&commands)[Count], uint8_t source) {
    for(auto& command : commands)
      command.cancel(source);
    }

  template<class Command, size_t Count>
  static void requestWeapon(Command (&commands)[Count], size_t weapon, size_t action,
                            uint8_t owner, uint8_t replaceMask) {
    // Only controller requests replace each other. Keyboard requests retain
    // their existing lifetime, including after physical key release.
    if(replaceMask!=0)
      cancelWeaponRequests(commands, replaceMask);
    commands[weapon].issue(action, owner);
    }

  template<class Command, size_t Count>
  static uint8_t pendingSpell(const Command (&commands)[Count], size_t firstSpell,
                              uint8_t source, uint8_t current) {
    for(uint8_t i=0; i<8; ++i)
      if(commands[firstSpell+i].hasOrigin(source))
        return uint8_t(i+3);
    return current;
    }

  // Slot numbers use the game's public 3..10 numbering; zero means no spell.
  static uint8_t nextSpell(uint8_t occupied, uint8_t current, bool previous) {
    if(occupied==0)
      return 0;
    const bool valid = current>=3 && current<=10;
    int index = valid ? int(current)-3 : (previous ? 0 : 7);
    for(int step=0; step<8; ++step) {
      index = (index + (previous ? 7 : 1))%8;
      if((occupied & (1u<<index))!=0)
        return uint8_t(index+3);
      }
    return 0;
    }
  };

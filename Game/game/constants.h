#pragma once

#include <cstdint>

enum class WeaponState : uint8_t {
  NoWeapon,
  Fist,
  W1H,
  W2H,
  Bow,
  CBow,
  Mage
  };

enum class FightMode : int32_t {
  FMODE_NONE  = 0,
  FMODE_FIST  = 1,
  FMODE_MELEE = 2,
  FMODE_FAR   = 5,
  FMODE_MAGIC = 7,
  };

enum class WalkBit : uint8_t {
  WM_Run  =0,
  WM_Walk =1,
  WM_Sneak=2
  };

inline WalkBit operator & (WalkBit a,const WalkBit& b){
  return WalkBit(uint8_t(a)&uint8_t(b));
  }

inline WalkBit operator | (WalkBit a,const WalkBit& b){
  return WalkBit(uint8_t(a)|uint8_t(b));
  }

inline WalkBit operator ^ (WalkBit a,const WalkBit& b){
  return WalkBit(uint8_t(a)^uint8_t(b));
  }

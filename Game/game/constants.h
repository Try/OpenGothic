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

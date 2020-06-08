#pragma once

#include <tuple>
#include <daedalus/DaedalusStdlib.h>

#include "game/constants.h"

class Npc;
class Bullet;

class DamageCalculator {
  public:
    enum {
      MinDamage = 5
      };

    struct Val final {
      Val()=default;
      Val(int32_t v,bool b):value(v),hasHit(b){}
      Val(int32_t v,bool b,bool i):value(v),hasHit(b),invinsible(i){}

      int32_t value      = 0;
      bool    hasHit     = false;
      bool    invinsible = false;
      };

    static Val     damageValue(Npc& src, Npc& other, const Bullet* b, const CollideMask bMsk);
    static auto    rangeDamageValue(Npc& src) -> std::array<int32_t, Daedalus::GEngineClasses::DAM_INDEX_MAX>;
    static int32_t damageTypeMask(Npc& npc);

  private:
    static bool    checkDamageMask(Npc& src, Npc& other, const Bullet* b);

    static Val     rangeDamage(Npc& src, Npc& other, const Bullet& b, const CollideMask bMsk);
    static Val     swordDamage(Npc& src, Npc& other);
  };


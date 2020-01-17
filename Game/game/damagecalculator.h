#pragma once

#include <tuple>
#include <daedalus/DaedalusStdlib.h>

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

      int32_t value  = 0;
      bool    hasHit = false;
      };

    static Val     damageValue(Npc& src, Npc& other, const Bullet* b);
    static auto    rangeDamageValue(Npc& src) -> std::array<int32_t, Daedalus::GEngineClasses::DAM_INDEX_MAX>;
    static int32_t damageTypeMask(Npc& npc);

  private:
    static bool    checkDamageMask(Npc& src, Npc& other, const Bullet* b);

    static Val     bowDamage  (Npc& src, Npc& other, const Bullet& b);
    static Val     swordDamage(Npc& src, Npc& other);
  };


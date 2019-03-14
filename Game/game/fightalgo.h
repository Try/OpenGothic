#pragma once

#include <cstdint>

class Npc;
class WorldScript;

class FightAlgo final {
  public:
    FightAlgo();

    enum Action : uint8_t {
      MV_ATACK=0,
      MV_MOVE =1
      };

    Action tick(Npc& npc, Npc& tg, WorldScript &owner, uint64_t dt);
    float  prefferedAtackDistance(const Npc &npc, const Npc &tg, WorldScript &owner) const;
    bool   isInAtackRange        (const Npc &npc, const Npc &tg, WorldScript &owner);
  };

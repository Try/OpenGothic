#pragma once

#include <cstdint>

class Npc;

class FightAlgo final {
  public:
    FightAlgo();

    enum Action : uint8_t {
      MV_ATACK=0,
      MV_MOVE =1
      };

    Action tick(Npc& npc,Npc& tg,uint64_t dt);
    bool   isInAtackRange(Npc& npc,Npc& target);
  };

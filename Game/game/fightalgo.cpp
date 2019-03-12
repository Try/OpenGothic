#include "fightalgo.h"

#include "world/npc.h"

FightAlgo::FightAlgo() {
  }

FightAlgo::Action FightAlgo::tick(Npc &npc, Npc &tg, uint64_t /*dt*/) {
  if(isInAtackRange(npc,tg))
    return MV_ATACK;

  return MV_MOVE;
  }

bool FightAlgo::isInAtackRange(Npc &npc, Npc &tg) {
  auto dist = npc.qDistTo(tg);
  return (dist<200*200);
  }

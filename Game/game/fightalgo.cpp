#include "fightalgo.h"

#include "world/npc.h"

FightAlgo::FightAlgo() {
  }

FightAlgo::Action FightAlgo::tick(Npc &npc, Npc &tg, uint64_t /*dt*/) {
  if(isInAtackRange(npc,tg))
    return MV_ATACK;

  return MV_MOVE;
  }

float FightAlgo::prefferedAtackDistance(const Npc &/*npc*/) const {
  return 200; // TODO
  }

bool FightAlgo::isInAtackRange(const Npc &npc,const Npc &tg) {
  auto dist = npc.qDistTo(tg);
  auto pd   = prefferedAtackDistance(npc);
  return (dist<pd*pd);
  }

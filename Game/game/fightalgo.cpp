#include "fightalgo.h"

#include "world/npc.h"

FightAlgo::FightAlgo() {
  }

FightAlgo::Action FightAlgo::tick(Npc &npc, Npc &tg, WorldScript& owner, uint64_t /*dt*/) {
  if(isInAtackRange(npc,tg,owner))
    return MV_ATACK;

  return MV_MOVE;
  }

float FightAlgo::prefferedAtackDistance(const Npc &npc, WorldScript &owner) const {
  auto  gl   = std::min<uint32_t>(npc.guild(),GIL_MAX);
  float g    = owner.guildVal().fight_range_g[gl];
  float base = owner.guildVal().fight_range_base[gl];
  float fist = owner.guildVal().fight_range_fist[gl];
  return base+fist+g; // TODO
  }

bool FightAlgo::isInAtackRange(const Npc &npc,const Npc &tg, WorldScript &owner) {
  auto dist = npc.qDistTo(tg);
  auto pd   = prefferedAtackDistance(npc,owner);
  return (dist<pd*pd);
  }

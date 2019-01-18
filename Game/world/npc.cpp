#include "npc.h"

Npc::Npc() {
  }

Npc::~Npc(){
  }

void Npc::setName(const std::string &n) {
  name = n;
  }

void Npc::setTalentSkill(Npc::Talent t, int32_t lvl) {
  if(t<NPC_TALENT_MAX)
    talents[t] = lvl;
  }

void Npc::equipItem() {
  }

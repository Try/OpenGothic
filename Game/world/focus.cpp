#include "focus.h"

#include "world/npc.h"
#include "world/interactive.h"

Focus::Focus(Interactive &i):interactive(&i){
  }

Focus::Focus(Npc &i):npc(&i){
  }

Focus::operator bool() const {
  return interactive || npc;
  }

std::array<float,3> Focus::displayPosition() const {
  if(interactive)
    return interactive->displayPosition();
  if(npc)
    return npc->position();
  return {{}};
  }

const char *Focus::displayName() const {
  if(interactive)
    return interactive->displayName();
  if(npc)
    return npc->displayName();
  return nullptr;
  }

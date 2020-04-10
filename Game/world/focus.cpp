#include "focus.h"

#include "world/npc.h"
#include "world/interactive.h"
#include "world/item.h"

Focus::Focus(Interactive &i):interactive(&i){
  }

Focus::Focus(Npc &i):npc(&i){
  }

Focus::Focus(Item &i):item(&i){
  }

Focus::operator bool() const {
  return interactive || npc || item;
  }

Tempest::Vec3 Focus::displayPosition() const {
  if(interactive)
    return interactive->displayPosition();
  if(npc)
    return npc->displayPosition();
  if(item)
    return item->position();
  return {};
  }

const char *Focus::displayName() const {
  if(interactive)
    return interactive->displayName();
  if(npc)
    return npc->displayName();
  if(item)
    return item->displayName();
  return nullptr;
  }

#include "touchdamage.h"

#include "world/objects/npc.h"
#include "world/world.h"
#include "game/serialize.h"

TouchDamage::TouchDamage(Vob* parent, World &world, const std::unique_ptr<phoenix::vob>& d, Flags flags)
  :AbstractTrigger(parent,world,d,flags) {
  auto* dmg = (const phoenix::vobs::touch_damage*) d.get();
  barrier = dmg->barrier;
  blunt = dmg->blunt;
  edge = dmg->edge;
  fire = dmg->fire;
  fly = dmg->fly;
  magic = dmg->magic;
  point = dmg->point;
  fall = dmg->fall;
  damage = dmg->damage;
  repeatDelaySec = dmg->repeat_delay_sec;
  }

void TouchDamage::onTrigger(const TriggerEvent&/*evt*/) {
  }

void TouchDamage::onIntersect(Npc& n) {
  AbstractTrigger::onIntersect(n);
  enableTicks();
  }

void TouchDamage::tick(uint64_t dt) {
  AbstractTrigger::tick(dt);

  if(world.tickCount()<=repeatTimeout)
    return;

  for(auto npc:intersections()) {
    bool mask[phoenix::daedalus::damage_type::count] = {};
    mask[phoenix::daedalus::damage_type::barrier] = barrier;
    mask[phoenix::daedalus::damage_type::blunt]   = blunt;
    mask[phoenix::daedalus::damage_type::edge]    = edge;
    mask[phoenix::daedalus::damage_type::fire]    = fire;
    mask[phoenix::daedalus::damage_type::fly]     = fly;
    mask[phoenix::daedalus::damage_type::magic]   = magic;
    mask[phoenix::daedalus::damage_type::point]   = point;
    mask[phoenix::daedalus::damage_type::fall]    = fall;

    auto& hnpc = *npc->handle();
    for(size_t i=0; i<phoenix::daedalus::damage_type::count; ++i) {
      if(!mask[i])
        continue;
      takeDamage(*npc,int32_t(damage),hnpc.protection[i]);
      }
    }

  repeatTimeout = world.tickCount() + uint64_t(repeatDelaySec*1000);

  if(intersections().empty())
    disableTicks();
  }

void TouchDamage::takeDamage(Npc& npc, int32_t val, int32_t prot) {
  if(prot<0) // Filter immune
    return;
  npc.changeAttribute(ATR_HITPOINTS,-std::max(val-prot,0),false);
  }

#include "touchdamage.h"

#include "world/objects/npc.h"
#include "world/world.h"
#include "game/serialize.h"

TouchDamage::TouchDamage(Vob* parent, World &world, const phoenix::vobs::touch_damage& dmg, Flags flags)
  :AbstractTrigger(parent,world,dmg,flags) {
  barrier = dmg.barrier;
  blunt = dmg.blunt;
  edge = dmg.edge;
  fire = dmg.fire;
  fly = dmg.fly;
  magic = dmg.magic;
  point = dmg.point;
  fall = dmg.fall;
  damage = dmg.damage;
  repeatDelaySec = dmg.repeat_delay_sec;
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
    bool mask[phoenix::damage_type::count] = {};
    mask[phoenix::damage_type::barrier] = barrier;
    mask[phoenix::damage_type::blunt]   = blunt;
    mask[phoenix::damage_type::edge]    = edge;
    mask[phoenix::damage_type::fire]    = fire;
    mask[phoenix::damage_type::fly]     = fly;
    mask[phoenix::damage_type::magic]   = magic;
    mask[phoenix::damage_type::point]   = point;
    mask[phoenix::damage_type::fall]    = fall;

    auto& hnpc = npc->handle();
    for(size_t i=0; i<phoenix::damage_type::count; ++i) {
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

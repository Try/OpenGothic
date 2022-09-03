#include "touchdamage.h"

#include <daedalus/DaedalusStdlib.h>

#include "world/objects/npc.h"
#include "world/world.h"
#include "game/serialize.h"

TouchDamage::TouchDamage(Vob* parent, World &world, const std::unique_ptr<phoenix::vobs::vob>& d, Flags flags)
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
    bool mask[Daedalus::GEngineClasses::PROT_INDEX_MAX] = {};
    mask[Daedalus::GEngineClasses::PROT_BARRIER] = barrier;
    mask[Daedalus::GEngineClasses::PROT_BLUNT]   = blunt;
    mask[Daedalus::GEngineClasses::PROT_EDGE]    = edge;
    mask[Daedalus::GEngineClasses::PROT_FIRE]    = fire;
    mask[Daedalus::GEngineClasses::PROT_FLY]     = fly;
    mask[Daedalus::GEngineClasses::PROT_MAGIC]   = magic;
    mask[Daedalus::GEngineClasses::PROT_POINT]   = point;
    mask[Daedalus::GEngineClasses::PROT_FALL]    = fall;

    auto& hnpc = *npc->handle();
    for(size_t i=0; i<Daedalus::GEngineClasses::PROT_INDEX_MAX; ++i) {
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

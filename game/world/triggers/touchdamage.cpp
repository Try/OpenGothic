#include "touchdamage.h"

#include <daedalus/DaedalusStdlib.h>

#include "world/objects/npc.h"
#include "world/world.h"
#include "game/serialize.h"

TouchDamage::TouchDamage(Vob* parent, World &world, ZenLoad::zCVobData&& d, Flags flags)
  :AbstractTrigger(parent,world,std::move(d),flags) {
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
    mask[Daedalus::GEngineClasses::PROT_BARRIER] = data.oCTouchDamage.touchDamage.barrier;
    mask[Daedalus::GEngineClasses::PROT_BLUNT]   = data.oCTouchDamage.touchDamage.blunt;
    mask[Daedalus::GEngineClasses::PROT_EDGE]    = data.oCTouchDamage.touchDamage.edge;
    mask[Daedalus::GEngineClasses::PROT_FIRE]    = data.oCTouchDamage.touchDamage.fire;
    mask[Daedalus::GEngineClasses::PROT_FLY]     = data.oCTouchDamage.touchDamage.fly;
    mask[Daedalus::GEngineClasses::PROT_MAGIC]   = data.oCTouchDamage.touchDamage.magic;
    mask[Daedalus::GEngineClasses::PROT_POINT]   = data.oCTouchDamage.touchDamage.point;
    mask[Daedalus::GEngineClasses::PROT_FALL]    = data.oCTouchDamage.touchDamage.fall;

    auto& hnpc = *npc->handle();
    for(size_t i=0; i<Daedalus::GEngineClasses::PROT_INDEX_MAX; ++i) {
      if(!mask[i])
        continue;
      takeDamage(*npc,int32_t(data.oCTouchDamage.damage),hnpc.protection[i]);
      }
    }

  repeatTimeout = world.tickCount() + uint64_t(data.oCTouchDamage.damageRepeatDelaySec*1000);

  if(intersections().empty())
    disableTicks();
  }

void TouchDamage::takeDamage(Npc& npc, int32_t val, int32_t prot) {
  if(prot<0) // Filter immune
    return;
  npc.changeAttribute(ATR_HITPOINTS,-std::max(val-prot,0),false);
  }

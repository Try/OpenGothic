#include "touchdamage.h"

#include <limits>

#include "world/objects/npc.h"
#include "world/world.h"
#include "game/serialize.h"

TouchDamage::TouchDamage(Vob* parent, World &world, const zenkit::VTouchDamage& dmg, Flags flags)
  :AbstractTrigger(parent,world,dmg,flags) {
  barrier        = dmg.barrier;
  blunt          = dmg.blunt;
  edge           = dmg.edge;
  fire           = dmg.fire;
  fly            = dmg.fly;
  magic          = dmg.magic;
  point          = dmg.point;
  fall           = dmg.fall;
  damage         = dmg.damage;
  repeatDelaySec = dmg.repeat_delay_sec;
  }

void TouchDamage::onTrigger(const TriggerEvent&/*evt*/) {
  }

void TouchDamage::onIntersect(Npc& n) {
  AbstractTrigger::onIntersect(n);
  if(intersections().size()==1) // first occupant after being empty: allow the entry hit
    repeatTimeout = 0;
  enableTicks();
  }

void TouchDamage::tick(uint64_t dt) {
  AbstractTrigger::tick(dt);

  if(world.tickCount()<=repeatTimeout)
    return;

  for(auto npc:intersections()) {
    bool mask[zenkit::DamageType::NUM] = {};
    mask[zenkit::DamageType::BARRIER] = barrier;
    mask[zenkit::DamageType::BLUNT]   = blunt;
    mask[zenkit::DamageType::EDGE]    = edge;
    mask[zenkit::DamageType::FIRE]    = fire;
    mask[zenkit::DamageType::FLY]     = fly;
    mask[zenkit::DamageType::MAGIC]   = magic;
    mask[zenkit::DamageType::POINT]   = point;
    mask[zenkit::DamageType::FALL]    = fall;

    auto& hnpc = npc->handle();
    // NOTE: in original-game oCNpc::OnDamage_Hit (Gothic2.exe 0x00666610) a DAM_BARRIER hit on a
    // victim that is swimming/diving (water level > 1) overrides the damage with the victim's
    // full current HP -- an instant kill (the magic barrier over deep water drowns you). Land
    // hits keep the normal flat trigger damage.
    if(mask[zenkit::DamageType::BARRIER] && npc->isSwim()) {
      npc->changeAttribute(ATR_HITPOINTS,-hnpc.attribute[ATR_HITPOINTS],false);
      continue;
      }
    for(size_t i=0; i<zenkit::DamageType::NUM; ++i) {
      if(!mask[i])
        continue;
      takeDamage(*npc,int32_t(damage),hnpc.protection[i]);
      }
    }

  // NOTE: in original-game zCTouchDamage::OnTouch/OnTimer (Gothic2.exe 0x615b70/0x615c70)
  // the repeat timer is armed only when repeatDelaySec>0; with repeatDelaySec==0 (also the
  // ctor default) damage is dealt exactly once per entry, not every frame.
  if(repeatDelaySec>0)
    repeatTimeout = world.tickCount() + uint64_t(repeatDelaySec*1000);
  else
    repeatTimeout = std::numeric_limits<uint64_t>::max();

  if(intersections().empty())
    disableTicks();
  }

void TouchDamage::takeDamage(Npc& npc, int32_t val, int32_t prot) {
  if(prot<0) // Filter immune
    return;
  npc.changeAttribute(ATR_HITPOINTS,-std::max(val-prot,0),false);
  }

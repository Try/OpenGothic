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
  // NOTE: in original-game zCTouchDamage::OnTouch (Gothic2.exe 0x00615b70) is a collision-enter
  // callback fired once per *entering* vob, and it deals that vob an immediate "entry" hit
  // regardless of how many other vobs are already inside; the shared OnTimer repeat (0x00615c70) is
  // a separate mechanism. OpenGothic granted the entry hit only on the empty->first transition, so a
  // vob entering an already-occupied zone (a second NPC on a one-shot spike trap) took no hit. Deal
  // the entry hit per-entrant here; arm the shared repeat cadence only on the first occupant.
  applyDamage(n);
  if(intersections().size()==1) {
    if(repeatDelaySec>0)
      repeatTimeout = world.tickCount() + uint64_t(repeatDelaySec*1000);
    else
      repeatTimeout = std::numeric_limits<uint64_t>::max();
    }
  enableTicks();
  }

void TouchDamage::applyDamage(Npc& npcRef) {
  Npc* npc = &npcRef;
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
  // victim that is swimming/diving (water level > 1) overrides the damage with the victim's full
  // current HP -- an instant kill (the magic barrier over deep water drowns you). Land hits keep
  // the normal flat trigger damage.
  if(mask[zenkit::DamageType::BARRIER] && npc->isSwim()) {
    npc->changeAttribute(ATR_HITPOINTS,-hnpc.attribute[ATR_HITPOINTS],false);
    return;
    }
  // NOTE: in original-game oCNpc::OnDamage_Hit @0x00666610 -> ApplyDamages @0x0065e5a0, a touch zone
  // provides a single scalar `damage` plus a multi-bit type mask; the engine splits the damage
  // evenly across the set types (damage/numSetTypes, truncated), subtracts per-type protection,
  // floors each share at 0, sums them and applies ONE HP change.
  int32_t nTypes = 0;
  for(size_t i=0; i<zenkit::DamageType::NUM; ++i)
    if(mask[i])
      ++nTypes;
  if(nTypes>0) {
    const int32_t share = int32_t(damage)/nTypes;
    int32_t total = 0;
    for(size_t i=0; i<zenkit::DamageType::NUM; ++i) {
      if(!mask[i] || hnpc.protection[i]<0) // skip unset & immune (-1 protection) types
        continue;
      total += std::max(share-hnpc.protection[i],0);
      }
    npc->changeAttribute(ATR_HITPOINTS,-total,false);
    }
  }

void TouchDamage::tick(uint64_t dt) {
  AbstractTrigger::tick(dt);

  if(world.tickCount()<=repeatTimeout) {
    if(intersections().empty())
      disableTicks();
    return;
    }

  // shared OnTimer repeat: re-hit every current occupant on the repeat cadence (the per-vob entry
  // hit is dealt in onIntersect).
  for(auto npc:intersections())
    applyDamage(*npc);

  // NOTE: in original-game zCTouchDamage::OnTimer (Gothic2.exe 0x00615c70) the shared repeat is
  // re-armed only when repeatDelaySec>0; with repeatDelaySec==0 (also the ctor default) damage is
  // dealt exactly once per entry, not every frame.
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

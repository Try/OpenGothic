#include "damagecalculator.h"

#include "world/objects/npc.h"
#include "world/objects/item.h"
#include "world/world.h"
#include "world/bullet.h"
#include "gothic.h"

// https://forum.worldofplayers.de/forum/threads/127320-Damage-System?p=2198181#post2198181
// https://strafkolonie-online.net/forum/board/thread/1895-info-erkl%C3%A4rung-der-berechnung-der-trefferchance-im-fernkampf/

static float mix(float x, float y, float a) {
  return x + (y-x)*a;
  }

DamageCalculator::Val DamageCalculator::damageValue(Npc& src, Npc& other, const Bullet* b, bool isSpell, const DamageCalculator::Damage& splDmg, const CollideMask bMsk) {
  DamageCalculator::Val ret;
  if(b!=nullptr) {
    ret = rangeDamage(src,other,*b,bMsk);
    } else
  if(isSpell) {
    ret = rangeDamage(src,other,splDmg,bMsk);
    }
  else {
    ret = swordDamage(src,other);
    }

#if 0
  // debug
  ret.value = MinDamage;
#endif

  // NOTE: in original-game oCNpc::OnDamage_Hit (Gothic2.exe 0x00666610) applies the
  // NPC_MINIMAL_DAMAGE (5) floor only to non-magic hits (the damage-descriptor magic vob is
  // null); spell damage is NOT floored, so a resisted/weak spell can deal < 5. Flooring
  // spells too made weak spells hit for a spurious minimum.
  if(ret.hasHit && !ret.invincible && !isSpell && Gothic::inst().version().game==2)
    ret.value = std::max<int32_t>(ret.value,MinDamage);
  return ret;
  }

DamageCalculator::Val DamageCalculator::damageFall(Npc& npc, float speed) {
  auto  gl = npc.guild();
  auto& g  = npc.world().script().guildVal();

  float   gravity     = DynamicWorld::gravity;
  float   fallTime    = speed/gravity;
  float   height      = 0.5f*std::abs(gravity)*fallTime*fallTime;
  // NOTE: in original-game oCNpc::SetTalentSkill (Gothic2.exe 0x00730f60) the ACROBAT talent
  // doubles the per-NPC fall-down height (offset 0x90c) consumed by CreateFallDamage; mirror
  // that 2x safe-fall threshold so acrobatics actually reduces fall damage.
  float   h0          = float(g.falldown_height[gl]);
  if(npc.talentSkill(TALENT_ACROBAT)>0)
    h0 *= 2.f;
  float   dmgPerMeter = float(g.falldown_damage[gl]);
  int32_t prot        = npc.protection(::PROT_FALL);

  Val ret;
  ret.invincible = (prot<0);
  // NOTE: in original-game oCNpc::CreateFallDamage (Gothic2.exe 0x00681da0) computes
  // damage from (fallDist + 50 - falldown_height): a fixed +50cm tolerance is added to the
  // fall distance. Omitting it under-counted every fall (less damage, higher no-damage
  // threshold).
  ret.value      = int32_t(dmgPerMeter*(height+50.f-h0)/100.f - float(prot));
  if(ret.value<=0 || ret.invincible) {
    ret.value = 0;
    return ret;
    }
  ret.hasHit = true;
  return ret;
  }

DamageCalculator::Val DamageCalculator::rangeDamage(Npc& nsrc, Npc& nother, const Bullet& b, const CollideMask bMsk) {
  float dist       = b.pathLength();
  // NOTE: in original-game a spell applies damage on physical collision with no path-length
  // gate (oCVisualFX::ProcessCollision @0x004958d0); only the arrow/bolt path has a range
  // falloff. OpenGothic zeroed spell damage past MaxMagRange(3500cm) even though spell bullets
  // fly to ~10000cm, so distant spell hits dealt 0. Apply the cutoff to non-spell bullets only.
  bool  noHit      = !b.isSpell() && dist>float(MaxMagRange);
  bool  invincible = !checkDamageMask(nsrc,nother,&b);
  auto  dmg        = b.damage();

  if(!b.isSpell()) {
    auto& script    = nsrc.world().script();
    float hitChance = float(script.rand(100))/100.f;
    float hitCh     = 0;
    bool  g2        = Gothic::inst().version().game==2;
    // NOTE: in original-game oCAIArrow::ReportCollisionToAI @0x006a18a2 the G2 falloff uses the
    // RANGED_CHANCE_MINDIST(1000)/RANGED_CHANCE_MAXDIST(10000) defaults, so the hit chance
    // decays to 0 only at 10000cm with no hard cutoff at 4500cm. The G1 path is left on the
    // prior 2000/4500 ranges (not analyzed against Gothic1.exe).
    float refRange  = g2 ? float(RangedChanceMinDist) : float(ReferenceBowRangeG1);
    float maxRange  = g2 ? float(RangedChanceMaxDist) : float(MaxBowRange);
    float chance    = b.hitChance();

    if(dist<refRange)
      hitCh = mix(1.f, chance, (dist / refRange));
    else if(dist<maxRange)
      hitCh = mix(chance, 0.f, (dist-refRange) / (maxRange-refRange));
    else
      hitCh = 0;

    noHit = (dist>maxRange || hitCh<=hitChance);

    if(!g2 && !noHit && !invincible) {
      const int32_t mul        = script.criticalDamageMultiplyer();
      const int     critChance = int(script.rand(100));
      if(std::lround(100.f * b.critChance())>critChance)
        dmg *= mul;
      }
    }

  if(noHit)
    return Val(0,false,invincible);

  if(invincible)
    return Val(0,true,true);

  if((bMsk & (COLL_APPLYDAMAGE | COLL_APPLYDOUBLEDAMAGE | COLL_APPLYHALVEDAMAGE | COLL_DOEVERYTHING))==0)
    return Val(0,true,true);

  return rangeDamage(nsrc,nother,dmg,bMsk);
  }

DamageCalculator::Val DamageCalculator::rangeDamage(Npc&, Npc& nother, Damage dmg, const CollideMask bMsk) {
  auto& other = nother.handle();

  if(bMsk & COLL_APPLYDOUBLEDAMAGE)
    dmg*=2;
  if(bMsk & COLL_APPLYHALVEDAMAGE)
    dmg/=2;

  int  value = 0;
  bool invincible = true;
  for(unsigned int i=0; i<zenkit::DamageType::NUM; ++i) {
    if(dmg[size_t(i)]==0)
      continue;
    int vd = std::max(dmg[size_t(i)] - other.protection[i],0);
    if(other.protection[i]>=0) { // Filter immune
      value     += vd;
      invincible = false;
      }
    }

  return Val(value,true,invincible);
  }

DamageCalculator::Val DamageCalculator::swordDamage(Npc& nsrc, Npc& nother) {
  if(!checkDamageMask(nsrc,nother,nullptr))
    return Val(0,true,true);

  auto& script = nsrc.world().script();
  auto& src    = nsrc.handle();
  auto& other  = nother.handle();

  // Swords/Fists
  const int dtype      = damageTypeMask(nsrc);
  Talent    tal        = TALENT_UNKNOWN;
  int       str        = nsrc.attribute(Attribute::ATR_STRENGTH);
  int       dex        = nsrc.attribute(Attribute::ATR_DEXTERITY);
  int       critChance = int(script.rand(100));

  int value = 0;

  if(auto w = nsrc.inventory().activeWeapon()) {
    if(w->is2H())
      tal = TALENT_2H; else
      tal = TALENT_1H;
    }

  if(Gothic::inst().version().game==2) {
    if(nsrc.isMonster() && tal==TALENT_UNKNOWN) {
      // regular monsters always do critical damage
      critChance = -1;
      }

    bool invincible = true;
    for(unsigned int i=0; i<zenkit::DamageType::NUM; ++i) {
      if((dtype & (1<<i))==0)
        continue;
      // NOTE: in original-game oCNpc::OnDamage_Hit (Gothic2.exe 0x00666610) the per-type melee
      // attribute boni adds STRENGTH to BLUNT/EDGE but DEXTERITY to POINT (GetAttribute(5)=DEX
      // feeds the point accumulator @desc+0x44, GetAttribute(4)=STR feeds blunt/edge @+0x30/+0x34).
      const int atr = (i==zenkit::DamageType::POINT) ? dex : str;
      int vd = std::max(atr + src.damage[i] - other.protection[i],0);
      if(src.hitchance[tal]<=critChance)
        vd = (vd-1)/10;
      if(other.protection[i]>=0) { // Filter immune
        value += vd;
        invincible = false;
        }
      }

    return Val(value,true,invincible);
    } else {
    bool invincible = true;
    const int32_t mul = script.criticalDamageMultiplyer();
    for(unsigned int i=0; i<zenkit::DamageType::NUM; ++i) {
      if((dtype & (1<<i))==0)
        continue;
      int vd = 0;
      if(nsrc.talentValue(tal)<=critChance)
        vd = std::max(str +     src.damage[i] - other.protection[i],0); else
        vd = std::max(str + mul*src.damage[i] - other.protection[i],0);
      if(other.protection[i]>=0) { // Filter immune
        value += vd;
        invincible = false;
        }
      }

    return Val(value,true,invincible);
    }
  }

int32_t DamageCalculator::damageTypeMask(Npc& npc) {
  if(auto w = npc.inventory().activeWeapon())
    return w->handle().damage_type;
  return npc.handle().damage_type;
  }

bool DamageCalculator::checkDamageMask(Npc& nsrc, Npc& nother, const Bullet* b) {
  auto& other = nother.handle();

  if(b!=nullptr) {
    auto dmg = b->damage();
    for(unsigned int i=0;i<zenkit::DamageType::NUM;++i) {
      if(dmg[size_t(i)]>0 && other.protection[i]>=0)
        return true;
      }
    } else {
    const int dtype = damageTypeMask(nsrc);
    for(unsigned int i=0;i<zenkit::DamageType::NUM;++i){
      if((dtype & (1<<i))==0)
        continue;
      return true;
      }
    }

  return false;
  }

DamageCalculator::Damage DamageCalculator::rangeDamageValue(Npc& src) {
  const int dtype = damageTypeMask(src);
  int d = Gothic::inst().version().game==2 ? src.attribute(Attribute::ATR_DEXTERITY) : 0;
  Damage ret={};
  for(unsigned int i=0;i<zenkit::DamageType::NUM;++i){
    if((dtype & (1<<i))==0)
      continue;
    ret[size_t(i)] = d + src.handle().damage[i];
    }
  return ret;
  }

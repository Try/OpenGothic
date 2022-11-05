#include "damagecalculator.h"

#include "world/objects/npc.h"
#include "world/objects/item.h"
#include "world/world.h"
#include "world/bullet.h"
#include "gothic.h"

// https://forum.worldofplayers.de/forum/threads/127320-Damage-System?p=2198181#post2198181


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

  if(ret.hasHit && !ret.invinsible)
    ret.value = std::max<int32_t>(ret.value,MinDamage);
  if(other.isImmortal())
    ret.value = 0;
  return ret;
  }

DamageCalculator::Val DamageCalculator::damageFall(Npc& npc, float speed) {
  auto  gl = npc.guild();
  auto& g  = npc.world().script().guildVal();

  float   gravity     = DynamicWorld::gravity;
  float   fallTime    = speed/gravity;
  float   height      = 0.5f*std::abs(gravity)*fallTime*fallTime;
  float   h0          = float(g.falldown_height[gl]);
  float   dmgPerMeter = float(g.falldown_damage[gl]);
  int32_t prot        = npc.protection(::PROT_FALL);

  Val ret;
  ret.invinsible = (prot<0 || npc.isImmortal());
  ret.value      = int32_t(dmgPerMeter*(height-h0)/100.f - float(prot));
  if(ret.value<=0 || ret.invinsible) {
    ret.value = 0;
    return ret;
    }
  ret.hasHit = true;
  return ret;
  }

DamageCalculator::Val DamageCalculator::rangeDamage(Npc& nsrc, Npc& nother, const Bullet& b, const CollideMask bMsk) {
  bool invinsible = !checkDamageMask(nsrc,nother,&b);
  if(b.pathLength() > float(MaxBowRange) * b.hitChance() && b.hitChance()<1.f)
    return Val(0,false,invinsible);

  if(invinsible)
    return Val(0,true,true);

  if((bMsk & (COLL_APPLYDAMAGE | COLL_APPLYDOUBLEDAMAGE | COLL_APPLYHALVEDAMAGE | COLL_DOEVERYTHING))==0)
    return Val(0,true,true);

  return rangeDamage(nsrc,nother,b.damage(),bMsk);
  }

DamageCalculator::Val DamageCalculator::rangeDamage(Npc&, Npc& nother, Damage dmg, const CollideMask bMsk) {
  auto& other = nother.handle();

  if(bMsk & COLL_APPLYDOUBLEDAMAGE)
    dmg*=2;
  if(bMsk & COLL_APPLYHALVEDAMAGE)
    dmg/=2;

  int  value = 0;
  for(unsigned int i=0; i<phoenix::damage_type::count; ++i) {
    if(dmg[size_t(i)]==0)
      continue;
    int vd = std::max(dmg[size_t(i)] - other.protection[i],0);
    if(other.protection[i]>=0) // Filter immune
      value  += vd;
    }

  return Val(value,true);
  }

DamageCalculator::Val DamageCalculator::swordDamage(Npc& nsrc, Npc& nother) {
  if(!checkDamageMask(nsrc,nother,nullptr))
    return Val(0,true,true);

  auto&  script = nsrc.world().script();
  auto& src    = nsrc.handle();
  auto& other  = nother.handle();

  // Swords/Fists
  const int dtype      = damageTypeMask(nsrc);
  uint8_t   hitCh      = TALENT_UNKNOWN;
  int       str        = nsrc.attribute(Attribute::ATR_STRENGTH);
  int       critChance = int(script.rand(100));

  int  value=0;

  if(auto w = nsrc.inventory().activeWeapon()){
    if(w->is2H())
      hitCh = TALENT_2H; else
      hitCh = TALENT_1H;
    }

  if(Gothic::inst().version().game==2) {
    if(nsrc.isMonster() && hitCh==TALENT_UNKNOWN) {
      // regular monsters always do critical damage
      critChance = 0;
      }

    for(unsigned int i=0; i<phoenix::damage_type::count; ++i){
      if((dtype & (1<<i))==0)
        continue;
      int vd = std::max(str + src.damage[i] - other.protection[i],0);
      if(src.hitchance[hitCh]<critChance)
        vd = (vd-1)/10;
      if(other.protection[i]>=0) // Filter immune
        value += vd;
      }

    return Val(value,true);
    } else {
    for(unsigned int i=0; i<phoenix::damage_type::count; ++i) {
      if((dtype & (1<<i))==0)
        continue;
      int vd = std::max(str + src.damage[i] - other.protection[i],0);
      if(src.hitchance[hitCh]<critChance)
        vd = std::max(str + src.damage[i]   - other.protection[i],0); else
        vd = std::max(str + src.damage[i]*2 - other.protection[i],0);
      if(other.protection[i]>=0) // Filter immune
        value += vd;
      }

    return Val(value,true);
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
    for(unsigned int i=0;i<phoenix::damage_type::count;++i) {
      if(dmg[size_t(i)]>0 && other.protection[i]>=0)
        return true;
      }
    } else {
    const int dtype = damageTypeMask(nsrc);
    for(unsigned int i=0;i<phoenix::damage_type::count;++i){
      if((dtype & (1<<i))==0)
        continue;
      return true;
      }
    }

  return false;
  }

DamageCalculator::Damage DamageCalculator::rangeDamageValue(Npc& src) {
  const int dtype = damageTypeMask(src);
  int d = src.attribute(Attribute::ATR_DEXTERITY);
  Damage ret={};
  for(unsigned int i=0;i<phoenix::damage_type::count;++i){
    if((dtype & (1<<i))==0)
      continue;
    ret[size_t(i)] = d + src.handle().damage[i];
    }
  return ret;
  }

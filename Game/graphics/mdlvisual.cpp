#include "mdlvisual.h"

#include "graphics/skeleton.h"
#include "game/serialize.h"
#include "world/npc.h"
#include "world/item.h"

using namespace Tempest;

MdlVisual::MdlVisual() {
  }

void MdlVisual::save(Serialize &fout) {
  if(skeleton!=nullptr)
    fout.write(skeleton->name()); else
    fout.write(std::string(""));
  }

void MdlVisual::load(Serialize &fin,Npc& npc) {
  std::string s;

  fin.read(s);
  npc.setVisual(s.c_str());
  }

void MdlVisual::setPos(const Tempest::Matrix4x4 &m) {
  // TODO: deferred setObjMatrix
  pos = m;
  head  .setObjMatrix(pos);
  sword .setObjMatrix(pos);
  bow   .setObjMatrix(pos);
  pfx   .setObjMatrix(pos);
  view  .setObjMatrix(pos);
  }

// mdl_setvisual
void MdlVisual::setVisual(const Skeleton *v) {
  skeleton = v;
  head  .setAttachPoint(skeleton);
  view  .setAttachPoint(skeleton);
  setPos(pos); // update obj matrix
  }

// mdl_setvisualbody
void MdlVisual::setVisualBody(StaticObjects::Mesh &&h, StaticObjects::Mesh &&body) {
  head    = std::move(h);
  view    = std::move(body);

  head.setAttachPoint(skeleton,"BIP01 HEAD");
  view.setAttachPoint(skeleton);
  }

void MdlVisual::setArmour(StaticObjects::Mesh &&a) {
  view = std::move(a);
  view.setAttachPoint(skeleton);
  setPos(pos);
  }

void MdlVisual::setSword(StaticObjects::Mesh &&s) {
  sword = std::move(s);
  setPos(pos);
  }

void MdlVisual::setRangeWeapon(StaticObjects::Mesh &&b) {
  bow = std::move(b);
  setPos(pos);
  }

void MdlVisual::setMagicWeapon(PfxObjects::Emitter &&spell) {
  pfx = std::move(spell);
  setPos(pos);
  }

bool MdlVisual::setFightMode(const ZenLoad::EFightMode mode) {
  WeaponState f=WeaponState::NoWeapon;

  switch(mode) {
    case ZenLoad::FM_LAST:
      return false;
    case ZenLoad::FM_NONE:
      f=WeaponState::NoWeapon;
      break;
    case ZenLoad::FM_FIST:
      f=WeaponState::Fist;
      break;
    case ZenLoad::FM_1H:
      f=WeaponState::W1H;
      break;
    case ZenLoad::FM_2H:
      f=WeaponState::W2H;
      break;
    case ZenLoad::FM_BOW:
      f=WeaponState::Bow;
      break;
    case ZenLoad::FM_CBOW:
      f=WeaponState::CBow;
      break;
    case ZenLoad::FM_MAG:
      f=WeaponState::Mage;
      break;
    }

  return setToFightMode(f);
  }

bool MdlVisual::setToFightMode(const WeaponState f) {
  if(f==fightMode)
    return false;
  fightMode = f;
  return true;
  }

void MdlVisual::updateWeaponSkeleton(const Item* weapon,const Item* range) {
  auto st = fightMode;
  if(st==WeaponState::W1H || st==WeaponState::W2H){
    sword.setAttachPoint(skeleton,"ZS_RIGHTHAND");
    } else {
    bool twoHands = weapon!=nullptr && weapon->is2H();
    sword.setAttachPoint(skeleton,twoHands ? "ZS_LONGSWORD" : "ZS_SWORD");
    }

  if(st==WeaponState::Bow || st==WeaponState::CBow){
    if(st==WeaponState::Bow)
      bow.setAttachPoint(skeleton,"ZS_LEFTHAND"); else
      bow.setAttachPoint(skeleton,"ZS_RIGHTHAND");
    } else {
    bool cbow  = range!=nullptr && range->isCrossbow();
    bow.setAttachPoint(skeleton,cbow ? "ZS_CROSSBOW" : "ZS_BOW");
    }
  if(st==WeaponState::Mage)
    pfx.setAttachPoint(skeleton,"ZS_RIGHTHAND");
  pfx.setActive(st==WeaponState::Mage);
  }

void MdlVisual::updateAnimation(Pose& pose) {
  head .setSkeleton(pose,pos);
  sword.setSkeleton(pose,pos);
  bow  .setSkeleton(pose,pos);
  pfx  .setSkeleton(pose,pos);
  view .setSkeleton(pose,pos);
  }

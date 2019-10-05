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
  if(armour.isEmpty()) {
    view  .setObjMatrix(pos);
    } else {
    armour.setObjMatrix(pos);
    view  .setObjMatrix(Matrix4x4());
    }
  }

// mdl_setvisual
void MdlVisual::setVisual(const Skeleton *v) {
  skeleton = v;
  head  .setAttachPoint(skeleton);
  view  .setAttachPoint(skeleton);
  armour.setAttachPoint(skeleton);
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

void MdlVisual::setSpell(PfxObjects::Emitter &&spell) {
  pfx = std::move(spell);
  setPos(pos);
  }

void MdlVisual::updateWeaponSkeleton(WeaponState st,const Item* weapon,const Item* range) {
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
    pfx.setAttachPoint(skeleton,"ZS_RIGHTHAND"); else
    pfx = PfxObjects::Emitter();
  }

void MdlVisual::updateAnimation(Pose& pose) {
  head .setSkeleton(pose,pos);
  sword.setSkeleton(pose,pos);
  bow  .setSkeleton(pose,pos);
  pfx  .setSkeleton(pose,pos);
  if(armour.isEmpty())
    view  .setSkeleton(pose,pos); else
    armour.setSkeleton(pose,pos);
  }

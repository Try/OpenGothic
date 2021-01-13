#include "bullet.h"

#include "graphics/visualfx.h"
#include "world/objects/item.h"
#include "world/objects/npc.h"
#include "world.h"

using namespace Tempest;

Bullet::Bullet(World& owner,const Item& itm,float x,float y,float z)
  :wrld(&owner) {
  obj = wrld->physic()->bulletObj(this);
  if(itm.isSpellOrRune()) {
    obj->setSpellId(itm.spellId());
    }

  if(itm.isSpellOrRune()) {
    material   = ZenLoad::NUM_MAT_GROUPS;
    int32_t id = itm.spellId();
    const VisualFx* vfx = owner.script().getSpellVFx(id);
    if(vfx!=nullptr) {
      auto e = Effect(*vfx,owner,Vec3(x,y,z),SpellFxKey::Cast);
      setView(std::move(e));
      }
    } else {
    material = uint8_t(itm.handle()->material);
    setView(owner.getItmView(itm.handle()->visual,material));
    }

  setPosition(x,y,z);
  }

Bullet::~Bullet() {
  wrld->physic()->deleteObj(obj);
  }

void Bullet::setPosition(const Tempest::Vec3& p) {
  obj->setPosition(p.x,p.y,p.z);
  updateMatrix();
  }

void Bullet::setPosition(float x, float y, float z) {
  obj->setPosition(x,y,z);
  updateMatrix();
  }

void Bullet::setDirection(float x, float y, float z) {
  obj->setDirection(x,y,z);
  updateMatrix();
  }

void Bullet::setView(MeshObjects::Mesh &&m) {
  view = std::move(m);
  updateMatrix();
  }

void Bullet::setView(Effect &&p) {
  vfx = std::move(p);
  vfx.setActive(true);
  vfx.setLooped(true);
  updateMatrix();
  }

bool Bullet::isSpell() const {
  return obj->isSpell();
  }

int32_t Bullet::spellId() const {
  return obj->spellId();
  }

void Bullet::setOwner(Npc *n) {
  ow = n;
  }

Npc *Bullet::owner() const {
  return ow;
  }

bool Bullet::isFinished() const {
  if((flags()&Bullet::Stopped)!=Bullet::Stopped)
    return false;
  return true;
  }

float Bullet::pathLength() const {
  return obj->pathLength();
  }

void Bullet::onStop() {
  addFlags(Stopped);
  updateMatrix();
  }

void Bullet::onMove() {
  updateMatrix();
  }

void Bullet::onCollide(uint8_t matId) {
  if(matId<ZenLoad::NUM_MAT_GROUPS) {
    if(material<ZenLoad::NUM_MAT_GROUPS) {
      auto pos = obj->position();
      wrld->emitLandHitSound(pos.x,pos.y,pos.z,material,matId);
      }
    }
  collideCommon(nullptr);
  }

void Bullet::onCollide(Npc& npc) {
  if(ow!=nullptr)
    npc.takeDamage(*ow,this);
  collideCommon(&npc);
  }

void Bullet::collideCommon(Npc* npc) {
  if(obj->isSpell()) {
    vfx.onCollide(*wrld, obj->position(), npc);
    vfx.setKey   (*wrld, SpellFxKey::Collide);
    wrld->runEffect(std::move(vfx));
    }
  }

void Bullet::updateMatrix() {
  auto mat = obj->matrix();
  view.setObjMatrix(mat);
  vfx .setObjMatrix(mat);
  vfx .setTarget(obj->position()+obj->direction());
  }

#include "bullet.h"

#include "graphics/visualfx.h"
#include "world/item.h"
#include "world/npc.h"
#include "world.h"

Bullet::Bullet(World& owner,const Item& itm,float x,float y,float z)
  :wrld(&owner) {
  obj = wrld->physic()->bulletObj(this);
  if(itm.isSpellOrRune()) {
    obj->setSpellId(itm.spellId());
    }

  if(itm.isSpellOrRune()) {
    material = ZenLoad::NUM_MAT_GROUPS;
    int32_t id = itm.spellId();
    const VisualFx* vfx     = owner.script().getSpellVFx(id);
    auto            emitter = owner.script().getSpellFx (vfx);

    setView(std::move(emitter));
    if(vfx!=nullptr)
      owner.emitSoundEffect(vfx->handle().sfxID.c_str(),x,y,z,0,true);
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

void Bullet::setView(PfxObjects::Emitter &&p) {
  pfx = std::move(p);
  pfx.setActive(true);
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
  collideCommon();
  }

void Bullet::onCollide(Npc& npc) {
  if(ow!=nullptr)
    npc.takeDamage(*ow,this);
  collideCommon();
  }

void Bullet::collideCommon() {
  if(obj->isSpell()) {
    const int32_t     id  = obj->spellId();
    const VisualFx*   vfx = wrld->script().getSpellVFx(id);

    if(vfx!=nullptr) {
      auto pos = obj->position();
      vfx->emitSound(*wrld,pos,SpellFxKey::Collide);
      }
    }
  }

void Bullet::updateMatrix() {
  auto mat = obj->matrix();
  view.setObjMatrix(mat);
  pfx .setObjMatrix(mat);
  }

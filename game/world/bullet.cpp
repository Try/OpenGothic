#include "bullet.h"

#include "graphics/visualfx.h"
#include "world/objects/item.h"
#include "world/objects/npc.h"
#include "world.h"

using namespace Tempest;

Bullet::Bullet(World& owner, const Item& itm, const Vec3& pos)
  :wrld(&owner) {
  obj = wrld->physic()->bulletObj(this);
  if(itm.isSpellOrRune()) {
    obj->setSpellId(itm.spellId());
    }

  if(itm.isSpellOrRune()) {
    material   = ZenLoad::NUM_MAT_GROUPS;
    int32_t id = itm.spellId();
    const VisualFx* vfx = owner.script().spellVfx(id);
    if(vfx!=nullptr) {
      auto e = Effect(*vfx,owner,pos,SpellFxKey::Cast);
      setView(std::move(e));
      }
    } else {
    material = uint8_t(itm.handle().material);
    setView(owner.addView(itm.handle()));
    }

  setPosition(pos);
  }

Bullet::~Bullet() {
  vfx.setBullet(nullptr,*wrld);
  wrld->physic()->deleteObj(obj);
  }

void Bullet::setPosition(const Tempest::Vec3& p) {
  obj->setPosition(p);
  updateMatrix();
  }

void Bullet::setPosition(float x, float y, float z) {
  obj->setPosition(Vec3(x,y,z));
  updateMatrix();
  }

void Bullet::setDirection(const Tempest::Vec3& dir) {
  obj->setDirection(dir);
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

void Bullet::setOrigin(Npc *n) {
  ow = n;
  vfx.setOrigin(n);
  }

Npc *Bullet::origin() const {
  return ow;
  }

void Bullet::setTarget(const Npc* n) {
  vfx.setTarget(n);
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
  flg = Flg(flg|Stopped);
  updateMatrix();
  }

void Bullet::onMove() {
  updateMatrix();
  }

void Bullet::onCollide(uint8_t matId) {
  if(matId<ZenLoad::NUM_MAT_GROUPS) {
    if(material<ZenLoad::NUM_MAT_GROUPS) {
      auto s = wrld->addLandHitEffect(ItemMaterial(material),ZenLoad::MaterialGroup(matId),obj->matrix());
      s.play();
      }
    }
  Effect::onCollide(*wrld,vfx.handle(),obj->position(),nullptr,ow,spellId());
  vfx.setLooped(false);
  wrld->runEffect(std::move(vfx));
  }

void Bullet::onCollide(Npc& npc) {
  if(ow!=nullptr) {
    if(isSpell())
      npc.takeDamage(*ow,this,vfx.handle(),spellId()); else
      npc.takeDamage(*ow,this);
    }
  vfx.setKey(*wrld,SpellFxKey::Collide);
  vfx.setLooped(false);
  wrld->runEffect(std::move(vfx));
  }

void Bullet::updateMatrix() {
  auto mat = obj->matrix();
  view.setObjMatrix(mat);
  // HACK: lighting bolt spell
  mat.rotateOY(90);
  vfx.setObjMatrix(mat);
  // vfx .setTarget(obj->position()+obj->direction());
  }

#include "bullet.h"

#include "world.h"

Bullet::Bullet(World& owner,const Item& itm)
  :wrld(&owner) {
  obj = wrld->physic()->bulletObj(this);
  if(itm.isSpellOrRune()) {
    obj->setSpellId(itm.spellId());
    }

  if(itm.isSpellOrRune()) {
    material = ZenLoad::NUM_MAT_GROUPS;
    int32_t id = itm.spellId();
    const ParticleFx* pfx = owner.script().getSpellFx(id,SpellFxType::Base);
    setView(owner.getView(pfx));
    } else {
    material = uint8_t(itm.handle()->material);
    setView(owner.getStaticView(itm.handle()->visual,material));
    }
  }

Bullet::~Bullet() {
  wrld->physic()->deleteObj(obj);
  }

void Bullet::setPosition(const std::array<float,3> &p) {
  obj->setPosition(p[0],p[1],p[2]);
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
      wrld->emitLandHitSound(pos[0],pos[1],pos[2],material,matId);
      }
    }
  }

void Bullet::onCollide(Npc& npc) {
  if(ow!=nullptr)
    npc.takeDamage(*ow,this);
  /*
  if((flg&Flg::Spell) && (flg&Flg::Stopped)) {
    //int32_t           id  = itm.spellId();
    //const ParticleFx* pfx = wrld.script().getSpellFx(id,SpellFxType::Collide);
    return true;
    }*/
  }

void Bullet::updateMatrix() {
  auto mat = obj->matrix();
  view.setObjMatrix(mat);
  pfx .setObjMatrix(mat);
  }

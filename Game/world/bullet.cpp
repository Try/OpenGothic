#include "bullet.h"

#include "world.h"

Bullet::Bullet(World& owner,const Item& itm)
  :wrld(&owner) {
  if(itm.isSpellOrRune()) {
    material = ZenLoad::NUM_MAT_GROUPS;
    int32_t id = itm.spellId();
    const ParticleFx* pfx      = owner.script().getSpellFx(id,SpellFxType::Base);
    setView(owner.getView(pfx));
    } else {
    material = uint8_t(itm.handle()->material);
    setView(owner.getStaticView(itm.handle()->visual,material));
    }
  }

Bullet::~Bullet() {
  }

void Bullet::setPosition(const std::array<float,3> &p) {
  pos = p;
  updateMatrix();
  }

void Bullet::setPosition(float x, float y, float z) {
  pos = {x,y,z};
  updateMatrix();
  }

void Bullet::setDirection(float x, float y, float z) {
  dir  = {x,y,z};
  dirL = std::sqrt(x*x + y*y + z*z);
  updateMatrix();
  }

void Bullet::setView(MeshObjects::Mesh &&m) {
  view = std::move(m);
  view.setObjMatrix(mat);
  updateMatrix();
  }

void Bullet::setView(PfxObjects::Emitter &&p) {
  pfx = std::move(p);
  pfx.setObjMatrix(mat);
  pfx.setActive(true);
  updateMatrix();
  }

void Bullet::setOwner(Npc *n) {
  ow = n;
  }

Npc *Bullet::owner() const {
  return ow;
  }

bool Bullet::tick(uint64_t dt) {
  auto r = wrld->physic()->moveBullet(*this,dir[0],dir[1],dir[2],dt);
  if(r.mat<ZenLoad::NUM_MAT_GROUPS) {
    if(material<ZenLoad::NUM_MAT_GROUPS)
      wrld->emitLandHitSound(pos[0],pos[1],pos[2],material,r.mat);
    }
  if(r.npc!=nullptr && ow!=nullptr){
    r.npc->takeDamage(*ow,this);
    return true;
    }
  if((flg&Flg::Spell) && (flg&Flg::Stopped)) {
    //int32_t           id  = itm.spellId();
    //const ParticleFx* pfx = wrld.script().getSpellFx(id,SpellFxType::Collide);
    return true;
    }
  return false;
  }

void Bullet::updateMatrix() {
  const float dx = dir[0]/dirL;
  const float dy = dir[1]/dirL;
  const float dz = dir[2]/dirL;

  float a2  = std::asin(dy)*float(180/M_PI);
  float ang = std::atan2(dz,dx)*float(180/M_PI)+180.f;

  mat.identity();
  mat.translate(pos[0],pos[1],pos[2]);
  mat.rotateOY(-ang);
  mat.rotateOZ(-a2);

  view.setObjMatrix(mat);
  pfx.setObjMatrix(mat);
  }

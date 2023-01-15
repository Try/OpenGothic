#include "effect.h"

#include <Tempest/Log>

#include "graphics/mesh/skeleton.h"
#include "world/objects/npc.h"
#include "world/world.h"
#include "visualfx.h"
#include "gothic.h"

using namespace Tempest;

Effect::Effect(PfxEmitter&& pfx, std::string_view node)
  :pfx(std::move(pfx)), nodeSlot(node) {
  pos.identity();
  }

Effect::Effect(const VisualFx& vfx, World& owner, const Npc& src, SpellFxKey key)
  :Effect(vfx, owner, src.position(), key) {
  }

Effect::Effect(const VisualFx& v, World& owner, const Vec3& inPos, SpellFxKey k) {
  root      = &v;
  nodeSlot  = root->emTrjOriginNode;
  pos.identity();
  pos.translate(inPos);
  setKey(owner,k);
  }

Effect::~Effect() {
  pfx.setPhysicsDisable();
  sfx.setLooping(false);
  }

void Effect::setupLight(World& owner) {
  auto* lightPresetName = &root->lightPresetName;
  if(key!=nullptr && !key->lightPresetName.empty())
    lightPresetName = &key->lightPresetName;

  if(lightPresetName->empty()) {
    light = LightGroup::Light();
    return;
    }

  Vec3 pos3 = {pos.at(3,0),pos.at(3,1),pos.at(3,2)};
  light = LightGroup::Light(owner,*lightPresetName);
  light.setPosition(pos3);
  if(key!=nullptr && key->lightRange>0)
    light.setRange(key->lightRange);
  }

void Effect::setupPfx(World& owner) {
  if(root->visName_S=="time.slw"  ||
     root->visName_S=="morph.fov" ||
     root->visName_S=="screenblend.scx" ||
     root->visName_S=="earthquake.eqk") {
    uint64_t emFXLifeSpan = root->emFXLifeSpan;
    if(key!=nullptr && key->emFXLifeSpan!=0)
      emFXLifeSpan = key->emFXLifeSpan;
    gfx = owner.addGlobalEffect(root->visName_S,emFXLifeSpan,root->userString, phoenix::c_fx_base::user_string_count);
    return;
    }

  const ParticleFx* pfxDecl = Gothic::inst().loadParticleFx(root->visName_S);
  if(key!=nullptr && key->visName!=nullptr)
    pfxDecl = key->visName;

  pfxDecl = Gothic::inst().loadParticleFx(pfxDecl,key);
  if(pfxDecl==nullptr)
    return;

  pfx = PfxEmitter(owner,pfxDecl);
  if(root->isMeshEmmiter())
    pfx.setMesh(meshEmitter,pose);
  pfx.setActive(active);
  pfx.setLooped(looped);
  }

void Effect::setupSfx(World& owner) {
  auto sfxID        = root->sfxID;
  auto sfxIsAmbient = root->sfxIsAmbient;
  if(key!=nullptr && !key->sfxID.empty()) {
    sfxID        = key->sfxID;
    sfxIsAmbient = key->sfxIsAmbient;
    }

  if(!sfxID.empty()) {
    Vec3 pos3 = {pos.at(3,0),pos.at(3,1),pos.at(3,2)};
    sfx = ::Sound(owner,::Sound::T_Regular,sfxID,pos3,2500.f,false);
    sfx.setAmbient(sfxIsAmbient);
    sfx.play();
    }
  }

bool Effect::is(const VisualFx& vfx) const {
  return root==&vfx;
  }

void Effect::tick(uint64_t dt) {
  if(next!=nullptr)
    next->tick(dt);

  if(root==nullptr)
    return;

  Vec3 vel = root->emSelfRotVel;
  if(key!=nullptr)
    vel = key->emSelfRotVel.value_or(vel);

  selfRotation += (vel*float(dt)/1000.f);
  if(vel!=Vec3())
    syncAttachesSingle(pos);
  }

void Effect::setActive(bool e) {
  active = e;
  if(next!=nullptr)
    next->setActive(e);
  pfx.setActive(e);
  sfx.setActive(e);
  }

void Effect::setLooped(bool l) {
  if(looped==l)
    return;
  looped = l;
  if(next!=nullptr)
    next->setLooped(l);
  pfx.setLooped(l);
  }

void Effect::setTarget(const Npc* tg) {
  if(next!=nullptr)
    next->setTarget(tg);
  target = tg;
  pfx.setTarget(tg);
  syncAttachesSingle(pos);
  }

void Effect::setOrigin(Npc* npc) {
  if(next!=nullptr)
    next->setOrigin(npc);
  origin = npc;
  syncAttachesSingle(pos);
  setupCollision(npc->world());
  }

void Effect::setObjMatrix(const Tempest::Matrix4x4& mt) {
  syncAttaches(mt);
  }

void Effect::syncAttaches(const Matrix4x4& inPos) {
  if(next!=nullptr)
    next->syncAttaches(inPos);
  syncAttachesSingle(inPos);
  }

void Effect::syncAttachesSingle(const Matrix4x4& inPos) {
  pos = inPos;

  auto  emTrjMode    = VisualFx::TrajectoryNone;
  float emTrjEaseVel = 0;
  Vec3  emSelfRotVel;

  if(root!=nullptr) {
    emTrjMode    = root->emTrjMode;
    emSelfRotVel = root->emSelfRotVel;
    emTrjEaseVel = root->emTrjTargetElev;
    if(key!=nullptr) {
      if(key->emTrjMode.has_value())
        emTrjMode    = key->emTrjMode.value();
      if(key->emSelfRotVel.has_value())
        emSelfRotVel = key->emSelfRotVel.value();
      }
    }

  auto p = inPos;
  if((emTrjMode&VisualFx::Trajectory::Target) && target!=nullptr) {
    p = target->transform();
    } else {
    if(pose!=nullptr && boneId<pose->boneCount())
      p = pose->bone(boneId); else
      p = inPos;
    }

  if(selfRotation!=Vec3() && false) {
    // FIXME
    Matrix4x4 m;
    m.rotateOX(selfRotation.x);
    m.rotateOY(selfRotation.y);
    m.rotateOZ(selfRotation.z);
    p.mul(m);
    }

  p.set(3,1, p.at(3,1)+emTrjEaseVel);
  Vec3  pos3 = {p.at(3,0),p.at(3,1),p.at(3,2)};
  pfx  .setObjMatrix(p);
  light.setPosition(pos3);
  sfx  .setPosition(pos3);
  }

void Effect::setKey(World& owner, SpellFxKey k, int32_t keyLvl) {
  if(next!=nullptr)
    next->setKey(owner,k);

  if(root==nullptr)
    return;

  key = root->key(k,keyLvl);

  const VisualFx* vfx = root->emFXCreate;
  if(key!=nullptr && key->emCreateFXID!=nullptr)
    vfx  = key->emCreateFXID;

  if(vfx!=nullptr && !(next!=nullptr && next->is(*vfx))) {
    Vec3 pos3 = {pos.at(3,0),pos.at(3,1),pos.at(3,2)};
    auto ex   = Effect(*vfx,owner,pos3,k);
    ex.setActive(active);
    ex.setLooped(looped);
    if(pose!=nullptr && skeleton!=nullptr)
      ex.bindAttaches(*pose,*skeleton);
    next.reset(new Effect(std::move(ex)));
    }
  else if(vfx==nullptr) {
    next.reset(nullptr);
    }

  setupPfx      (owner);
  setupLight    (owner);
  setupSfx      (owner);
  setupCollision(owner);
  syncAttachesSingle(pos);
  }

void Effect::setMesh(const MeshObjects::Mesh* mesh) {
  if(next!=nullptr)
    next->setMesh(mesh);
  meshEmitter = mesh;
  if(root!=nullptr && root->isMeshEmmiter())
    pfx.setMesh(meshEmitter,pose);
  }

void Effect::setBullet(Bullet* b, World& owner) {
  if(next!=nullptr)
    next->setBullet(b,owner);
  bullet = b;
  setupCollision(owner);
  }

void Effect::setSpellId(int32_t s, World& owner) {
  if(next!=nullptr)
    next->setSpellId(s,owner);
  splId = s;
  setupCollision(owner);
  }

uint64_t Effect::effectPrefferedTime() const {
  uint64_t ret = next==nullptr ? 0 : next->effectPrefferedTime();

  ret = std::max(ret, root==nullptr ? 0 : root->effectPrefferedTime());
  ret = std::max(ret, pfx  .effectPrefferedTime());
  // ret = std::max(ret, sfx  .effectPrefferedTime());
  // ret = std::max(ret, gfx  .effectPrefferedTime());
  // ret = std::max(ret, light.effectPrefferedTime());
  return ret;
  }

bool Effect::isAlive() const {
  if(pfx.isAlive())
    return true;
  return (next!=nullptr && next->isAlive());
  }

void Effect::setPhysicsDisable() {
  noPhysics = true;
  pfx.setPhysicsDisable();
  }

void Effect::bindAttaches(const Pose& p, const Skeleton& to) {
  if(next!=nullptr)
    next->bindAttaches(p,to);

  skeleton = &to;
  pose     = &p;
  boneId   = to.findNode(nodeSlot);
  if(root!=nullptr && root->isMeshEmmiter())
    pfx.setMesh(meshEmitter,pose);
  }

void Effect::onCollide(World& world, const VisualFx* root, const Vec3& pos, Npc* npc, Npc* other, int32_t splId) {
  if(root==nullptr)
    return;

  const VisualFx* vfx = root->emFXCollStat;
  if(npc!=nullptr)
    vfx = root->emFXCollDyn;

  if(vfx!=nullptr) {
    Effect eff(*vfx,world,pos,SpellFxKey::Collide);
    eff.setSpellId(splId,world);
    eff.setOrigin(other);
    eff.setActive(true);

    if(npc!=nullptr)
      npc ->runEffect(std::move(eff)); else
      world.runEffect(std::move(eff));
    }

  if(npc!=nullptr && root->emFXCollDynPerc!=nullptr) {
    const VisualFx* vfx = root->emFXCollDynPerc;
    Effect eff(*vfx,world,pos,SpellFxKey::Collide);
    eff.setActive(true);
    npc->runEffect(std::move(eff));
    if(vfx->sendAssessMagic) {
      auto oth = other==nullptr ? npc : other;
      npc->perceptionProcess(*oth,npc,0,PERC_ASSESSMAGIC);
      }
    }
  }

void Effect::setupCollision(World& owner) {
  if(root==nullptr) {
    pfx.setPhysicsDisable();
    return;
    }

  bool emCheckCollision = root->emCheckCollision;
  if(key!=nullptr)
    emCheckCollision = key->emCheckCollision;
  (void)emCheckCollision;

  bool physics = false;
  if(root->emFXCollDyn!=nullptr || root->emFXCollDynPerc!=nullptr)
    physics = true;
  if(emCheckCollision)
    physics = true;
  if(noPhysics)
    physics = false;

  if(!pfx.isEmpty() && physics) {
    auto vfx = root;
    auto n   = origin;
    auto sId = splId;
    auto b   = bullet;
    pfx.setPhysicsEnable(owner,[vfx,n,sId,b](Npc& npc){
      if(n!=&npc) {
        auto src = (n!=nullptr ? n : &npc);
        npc.takeDamage(*src,b,vfx,sId);
        if(b!=nullptr)
          b->setFlags(Bullet::Stopped);
        }
      });
    }
  }

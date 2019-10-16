#include "mdlvisual.h"

#include "graphics/skeleton.h"
#include "game/serialize.h"
#include "world/npc.h"
#include "world/item.h"
#include "world/world.h"

using namespace Tempest;

MdlVisual::MdlVisual()
  :skInst(std::make_unique<Pose>()) {
  }

void MdlVisual::save(Serialize &fout) {
  fout.write(fightMode);
  if(skeleton!=nullptr)
    fout.write(skeleton->name()); else
    fout.write(std::string(""));
  solver.save(fout);
  skInst->save(fout);
  }

void MdlVisual::load(Serialize &fin,Npc& npc) {
  std::string s;

  fin.read(fightMode);
  fin.read(s);
  npc.setVisual(s.c_str());
  solver.load(fin);
  skInst->load(fin,solver);
  }

void MdlVisual::setPos(float x, float y, float z) {
  pos.set(3,0,x);
  pos.set(3,1,y);
  pos.set(3,2,z);
  setPos(pos);
  }

void MdlVisual::setPos(const Tempest::Matrix4x4 &m) {
  // TODO: deferred setObjMatrix
  pos = m;
  head .setObjMatrix(pos);
  sword.setObjMatrix(pos);
  bow  .setObjMatrix(pos);
  pfx  .setObjMatrix(pos);
  view .setObjMatrix(pos);
  }

// mdl_setvisual
void MdlVisual::setVisual(const Skeleton *v) {
  skeleton = v;
  solver.setSkeleton(skeleton);
  skInst->setSkeleton(v);
  head  .setAttachPoint(skeleton);
  view  .setAttachPoint(skeleton);
  setPos(pos); // update obj matrix
  }

// Mdl_SetVisualBody
void MdlVisual::setVisualBody(MeshObjects::Mesh &&h, MeshObjects::Mesh &&body) {
  head    = std::move(h);
  view    = std::move(body);

  head.setAttachPoint(skeleton,"BIP01 HEAD");
  view.setAttachPoint(skeleton);
  }

// Mdl_ApplyOverlayMdsTimed, Mdl_ApplyOverlayMds
void MdlVisual::addOverlay(const Skeleton *sk, uint64_t time) {
  solver.addOverlay(sk,time);
  }

// Mdl_RemoveOverlayMDS
void MdlVisual::delOverlay(const char *sk) {
  solver.delOverlay(sk);
  }

// Mdl_RemoveOverlayMDS
void MdlVisual::delOverlay(const Skeleton *sk) {
  solver.delOverlay(sk);
  }

void MdlVisual::setArmour(MeshObjects::Mesh &&a) {
  view = std::move(a);
  view.setAttachPoint(skeleton);
  setPos(pos);
  }

void MdlVisual::setSword(MeshObjects::Mesh &&s) {
  sword = std::move(s);
  setPos(pos);
  }

void MdlVisual::setRangeWeapon(MeshObjects::Mesh &&b) {
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

void MdlVisual::updateAnimation(Npc& npc) {
  Pose&    pose      = *skInst;
  uint64_t tickCount = npc.world().tickCount();

  if(npc.world().isInListenerRange(npc.position()))
    pose.emitSfx(npc,tickCount);

  solver.update(tickCount);
  pose.update(tickCount);

  head .setSkeleton(pose,pos);
  sword.setSkeleton(pose,pos);
  bow  .setSkeleton(pose,pos);
  pfx  .setSkeleton(pose,pos);
  view .setSkeleton(pose,pos);
  }

void MdlVisual::stopAnim(Npc& npc,const char* ani) {
  skInst->stopAnim(ani);
  if(!skInst->hasAnim())
    setAnim(npc,AnimationSolver::Idle,fightMode);
  }

bool MdlVisual::isInAnim(AnimationSolver::Anim a) const {
  return false; //TODO
  }

bool MdlVisual::isStanding() const {
  return skInst->isStanding();
  }

bool MdlVisual::setAnim(Npc& npc, AnimationSolver::Anim a, WeaponState st) {
  if(a==AnimationSolver::Interact || a==AnimationSolver::InteractOut) {
    auto inter = npc.interactive();
    const Animation::Sequence *sq = solver.solveAnim(inter,a,*skInst);
    if(sq!=nullptr && inter!=nullptr){
      if(skInst->startAnim(solver,sq,false,npc.world().tickCount())) {
        if(a==AnimationSolver::Anim::Interact)
          inter->nextState(); else
          inter->prevState();
        return true;
        }
      }
    return false;
    }

  bool forceAnim=false;
  if(a==AnimationSolver::Anim::DeadA || a==AnimationSolver::Anim::UnconsciousA ||
     a==AnimationSolver::Anim::DeadB || a==AnimationSolver::Anim::UnconsciousB)
    forceAnim=true;

  const Animation::Sequence *sq = solver.solveAnim(a,st,npc.walkMode(),*skInst);
  if(skInst->startAnim(solver,sq,forceAnim,npc.world().tickCount())) {
    return true;
    }
  return false;
  }

bool MdlVisual::setAnim(Npc &npc, WeaponState st) {
  const Animation::Sequence *sq = solver.solveAnim(st,fightMode,*skInst);
  if(sq==nullptr)
    return true;
  if(skInst->startAnim(solver,sq,false,npc.world().tickCount())) {
    return true;
    }
  return false;
  }

void MdlVisual::setRotation(Npc &npc, int dir) {
  const Animation::Sequence *sq = nullptr;
  if(dir==0) {
    if(rotation!=nullptr) {
      if(skInst->stopAnim(rotation->name.c_str()))
        rotation = nullptr;
      }
    return;
    }
  if(!skInst->isIdle() || rotation!=nullptr)
    return;
  if(dir<0) {
    sq = solver.solveAnim(AnimationSolver::Anim::RotL,fightMode,npc.walkMode(),*skInst);
    } else {
    sq = solver.solveAnim(AnimationSolver::Anim::RotR,fightMode,npc.walkMode(),*skInst);
    }
  if(sq==nullptr)
    return;
  if(skInst->startAnim(solver,sq,false,npc.world().tickCount())) {
    rotation = sq;
    return;
    }
  }

AnimationSolver::Anim MdlVisual::animByName(const std::string &name) const {
  return solver.animByName(name);
  }

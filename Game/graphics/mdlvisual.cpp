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
  fout.write(fgtMode);
  if(skeleton!=nullptr)
    fout.write(skeleton->name()); else
    fout.write(std::string(""));
  solver.save(fout);
  skInst->save(fout);
  }

void MdlVisual::load(Serialize &fin,Npc& npc) {
  std::string s;

  fin.read(fgtMode);
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
  head   .setObjMatrix(pos);
  sword  .setObjMatrix(pos);
  bow    .setObjMatrix(pos);
  for(auto& i:item)
    i.setObjMatrix(pos);
  pfx    .setObjMatrix(pos);
  view   .setObjMatrix(pos);
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

void MdlVisual::setSlotItem(MeshObjects::Mesh &&itm, const char *bone) {
  if(bone==nullptr)
    return;

  for(auto& i:item) {
    const char* b = i.attachPoint();
    if(b!=nullptr && std::strcmp(b,bone)==0) {
      i = std::move(itm);
      i.setAttachPoint(skeleton,bone);
      break;
      }
    }
  itm.setAttachPoint(skeleton,bone);
  item.emplace_back(std::move(itm));
  setPos(pos);
  }

void MdlVisual::clearSlotItem(const char *bone) {
  for(size_t i=0;i<item.size();++i) {
    const char* b = item[i].attachPoint();
    if(bone==nullptr || (b!=nullptr && std::strcmp(b,bone)==0)) {
      item[i] = std::move(item.back());
      item.pop_back();
      }
    }
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
  if(f==fgtMode)
    return false;
  fgtMode = f;
  return true;
  }

void MdlVisual::updateWeaponSkeleton(const Item* weapon,const Item* range) {
  auto st = fgtMode;
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
    pose.processSfx(npc,tickCount);

  solver.update(tickCount);
  pose.update(solver,tickCount);

  head   .setSkeleton(pose,pos);
  sword  .setSkeleton(pose,pos);
  bow    .setSkeleton(pose,pos);
  for(auto& i:item)
    i.setSkeleton(pose,pos);
  pfx    .setSkeleton(pose,pos);
  view   .setSkeleton(pose,pos);
  }

void MdlVisual::stopAnim(Npc& npc,const char* ani) {
  skInst->stopAnim(ani);
  if(!skInst->hasAnim())
    startAnim(npc,AnimationSolver::Idle,fgtMode,npc.walkMode());
  }

void MdlVisual::stopWalkAnim(Npc &npc) {
  skInst->stopAnim(nullptr);
  startAnim(npc,AnimationSolver::Idle,fgtMode,npc.walkMode());
  }

bool MdlVisual::isRunTo(const Npc& npc) const {
  const Animation::Sequence *sq = solver.solveAnim(AnimationSolver::Anim::Move,fgtMode,npc.walkMode(),*skInst);
  return skInst->isInAnim(sq);
  }

bool MdlVisual::isStanding() const {
  return skInst->isStanding();
  }

bool MdlVisual::isItem() const {
  return skInst->isItem();
  }

const Animation::Sequence* MdlVisual::startAnimAndGet(Npc &npc, const char *name, BodyState bs) {
  bool forceAnim=true;

  const Animation::Sequence *sq = solver.solveFrm(name);
  if(skInst->startAnim(solver,sq,bs,forceAnim,npc.world().tickCount())) {
    return sq;
    }
  return nullptr;
  }

bool MdlVisual::startAnim(Npc &npc, const char *name, BodyState bs) {
  return startAnimAndGet(npc,name,bs)!=nullptr;
  }

bool MdlVisual::startAnim(Npc& npc, AnimationSolver::Anim a, WeaponState st, WalkBit wlk) {
  // for those use MdlVisual::setRotation
  assert(a!=AnimationSolver::Anim::RotL && a!=AnimationSolver::Anim::RotR);

  if(a==AnimationSolver::InteractIn || a==AnimationSolver::InteractOut) {
    auto inter = npc.interactive();
    const Animation::Sequence *sq = solver.solveAnim(inter,a,*skInst);
    if(sq!=nullptr){
      if(skInst->startAnim(solver,sq,BS_MOBINTERACT,false,npc.world().tickCount())) {
        return true;
        }
      }
    return false;
    }

  const Animation::Sequence *sq = solver.solveAnim(a,st,wlk,*skInst);

  bool forceAnim=false;
  if(a==AnimationSolver::Anim::DeadA || a==AnimationSolver::Anim::UnconsciousA ||
     a==AnimationSolver::Anim::DeadB || a==AnimationSolver::Anim::UnconsciousB) {
    if(sq!=nullptr)
      skInst->stopAllAnim();
    forceAnim=true;
    }

  BodyState bs = BS_NONE;
  switch(a) {
    case AnimationSolver::Anim::NoAnim:
      bs = BS_NONE;
      break;
    case AnimationSolver::Anim::Idle:
    case AnimationSolver::Anim::MagNoMana:
      bs = BS_STAND;
      break;
    case AnimationSolver::Anim::DeadA:
    case AnimationSolver::Anim::DeadB:
      bs = BS_DEAD;
      break;
    case AnimationSolver::Anim::UnconsciousA:
    case AnimationSolver::Anim::UnconsciousB:
      bs = BS_UNCONSCIOUS;
      break;
    case AnimationSolver::Anim::Move:
    case AnimationSolver::Anim::MoveL:
    case AnimationSolver::Anim::MoveR:
    case AnimationSolver::Anim::MoveBack:
      if(bool(wlk & WalkBit::WM_Walk))
        bs = BS_WALK; else
        bs = BS_RUN;
      break;
    case AnimationSolver::Anim::RotL:
    case AnimationSolver::Anim::RotR:
      break;
    case AnimationSolver::Anim::Jump:
    case AnimationSolver::Anim::JumpUpLow:
    case AnimationSolver::Anim::JumpUpMid:
    case AnimationSolver::Anim::JumpUp:
      bs = BS_JUMP;
      break;
    case AnimationSolver::Anim::Fall:
    case AnimationSolver::Anim::FallDeep:
      bs = BS_FALL;
      break;
    case AnimationSolver::Anim::SlideA:
    case AnimationSolver::Anim::SlideB:
      bs = BS_NONE;
      break;
    case AnimationSolver::Anim::InteractIn:
    case AnimationSolver::Anim::InteractOut:
      bs = BS_MOBINTERACT;
      break;
    case AnimationSolver::Anim::Atack:
    case AnimationSolver::Anim::AtackL:
    case AnimationSolver::Anim::AtackR:
    case AnimationSolver::Anim::AtackFinish:
      bs = BS_NONE;
      break;
    case AnimationSolver::Anim::AtackBlock:
      bs = BS_PARADE;
      break;
    case AnimationSolver::Anim::StumbleA:
    case AnimationSolver::Anim::StumbleB:
      bs = BS_PARADE;
      break;
    case AnimationSolver::Anim::AimBow:
      bs = BS_AIMNEAR; //TODO: BS_AIMFAR
      break;
    }

  if(bool(wlk & WalkBit::WM_Swim))
    bs = BS_SWIM;

  if(skInst->startAnim(solver,sq,bs,forceAnim,npc.world().tickCount())) {
    return true;
    }
  return false;
  }

bool MdlVisual::startAnim(Npc &npc, WeaponState st) {
  const bool run = (skInst->bodyState()&BS_MAX)==BS_RUN;

  const Animation::Sequence *sq = solver.solveAnim(st,fgtMode,run);
  if(sq==nullptr)
    return true;
  if(skInst->startAnim(solver,sq,run ? BS_RUN : BS_NONE,false,npc.world().tickCount())) {
    return true;
    }
  return false;
  }

void MdlVisual::setRotation(Npc &npc, int dir) {
  skInst->setRotation(solver,npc,fgtMode,dir);
  }

std::array<float,3> MdlVisual::displayPosition() const {
  if(skeleton!=nullptr)
    return {0,skeleton->colisionHeight()*1.5f,0};
  return {0.f,0.f,0.f};
  }

bool MdlVisual::startAnimItem(Npc &npc, const char *scheme) {
  return skInst->setAnimItem(solver,npc,scheme);
  }

bool MdlVisual::startAnimSpell(Npc &npc, const char *scheme) {
  char name[128]={};
  std::snprintf(name,sizeof(name),"S_%sSHOOT",scheme);

  const Animation::Sequence *sq = solver.solveFrm(name);
  if(skInst->startAnim(solver,sq,BS_CASTING,true,npc.world().tickCount())) {
    return true;
    }
  return false;
  }

bool MdlVisual::startAnimDialog(Npc &npc) {
  //const int countG1 = 21;
  const int countG2 = 11;
  const int id      = std::rand()%countG2 + 1;

  char name[32]={};
  std::snprintf(name,sizeof(name),"T_DIALOGGESTURE_%02d",id);

  const Animation::Sequence *sq = solver.solveFrm(name);
  if(skInst->startAnim(solver,sq,BS_NONE,false,npc.world().tickCount())) {
    return true;
    }
  return false;
  }

void MdlVisual::stopDlgAnim() {
  //const int countG1 = 21;
  const int countG2 = 11;
  for(uint16_t i=0; i<countG2; i++){
    char buf[32]={};
    std::snprintf(buf,sizeof(buf),"T_DIALOGGESTURE_%02d",i+1);
    skInst->stopAnim(buf);
    }
  }

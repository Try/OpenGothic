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

// Mdl_SetVisual
void MdlVisual::setVisual(const Skeleton *v) {
  if(skeleton!=nullptr && v!=nullptr)
    rebindAttaches(*skeleton,*v);
  solver.setSkeleton(v);
  skInst->setSkeleton(v);
  view.setSkeleton(v);

  skeleton = v;
  setPos(pos); // update obj matrix
  }

// Mdl_SetVisualBody
void MdlVisual::setVisualBody(MeshObjects::Mesh &&h, MeshObjects::Mesh &&body, World& owner) {
  bind(head,std::move(h),"BIP01 HEAD");

  if(auto p = body.protoMesh()) {
    for(auto& att:p->attach) {
      auto view = owner.getAtachView(att);
      setSlotItem(std::move(view),att.name.c_str());
      }
    }
  view = std::move(body);
  view.setSkeleton(skeleton);
  }

bool MdlVisual::hasOverlay(const Skeleton* sk) const {
  return solver.hasOverlay(sk);
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
  view.setSkeleton(skeleton);
  setPos(pos);
  }

void MdlVisual::setSword(MeshObjects::Mesh &&s) {
  bind(sword,std::move(s),"ZS_RIGHTHAND");
  }

void MdlVisual::setRangeWeapon(MeshObjects::Mesh &&b) {
  bind(bow,std::move(b),"ZS_BOW");
  }

void MdlVisual::setAmmoItem(MeshObjects::Mesh&& a, const char *bone) {
  bind(ammunition,std::move(a),bone);
  }

void MdlVisual::setMagicWeapon(PfxObjects::Emitter &&spell) {
  bind(pfx,std::move(spell),"ZS_RIGHTHAND");
  }

void MdlVisual::setSlotItem(MeshObjects::Mesh &&itm, const char *bone) {
  if(bone==nullptr || skeleton==nullptr)
    return;

  size_t id = skeleton->findNode(bone);
  if(id==size_t(-1))
    return;

  for(auto& i:item) {
    if(i.boneId==id) {
      i.view=std::move(itm);
      syncAttaches();
      return;
      }
    }

  MeshAttach slt;
  slt.bone = skeleton->nodes[id].name.c_str();
  bind(slt,std::move(itm),slt.bone);
  item.push_back(std::move(slt));
  syncAttaches();
  }

void MdlVisual::setStateItem(MeshObjects::Mesh&& a, const char* bone) {
  bind(stateItm,std::move(a),bone);
  syncAttaches();
  }

void MdlVisual::clearSlotItem(const char *bone) {
  size_t id = skeleton==nullptr ? size_t(-1) : skeleton->findNode(bone);

  for(size_t i=0;i<item.size();++i) {
    if(id==size_t(-1) || item[i].boneId==id) {
      item[i] = std::move(item.back());
      item.pop_back();
      syncAttaches();
      return;
      }
    }
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

void MdlVisual::startParticleEffect(PfxObjects::Emitter&& pfx, int32_t slot, const char* bone, uint64_t timeUntil) {
  if(bone==nullptr || skeleton==nullptr)
    return;

  size_t id = skeleton->findNode(bone);
  if(id==size_t(-1))
    return;

  for(auto& i:effects) {
    if(i.id==slot) {
      i.bone      = skeleton->nodes[id].name.c_str();
      i.timeUntil = timeUntil;
      bind(i,std::move(pfx),i.bone);
      syncAttaches();
      return;
      }
    }

  PfxSlot slt;
  slt.bone      = skeleton->nodes[id].name.c_str();
  slt.id        = slot;
  slt.timeUntil = timeUntil;
  bind(slt,std::move(pfx),slt.bone);
  effects.push_back(std::move(slt));
  syncAttaches();
  }

void MdlVisual::stopParticleEffect(int32_t slot) {
  for(size_t i=0;i<effects.size();++i) {
    if(effects[i].id==slot) {
      effects[i] = std::move(effects.back());
      effects.pop_back();
      syncAttaches();
      return;
      }
    }
  }

bool MdlVisual::setToFightMode(const WeaponState f) {
  if(f==fgtMode)
    return false;
  fgtMode = f;
  return true;
  }

void MdlVisual::setPos(float x, float y, float z) {
  pos.set(3,0,x);
  pos.set(3,1,y);
  pos.set(3,2,z);
  setPos(pos);
  }

void MdlVisual::setPos(const Tempest::Matrix4x4 &m) {
  pos = m;
  view.setObjMatrix(pos);
  syncAttaches();
  }

void MdlVisual::setTarget(const Tempest::Vec3& p) {
  targetPos = p;
  }

void MdlVisual::updateWeaponSkeleton(const Item* weapon,const Item* range) {
  auto st = fgtMode;
  if(st==WeaponState::W1H || st==WeaponState::W2H){
    bind(sword,"ZS_RIGHTHAND");
    } else {
    bool twoHands = weapon!=nullptr && weapon->is2H();
    bind(sword,twoHands ? "ZS_LONGSWORD" : "ZS_SWORD");
    }

  if(st==WeaponState::Bow || st==WeaponState::CBow){
    if(st==WeaponState::Bow)
      bind(bow,"ZS_LEFTHAND"); else
      bind(bow,"ZS_RIGHTHAND");
    } else {
    bool cbow  = range!=nullptr && range->isCrossbow();
    bind(bow,cbow ? "ZS_CROSSBOW" : "ZS_BOW");
    }
  if(st==WeaponState::Mage)
    bind(pfx,"ZS_RIGHTHAND");
  pfx.view.setActive(st==WeaponState::Mage);
  syncAttaches();
  }

void MdlVisual::updateAnimation(Npc& npc,int comb) {
  Pose&    pose      = *skInst;
  uint64_t tickCount = npc.world().tickCount();

  if(npc.world().isInListenerRange(npc.position()))
    pose.processSfx(npc,tickCount);

  pose.processPfx(npc,tickCount);

  for(size_t i=0;i<effects.size();) {
    if(effects[i].timeUntil<tickCount) {
      effects[i] = std::move(effects.back());
      effects.pop_back();
      } else {
      ++i;
      }
    }

  solver.update(tickCount);
  const bool changed = pose.update(solver,comb,tickCount);
  if(!changed)
    return;
  syncAttaches();

  view.setPose(pose,pos);
  }

Vec3 MdlVisual::mapBone(const size_t boneId) const {
  Pose&  pose = *skInst;
  if(boneId==size_t(-1))
    return {0,0,0};

  auto mat = pos;
  mat.mul(pose.bone(boneId));

  return {mat.at(3,0) - pos.at(3,0),
          mat.at(3,1) - pos.at(3,1),
          mat.at(3,2) - pos.at(3,2)};
  }

Vec3 MdlVisual::mapWeaponBone() const {
  if(fgtMode==WeaponState::Bow || fgtMode==WeaponState::CBow)
    return mapBone(ammunition.boneId);
  if(fgtMode==WeaponState::Mage && skeleton!=nullptr)
    return mapBone(skeleton->findNode("ZS_RIGHTHAND"));
  return {0,0,0};
  }

void MdlVisual::stopAnim(Npc& npc,const char* ani) {
  skInst->stopAnim(ani);
  if(!skInst->hasAnim())
    startAnimAndGet(npc,AnimationSolver::Idle,fgtMode,npc.walkMode(),false);
  }

void MdlVisual::stopItemStateAnim(Npc& npc) {
  skInst->stopItemStateAnim();
  if(!skInst->hasAnim())
    startAnimAndGet(npc,AnimationSolver::Idle,fgtMode,npc.walkMode(),false);
  }

void MdlVisual::stopWalkAnim(Npc &npc) {
  auto state = pose().bodyState();
  if(state!=BS_STAND && state!=BS_MOBINTERACT) {
    skInst->stopAnim(nullptr);
    startAnimAndGet(npc,AnimationSolver::Idle,fgtMode,npc.walkMode(),false);
    }
  }

bool MdlVisual::isStanding() const {
  return skInst->isStanding();
  }

bool MdlVisual::isItem() const {
  return skInst->isItem();
  }

bool MdlVisual::isAnimExist(const char* name) const {
  const Animation::Sequence *sq = solver.solveFrm(name);
  return sq!=nullptr;
  }

const Animation::Sequence* MdlVisual::startAnimAndGet(Npc &npc, const char *name,
                                                      bool forceAnim, BodyState bs) {
  const Animation::Sequence *sq = solver.solveFrm(name);
  Pose::StartHint hint = Pose::StartHint(forceAnim  ? Pose::Force : Pose::NoHint);
  if(skInst->startAnim(solver,sq,bs,hint,npc.world().tickCount())) {
    return sq;
    }
  return nullptr;
  }

const Animation::Sequence* MdlVisual::startAnimAndGet(Npc& npc, AnimationSolver::Anim a,
                                                      WeaponState st, WalkBit wlk,
                                                      bool noInterupt) {
  // for those use MdlVisual::setRotation
  assert(a!=AnimationSolver::Anim::RotL && a!=AnimationSolver::Anim::RotR);

  if(a==AnimationSolver::InteractIn ||
     a==AnimationSolver::InteractOut ||
     a==AnimationSolver::InteractToStand ||
     a==AnimationSolver::InteractFromStand) {
    auto inter = npc.interactive();
    const Animation::Sequence *sq = solver.solveAnim(inter,a,*skInst);
    if(sq!=nullptr){
      if(skInst->startAnim(solver,sq,BS_MOBINTERACT,Pose::NoHint,npc.world().tickCount())) {
        return sq;
        }
      }
    return nullptr;
    }

  const Animation::Sequence *sq = solver.solveAnim(a,st,wlk,*skInst);

  bool forceAnim=false;
  if(a==AnimationSolver::Anim::DeadA || a==AnimationSolver::Anim::UnconsciousA ||
     a==AnimationSolver::Anim::DeadB || a==AnimationSolver::Anim::UnconsciousB) {
    if(sq!=nullptr)
      skInst->stopAllAnim();
    forceAnim = true;
    }
  if(!noInterupt) {
    if(a==AnimationSolver::Anim::StumbleA || a==AnimationSolver::Anim::StumbleB ||
       a==AnimationSolver::Anim::JumpHang)
      forceAnim = true;
    }

  BodyState bs = BS_NONE;
  switch(a) {
    case AnimationSolver::Anim::NoAnim:
    case AnimationSolver::Anim::Fallen:
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
    case AnimationSolver::Anim::JumpUp:
      bs = BS_JUMP;
      break;
    case AnimationSolver::Anim::JumpUpLow:
    case AnimationSolver::Anim::JumpUpMid:
    case AnimationSolver::Anim::JumpHang:
      bs = BS_CLIMB;
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
    case AnimationSolver::Anim::InteractToStand:
    case AnimationSolver::Anim::InteractFromStand:
      bs = BS_MOBINTERACT;
      break;
    case AnimationSolver::Anim::Atack:
    case AnimationSolver::Anim::AtackL:
    case AnimationSolver::Anim::AtackR:
    case AnimationSolver::Anim::AtackFinish:
      bs = pose().bodyState()==BS_RUN ? BS_RUN : BS_NONE;
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
  if(bool(wlk & WalkBit::WM_Sneak))
    bs = BS_SNEAK;

  Pose::StartHint hint = Pose::StartHint((forceAnim  ? Pose::Force : Pose::NoHint) |
                                         (noInterupt ? Pose::NoInterupt : Pose::NoHint));

  if(skInst->startAnim(solver,sq,bs,hint,npc.world().tickCount())) {
    return sq;
    }
  return nullptr;
  }

bool MdlVisual::startAnim(Npc &npc, WeaponState st) {
  const bool run = (skInst->bodyState()&BS_MAX)==BS_RUN;

  if(st==fgtMode)
    return true;
  const Animation::Sequence *sq = solver.solveAnim(st,fgtMode,run);
  if(sq==nullptr)
    return false;
  if(skInst->startAnim(solver,sq,run ? BS_RUN : BS_NONE,Pose::NoHint,npc.world().tickCount()))
    return true;
  return false;
  }

void MdlVisual::setRotation(Npc &npc, int dir) {
  skInst->setRotation(solver,npc,fgtMode,dir);
  }

void MdlVisual::interrupt() {
  skInst->interrupt();
  }

Tempest::Vec3 MdlVisual::displayPosition() const {
  if(skeleton!=nullptr)
    return {0,skeleton->colisionHeight()*1.5f,0};
  return {0.f,0.f,0.f};
  }

const Animation::Sequence* MdlVisual::continueCombo(Npc& npc, AnimationSolver::Anim a,
                                                    WeaponState st, WalkBit wlk)  {
  if(st==WeaponState::Fist || st==WeaponState::W1H || st==WeaponState::W2H) {
    const Animation::Sequence *sq = solver.solveAnim(a,st,wlk,*skInst);
    if(auto ret = skInst->continueCombo(solver,sq,npc.world().tickCount()))
      return ret;
    }
  return startAnimAndGet(npc,a,st,wlk,false);
  }

uint32_t MdlVisual::comboLength() const {
  return skInst->comboLength();
  }

void MdlVisual::bind(MeshAttach& slot, MeshObjects::Mesh&& itm, const char* bone) {
  slot.boneId = skeleton==nullptr ? size_t(-1) : skeleton->findNode(bone);
  slot.view   = std::move(itm);
  slot.bone   = bone;
  // sync?
  }

void MdlVisual::bind(MdlVisual::PfxAttach& slot, PfxObjects::Emitter&& itm, const char* bone) {
  slot.boneId = skeleton==nullptr ? size_t(-1) : skeleton->findNode(bone);
  slot.view   = std::move(itm);
  slot.bone   = bone;
  // sync?
  }

template<class View>
void MdlVisual::bind(Attach<View>& slot, const char* bone) {
  slot.boneId = skeleton==nullptr ? size_t(-1) : skeleton->findNode(bone);
  slot.bone   = bone;
  // sync?
  }

void MdlVisual::rebindAttaches(const Skeleton& from, const Skeleton& to) {
  auto  mesh = {&head,&sword,&bow,&ammunition,&stateItm};
  for(auto i:mesh)
    rebindAttaches(*i,from,to);
  for(auto& i:item)
    rebindAttaches(i,from,to);
  rebindAttaches(pfx,from,to);
  }

template<class View>
void MdlVisual::rebindAttaches(Attach<View>& mesh, const Skeleton& from, const Skeleton& to) {
  if(mesh.boneId<from.nodes.size()) {
    size_t nid = 0;
    if(mesh.bone==nullptr)
      nid = to.findNode(from.nodes[mesh.boneId].name); else
      nid = to.findNode(mesh.bone);
    mesh.boneId = nid;
    }
  }

void MdlVisual::syncAttaches() {
  MdlVisual::MeshAttach* mesh[] = {&head,&sword,&bow,&ammunition,&stateItm};
  for(auto i:mesh)
    syncAttaches(*i);
  for(auto& i:item)
    syncAttaches(i);
  for(auto& i:effects) {
    syncAttaches(i);
    i.view.setTarget(targetPos);
    }
  syncAttaches(pfx);
  }

template<class View>
void MdlVisual::syncAttaches(Attach<View>& att) {
  auto& pose = *skInst;
  if(att.view.isEmpty())
    return;
  auto p = pos;
  if(att.boneId<pose.tr.size())
    p.mul(pose.tr[att.boneId]);
  att.view.setObjMatrix(p);
  }

bool MdlVisual::startAnimItem(Npc &npc, const char *scheme) {
  return skInst->setAnimItem(solver,npc,scheme);
  }

bool MdlVisual::startAnimSpell(Npc &npc, const char *scheme) {
  char name[128]={};
  std::snprintf(name,sizeof(name),"S_%sSHOOT",scheme);

  const Animation::Sequence *sq = solver.solveFrm(name);
  if(skInst->startAnim(solver,sq,BS_CASTING,Pose::Force,npc.world().tickCount())) {
    return true;
    }
  return false;
  }

bool MdlVisual::startAnimDialog(Npc &npc) {
  if((npc.bodyState()&BS_FLAG_FREEHANDS)==0 || fgtMode!=WeaponState::NoWeapon)
    return true;

  //const int countG1 = 21;
  const int countG2 = 11;
  const int id      = std::rand()%countG2 + 1;

  char name[32]={};
  std::snprintf(name,sizeof(name),"T_DIALOGGESTURE_%02d",id);

  const Animation::Sequence *sq = solver.solveFrm(name);
  if(skInst->startAnim(solver,sq,BS_STAND,Pose::NoHint,npc.world().tickCount())) {
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

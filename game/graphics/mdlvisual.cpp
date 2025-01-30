#include "mdlvisual.h"

#include "graphics/mesh/skeleton.h"
#include "game/serialize.h"
#include "utils/string_frm.h"
#include "world/objects/npc.h"
#include "world/objects/interactive.h"
#include "world/objects/item.h"
#include "world/world.h"
#include "objvisual.h"
#include "gothic.h"

using namespace Tempest;

MdlVisual::MdlVisual()
  :skInst(std::make_unique<Pose>()) {
  pos.identity();
  }

MdlVisual::~MdlVisual() {
  }

void MdlVisual::save(Serialize &fout, const Npc&) const {
  fout.write(fgtMode);
  if(skeleton!=nullptr)
    fout.write(skeleton->name()); else
    fout.write(std::string(""));
  solver.save(fout);
  skInst->save(fout);
  }

void MdlVisual::save(Serialize& fout, const Interactive&) const {
  solver.save(fout);
  skInst->save(fout);
  }

void MdlVisual::load(Serialize& fin, Npc& npc) {
  std::string s;

  fin.read(fgtMode);
  fin.read(s);
  npc.setVisual(s);

  solver.load(fin);
  skInst->load(fin,solver);
  if(skeleton!=nullptr)
    rebindAttaches(*skeleton);
  }

void MdlVisual::load(Serialize& fin, Interactive&) {
  solver.load(fin);
  skInst->load(fin,solver);
  if(skeleton!=nullptr)
    rebindAttaches(*skeleton);
  }

// Mdl_SetVisual
void MdlVisual::setVisual(const Skeleton *v) {
  if(v!=nullptr)
    rebindAttaches(*v);
  solver.setSkeleton(v);
  skInst->setSkeleton(v);
  view.setSkeleton(v);

  skeleton = v;
  setObjMatrix(pos); // invalidate obj matrix
  }

void MdlVisual::setYTranslationEnable(bool e) {
  if(e)
    skInst->setFlags(Pose::NoFlags); else
    skInst->setFlags(Pose::NoTranslation);
  }

void MdlVisual::setVisualBody(World& owner, MeshObjects::Mesh&& body) {
  implSetBody(nullptr,owner,std::move(body),0);
  }

// Mdl_SetVisualBody
void MdlVisual::setVisualBody(Npc& npc, MeshObjects::Mesh &&h, MeshObjects::Mesh &&body, int32_t version) {
  bind(head,std::move(h),"BIP01 HEAD");
  implSetBody(&npc,npc.world(),std::move(body),version);
  head.view.setAsGhost(hnpcFlagGhost);
  head.view.setFatness(npc.fatness());
  }

bool MdlVisual::hasOverlay(const Skeleton* sk) const {
  return solver.hasOverlay(sk);
  }

// Mdl_ApplyOverlayMdsTimed, Mdl_ApplyOverlayMds
void MdlVisual::addOverlay(const Skeleton *sk, uint64_t time) {
  solver.addOverlay(sk,time);
  }

// Mdl_RemoveOverlayMDS
void MdlVisual::delOverlay(std::string_view sk) {
  solver.delOverlay(sk);
  }

// Mdl_RemoveOverlayMDS
void MdlVisual::delOverlay(const Skeleton *sk) {
  solver.delOverlay(sk);
  }

void MdlVisual::clearOverlays() {
  solver.clearOverlays();
  }

void MdlVisual::setBody(Npc& npc, MeshObjects::Mesh&& a, const int32_t version) {
  implSetBody(&npc,npc.world(),std::move(a),version);
  setObjMatrix(pos);
  }

void MdlVisual::setArmour(Npc& npc, MeshObjects::Mesh&& armour) {
  // NOTE: Giant_Bug have no version tag in attachments;
  // Light dragon hunter armour has broken attachment with no tags
  // Big dragon hunter armour has many atachments with version tags
  implSetBody(&npc,npc.world(),std::move(armour),-1);
  setObjMatrix(pos);
  }

void MdlVisual::implSetBody(Npc* npc, World& world, MeshObjects::Mesh&& body, const int32_t version) {
  attach.clear();
  if(auto p = body.protoMesh()) {
    for(auto& att:p->attach) {
      if(!att.hasNode) {
        auto view = world.addAtachView(att,version);
        setSlotAttachment(std::move(view),att.name);
        }
      }
    syncAttaches();
    }
  view = std::move(body);
  view.setAsGhost(hnpcFlagGhost);
  view.setFatness(npc==nullptr ? 0 : npc->fatness());
  view.setSkeleton(skeleton);
  view.setPose(pos,*skInst);
  hnpcVisual.view.setMesh(&view);
  }

void MdlVisual::setSlotAttachment(MeshObjects::Mesh&& itm, std::string_view bone) {
  if(bone.empty() || skeleton==nullptr)
    return;

  size_t id = skeleton->findNode(bone);
  if(id==size_t(-1))
    return;

  for(auto& i:attach) {
    if(i.boneId==id) {
      i.view=std::move(itm);
      syncAttaches();
      return;
      }
    }

  MeshAttach slt;
  slt.bone = skeleton->nodes[id].name;
  bind(slt,std::move(itm),slt.bone);
  attach.push_back(std::move(slt));
  }

void MdlVisual::setSword(MeshObjects::Mesh &&s) {
  bind(sword,std::move(s),"ZS_RIGHTHAND");
  }

void MdlVisual::setRangeWeapon(MeshObjects::Mesh &&b) {
  bind(bow,std::move(b),"ZS_BOW");
  }

void MdlVisual::setShield(MeshObjects::Mesh&& s) {
  bind(shield,std::move(s),"ZS_SHIELD");
  }

void MdlVisual::setAmmoItem(MeshObjects::Mesh&& a, std::string_view bone) {
  bind(ammunition,std::move(a),bone);
  }

void MdlVisual::setMagicWeapon(Effect&& spell, World& owner) {
  auto n = std::move(pfx.view);
  n.setLooped(false);
  n.setActive(false);
  startEffect(owner,std::move(n),0,true);

  pfx.view = std::move(spell);
  pfx.view.setLooped(true);
  pfx.view.setPhysicsDisable();
  if(skeleton!=nullptr)
    pfx.view.bindAttaches(*skInst,*skeleton);
  }

void MdlVisual::setMagicWeaponKey(World& owner, SpellFxKey key, int32_t keyLvl) {
  pfx.view.setKey(owner,key,keyLvl);
  }

void MdlVisual::setSlotItem(MeshObjects::Mesh &&itm, std::string_view bone) {
  if(bone.empty() || skeleton==nullptr)
    return;

  size_t id = skeleton->findNode(bone);
  if(id==size_t(-1))
    return;

  // HACK: light dragon hunter armour
  if(id==head.boneId && !head.view.isEmpty())
    return;

  for(auto& i:item) {
    if(i.boneId==id) {
      i.view=std::move(itm);
      syncAttaches();
      return;
      }
    }

  MeshAttach slt;
  slt.bone = skeleton->nodes[id].name;
  bind(slt,std::move(itm),slt.bone);
  item.push_back(std::move(slt));
  syncAttaches();
  }

void MdlVisual::setStateItem(MeshObjects::Mesh&& a, std::string_view bone) {
  bind(stateItm,std::move(a),bone);
  syncAttaches();
  }

void MdlVisual::clearSlotItem(std::string_view bone) {
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

bool MdlVisual::setFightMode(zenkit::MdsFightMode mode) {
  WeaponState f = WeaponState::NoWeapon;

  switch(mode) {
    case zenkit::MdsFightMode::INVALID:
      return false;
    case zenkit::MdsFightMode::NONE:
      f=WeaponState::NoWeapon;
      break;
    case zenkit::MdsFightMode::FIST:
      f=WeaponState::Fist;
      break;
    case zenkit::MdsFightMode::SINGLE_HANDED:
      f=WeaponState::W1H;
      break;
    case zenkit::MdsFightMode::DUAL_HANDED:
      f=WeaponState::W2H;
      break;
    case zenkit::MdsFightMode::BOW:
      f=WeaponState::Bow;
      break;
    case zenkit::MdsFightMode::CROSSBOW:
      f=WeaponState::CBow;
      break;
    case zenkit::MdsFightMode::MAGIC:
      f=WeaponState::Mage;
      break;
    }

  return setToFightMode(f);
  }

void MdlVisual::dropWeapon(Npc& npc) {
  MeshAttach* att  = &sword;
  auto&       pose = *skInst;

  if(fgtMode!=WeaponState::W1H && fgtMode!=WeaponState::W2H &&
     fgtMode!=WeaponState::Bow && fgtMode!=WeaponState::CBow)
    return;

  auto p = pos;
  if(att->boneId<pose.boneCount())
    p = pose.bone(att->boneId);

  Item* itm = nullptr;
  if(fgtMode==WeaponState::W1H || fgtMode==WeaponState::W2H)
    itm = npc.currentMeleeWeapon(); else
    itm = npc.currentRangeWeapon();

  if(itm==nullptr)
    return;

  auto it = npc.world().addItemDyn(itm->clsId(),p,npc.handle().symbol_index());
  it->setCount(1);

  npc.delItem(itm->clsId(),1);
  }

void MdlVisual::startEffect(World& owner, Effect&& vfx, int32_t slot, bool noSlot) {
  uint64_t timeUntil = vfx.effectPrefferedTime();
  if(timeUntil!=0)
    timeUntil+=owner.tickCount();

  if(skeleton==nullptr)
    return;

  vfx.setMesh(&view);
  vfx.setBullet(nullptr,owner);

  for(auto& i:effects) {
    if(i.id==slot && !i.noSlot) {
      i.timeUntil = timeUntil;
      i.view = std::move(vfx);
      i.view.bindAttaches(*skInst,*skeleton);
      syncAttaches();
      return;
      }
    }

  PfxSlot slt;
  slt.id        = slot;
  slt.timeUntil = timeUntil;
  slt.noSlot    = noSlot;
  slt.view      = std::move(vfx);
  slt.view.bindAttaches(*skInst,*skeleton);
  effects.push_back(std::move(slt));
  syncAttaches();
  }

void MdlVisual::stopEffect(const VisualFx& vfx) {
  for(size_t i=0;i<effects.size();++i) {
    if(effects[i].view.is(vfx)) {
      effects[i] = std::move(effects.back());
      effects.pop_back();
      syncAttaches();
      return;
      }
    }
  }

void MdlVisual::stopEffect(int32_t slot) {
  for(size_t i=0;i<effects.size();++i) {
    if(effects[i].id==slot) {
      effects[i] = std::move(effects.back());
      effects.pop_back();
      syncAttaches();
      return;
      }
    }
  }

void MdlVisual::setNpcEffect(World& owner, Npc& npc, std::string_view s, zenkit::NpcFlag flags) {
  if(hnpcVisualName!=s) {
    hnpcVisualName = s;
    auto vfx = Gothic::inst().loadVisualFx(s);
    if(vfx==nullptr) {
      hnpcVisual.view = Effect();
      return;
      }
    hnpcVisual.view = Effect(*vfx,owner,npc,SpellFxKey::Count);
    if(skeleton!=nullptr)
      hnpcVisual.view.bindAttaches(*skInst,*skeleton);
    hnpcVisual.view.setActive(true);
    hnpcVisual.view.setLooped(true);
    hnpcVisual.view.setMesh(&view);
    }

  const bool nextGhost = (flags & zenkit::NpcFlag::GHOST);
  if(hnpcFlagGhost!=nextGhost) {
    hnpcFlagGhost=nextGhost;
    view.setAsGhost(hnpcFlagGhost);
    head.view.setAsGhost(hnpcFlagGhost);
    for(auto& i:attach)
      i.view.setAsGhost(hnpcFlagGhost);
    }
  }

void MdlVisual::setFatness(float fatness) {
  view.setFatness(fatness);
  head.view.setFatness(fatness);
  for(auto& i:attach)
    i.view.setFatness(fatness);
  }

void MdlVisual::emitBlockEffect(Npc& dest, Npc& source) {
  auto& world = source.world();

  auto& pose  = *skInst;
  auto  p     = pos;
  if(sword.boneId<pose.boneCount())
    p = pose.bone(sword.boneId);

  auto src = source.inventory().activeWeapon();
  auto dst = dest  .inventory().activeWeapon();

  if(src==nullptr || dst==nullptr)
    return;

  auto s = world.addWeaponBlkEffect(ItemMaterial(src->handle().material),ItemMaterial(dst->handle().material),p);
  s.play();
  }

bool MdlVisual::setToFightMode(const WeaponState f) {
  if(f==fgtMode)
    return false;
  fgtMode = f;
  return true;
  }

void MdlVisual::setObjMatrix(const Tempest::Matrix4x4 &m, bool syncAttach) {
  pos = m;
  skInst->setObjectMatrix(m,syncAttach);
  view.setPose(m,*skInst);
  if(syncAttach)
    syncAttaches();
  }

void MdlVisual::setHeadRotation(float dx, float dz) {
  skInst->setHeadRotation(dx,dz);
  syncAttaches(head);
  }

Vec2 MdlVisual::headRotation() const {
  return skInst->headRotation();
  }

void MdlVisual::updateWeaponSkeleton(const Item* weapon, const Item* range) {
  auto st = fgtMode;
  if(st==WeaponState::W1H || st==WeaponState::W2H){
    bind(sword, "ZS_RIGHTHAND");
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

  bind(shield, st==WeaponState::W1H ? "ZS_LEFTARM" : "ZS_SHIELD");

  pfx.view.setActive(st==WeaponState::Mage);
  syncAttaches();
  }

void MdlVisual::setTorch(bool t, World& owner) {
  if(!t) {
    torch.view.reset();
    return;
    }
  size_t torchId = owner.script().findSymbolIndex("ItLsTorchburning");
  if(torchId==size_t(-1))
    return;

  auto hitem = std::make_shared<zenkit::IItem>();
  owner.script().initializeInstanceItem(hitem, torchId);
  torch.view.reset(new ObjVisual());
  torch.view->setVisual(*hitem,owner,false);
  torch.boneId = (skeleton==nullptr ? size_t(-1) : skeleton->findNode("ZS_LEFTHAND"));
  }

bool MdlVisual::isUsingTorch() const {
  return torch.view!=nullptr;
  }

bool MdlVisual::updateAnimation(Npc* npc, World& world, uint64_t dt) {
  Pose&    pose      = *skInst;
  uint64_t tickCount = world.tickCount();
  auto     pos3      = Vec3{pos.at(3,0), pos.at(3,1), pos.at(3,2)};

  if(npc!=nullptr && world.isInSfxRange(pos3))
    pose.processSfx(*npc,tickCount);
  if(world.isInPfxRange(pos3))
    pose.processPfx(*this,world,tickCount);

  for(size_t i=0;i<effects.size();) {
    if(effects[i].timeUntil<tickCount) {
      effects[i] = std::move(effects.back());
      effects.pop_back();
      } else {
      effects[i].view.tick(dt);
      ++i;
      }
    }

  solver.update(tickCount);
  pose.setObjectMatrix(pos,false);
  const bool changed = pose.update(tickCount);

  if(changed)
    view.setPose(pos,pose);
  return changed;
  }

void MdlVisual::processLayers(World& world) {
  Pose&    pose      = *skInst;
  uint64_t tickCount = world.tickCount();
  pose.processLayers(solver,tickCount);
  }

bool MdlVisual::processEvents(World& world, uint64_t &barrier, Animation::EvCount &ev) {
  Pose&    pose      = *skInst;
  uint64_t tickCount = world.tickCount();
  return pose.processEvents(barrier,tickCount,ev);
  }

Vec3 MdlVisual::mapBone(const size_t boneId) const {
  Pose&  pose = *skInst;
  if(boneId==size_t(-1))
    return {pos.at(3,0), pos.at(3,1), pos.at(3,2)};

  auto mat = pose.bone(boneId);
  return {mat.at(3,0), mat.at(3,1), mat.at(3,2)};
  }

Vec3 MdlVisual::mapWeaponBone() const {
  if(fgtMode==WeaponState::Bow || fgtMode==WeaponState::CBow)
    return mapBone(ammunition.boneId);
  if(fgtMode==WeaponState::Mage && skeleton!=nullptr)
    return mapBone(skeleton->findNode("ZS_RIGHTHAND"));
  return {pos.at(3,0), pos.at(3,1), pos.at(3,2)};
  }

Vec3 MdlVisual::mapHeadBone() const {
  if(skeleton->BIP01_HEAD==size_t(-1))
    return {pos.at(3,0), pos.at(3,1)+180, pos.at(3,2)};
  return mapBone(skeleton->BIP01_HEAD);
  }

void MdlVisual::stopAnim(Npc& npc, std::string_view anim) {
  skInst->stopAnim(anim);
  if(!skInst->hasAnim())
    startAnimAndGet(npc,AnimationSolver::Idle,0,fgtMode,npc.walkMode());
  }

bool MdlVisual::stopItemStateAnim(Npc& npc) {
  if(!skInst->stopItemStateAnim(solver,npc.world().tickCount()))
    return false;
  if(!skInst->hasAnim())
    startAnimAndGet(npc,AnimationSolver::Idle,0,fgtMode,npc.walkMode());
  return true;
  }

bool MdlVisual::hasAnim(std::string_view scheme) const {
  return solver.solveFrm(scheme)!=nullptr;
  }

void MdlVisual::stopWalkAnim(Npc &npc) {
  skInst->stopWalkAnim();
  if(!skInst->hasAnim())
    startAnimAndGet(npc,AnimationSolver::Idle,0,fgtMode,npc.walkMode());
  }

bool MdlVisual::isStanding() const {
  return skInst->isStanding();
  }

bool MdlVisual::isAnimExist(std::string_view name) const {
  const Animation::Sequence *sq = solver.solveFrm(name);
  return sq!=nullptr;
  }

const Animation::Sequence* MdlVisual::startAnimAndGet(std::string_view name, uint64_t tickCount, bool forceAnim) {
  auto sq = solver.solveFrm(name);
  if(sq!=nullptr) {
    const Pose::StartHint hint = Pose::StartHint(forceAnim  ? Pose::Force : Pose::NoHint);
    if(skInst->startAnim(solver,sq,0,BS_NONE,hint,tickCount)) {
      return sq;
      }
    }
  return nullptr;
  }

const Animation::Sequence* MdlVisual::startAnimAndGet(Npc &npc, std::string_view name, uint8_t comb, BodyState bs) {
  const Animation::Sequence* sq  = solver.solveFrm(name);
  if(skInst->startAnim(solver,sq,comb,bs,Pose::NoHint,npc.world().tickCount()))
    return sq;
  return nullptr;
  }

const Animation::Sequence* MdlVisual::startAnimAndGet(Npc& npc, AnimationSolver::Anim a,
                                                      uint8_t comb,
                                                      WeaponState st, WalkBit wlk) {
  // for those use MdlVisual::setRotation
  assert(a!=AnimationSolver::Anim::RotL && a!=AnimationSolver::Anim::RotR &&
         a!=AnimationSolver::Anim::WhirlL && a!=AnimationSolver::Anim::WhirlR);

  if(a==AnimationSolver::InteractIn ||
     a==AnimationSolver::InteractOut ||
     a==AnimationSolver::InteractToStand ||
     a==AnimationSolver::InteractFromStand) {
    auto inter = npc.interactive();
    const Animation::Sequence *sq = solver.solveAnim(inter,a,*skInst);
    if(sq!=nullptr){
      auto bs=inter->isLadder() ? BS_CLIMB : BS_MOBINTERACT;
      if(skInst->startAnim(solver,sq,comb,bs,Pose::NoHint,npc.world().tickCount()))
        return sq;
      }
    return nullptr;
    }

  if(a==AnimationSolver::Anim::NoAnim) {
    skInst->stopAllAnim();
    return nullptr;
    }

  const Animation::Sequence *sq = solver.solveAnim(a,st,wlk,*skInst);
  if(sq==nullptr)
    return nullptr;

  bool forceAnim=false;
  if(a==AnimationSolver::Anim::DeadA        || a==AnimationSolver::Anim::DeadB ||
     a==AnimationSolver::Anim::UnconsciousA || a==AnimationSolver::Anim::UnconsciousB ||
     a==AnimationSolver::Anim::FallDeepA    || a==AnimationSolver::Anim::FallDeepB) {
    skInst->stopAllAnim();
    forceAnim = true;
    }
  if(a==AnimationSolver::Anim::JumpHang) {
    forceAnim = true;
    }

  BodyState bs = BS_NONE;
  switch(a) {
    case AnimationSolver::Anim::NoAnim:
      bs = BS_NONE;
      break;
    case AnimationSolver::Anim::Fallen:
    case AnimationSolver::Anim::FallenA:
    case AnimationSolver::Anim::FallenB:
      bs = BS_LIE;
      break;
    case AnimationSolver::Anim::Idle:
    case AnimationSolver::Anim::MagNoMana:
      bs = BS_STAND;
      break;
    case AnimationSolver::Anim::Move:
    case AnimationSolver::Anim::MoveL:
    case AnimationSolver::Anim::MoveR:
      if(bool(wlk & WalkBit::WM_Walk))
        bs = BS_WALK; else
        bs = BS_RUN;
      break;
    case AnimationSolver::Anim::MoveBack:
      if(st==WeaponState::Fist || st==WeaponState::W1H || st==WeaponState::W2H)
        bs = BS_PARADE; else
        bs = BS_RUN;
    case AnimationSolver::Anim::RotL:
    case AnimationSolver::Anim::RotR:
    case AnimationSolver::Anim::WhirlL:
    case AnimationSolver::Anim::WhirlR:
      break;
    case AnimationSolver::Anim::Fall:
    case AnimationSolver::Anim::FallDeep:
    case AnimationSolver::Anim::FallDeepA:
    case AnimationSolver::Anim::FallDeepB:
      bs = BS_FALL;
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
    case AnimationSolver::Anim::SlideA:
    case AnimationSolver::Anim::SlideB:
      bs = BS_NONE;
      break;
    case AnimationSolver::Anim::DeadA:
    case AnimationSolver::Anim::DeadB:
      bs = BS_DEAD;
      break;
    case AnimationSolver::Anim::UnconsciousA:
    case AnimationSolver::Anim::UnconsciousB:
      bs = BS_UNCONSCIOUS;
      break;
    case AnimationSolver::Anim::InteractIn:
    case AnimationSolver::Anim::InteractOut:
    case AnimationSolver::Anim::InteractToStand:
    case AnimationSolver::Anim::InteractFromStand:
      bs = BS_MOBINTERACT;
      break;
    case AnimationSolver::Anim::Attack:
    case AnimationSolver::Anim::AttackL:
    case AnimationSolver::Anim::AttackR:
    case AnimationSolver::Anim::AttackFinish:
      bs = BS_HIT;
      break;
    case AnimationSolver::Anim::AttackBlock:
      bs = BS_PARADE;
      break;
    case AnimationSolver::Anim::StumbleA:
    case AnimationSolver::Anim::StumbleB:
      bs = BS_STUMBLE;
      break;
    case AnimationSolver::Anim::AimBow:
      bs = BS_AIMNEAR; //TODO: BS_AIMFAR
      break;
    case AnimationSolver::Anim::ItmGet:
      bs = BS_TAKEITEM;
      break;
    case AnimationSolver::Anim::ItmDrop:
      bs = BS_DROPITEM;
      break;
    case AnimationSolver::Anim::PointAt:
      bs = BS_STAND;
      break;
    }

  if(bool(wlk & WalkBit::WM_Dive))
    bs = BS_DIVE;
  else if(bool(wlk & WalkBit::WM_Swim))
    bs = BS_SWIM;
  else if(bool(wlk & WalkBit::WM_Sneak) && (st!=WeaponState::Bow && st!=WeaponState::CBow))
    bs = BS_SNEAK;

  Pose::StartHint hint = (forceAnim ? Pose::Force : Pose::NoHint);

  if(skInst->startAnim(solver,sq,comb,bs,hint,npc.world().tickCount()))
    return sq;
  return nullptr;
  }

bool MdlVisual::startAnim(Npc &npc, WeaponState st) {
  const bool run = (skInst->bodyState()&BS_MAX)==BS_RUN;

  if(st==fgtMode)
    return true;
  const Animation::Sequence *sq = solver.solveAnim(st,fgtMode,run);
  if(sq==nullptr)
    return false;
  if(skInst->startAnim(solver,sq,0,run ? BS_RUN : BS_NONE,Pose::NoHint,npc.world().tickCount()))
    return true;
  return false;
  }

void MdlVisual::setAnimRotate(Npc &npc, int dir) {
  skInst->setAnimRotate(solver,npc,fgtMode,TURN_ANIM_STD,dir);
  }

void MdlVisual::setAnimWhirl(Npc &npc, int dir) {
  skInst->setAnimRotate(solver,npc,fgtMode,TURN_ANIM_WHIRL,dir);
  }

void MdlVisual::interrupt() {
  skInst->interrupt();
  item.clear();
  setStateItem(MeshObjects::Mesh(),"");
  }

Tempest::Vec3 MdlVisual::displayPosition() const {
  if(skeleton!=nullptr)
    return {0,skeleton->colisionHeight()*1.5f,0};
  return {0.f,0.f,0.f};
  }

float MdlVisual::viewDirection() const {
  auto p = pos;
  if(nullptr!=skeleton) {
    size_t nodeId = skeleton->findNode("BIP01");
    if(nodeId!=size_t(-1))
      p = pose().bone(nodeId);
    }
  float rx = p.at(2,0);
  float rz = p.at(2,2);
  return float(std::atan2(rz,rx)) * 180.f / float(M_PI);
  }

const Animation::Sequence* MdlVisual::continueCombo(Npc& npc, AnimationSolver::Anim a, BodyState bs,
                                                    WeaponState st, WalkBit wlk)  {
  if(st==WeaponState::Fist || st==WeaponState::W1H || st==WeaponState::W2H) {
    const Animation::Sequence *sq = solver.solveAnim(a,st,wlk,*skInst);
    if(auto ret = skInst->continueCombo(solver,sq,bs,npc.world().tickCount()))
      return ret;
    }
  return startAnimAndGet(npc,a,0,st,wlk);
  }

uint16_t MdlVisual::comboLength() const {
  return skInst->comboLength();
  }

Bounds MdlVisual::bounds() const {
  auto b = view.bounds();
  if(!head.view.isEmpty())
    b.assign(b,head.view.bounds());
  return b;
  }

void MdlVisual::bind(MeshAttach& slot, MeshObjects::Mesh&& itm, std::string_view bone) {
  slot.boneId = skeleton==nullptr ? size_t(-1) : skeleton->findNode(bone);
  slot.view   = std::move(itm);
  slot.bone   = bone;
  // sync?
  }

void MdlVisual::bind(PfxAttach& slot, Effect&& itm, std::string_view bone) {
  slot.boneId = skeleton==nullptr ? size_t(-1) : skeleton->findNode(bone);
  slot.view   = std::move(itm);
  slot.bone   = bone;
  // sync?
  }

template<class View>
void MdlVisual::bind(Attach<View>& slot, std::string_view bone) {
  slot.boneId = skeleton==nullptr ? size_t(-1) : skeleton->findNode(bone);
  slot.bone   = bone;
  // sync?
  }

void MdlVisual::rebindAttaches(const Skeleton& to) {
  auto  mesh = {&head,&sword,&bow,&ammunition,&stateItm};
  for(auto i:mesh)
    rebindAttaches(*i,to);
  for(auto& i:item)
    rebindAttaches(i,to);
  for(auto& i:attach)
    rebindAttaches(i,to);
  for(auto& i:effects)
    i.view.bindAttaches(*skInst,to);
  pfx.view.bindAttaches(*skInst,to);
  hnpcVisual.view.bindAttaches(*skInst,to);
  }

template<class View>
void MdlVisual::rebindAttaches(Attach<View>& mesh, const Skeleton& to) {
  if(mesh.bone.empty())
    mesh.boneId = size_t(-1); else
    mesh.boneId = to.findNode(mesh.bone);
  }

void MdlVisual::syncAttaches() {
  MdlVisual::MeshAttach* mesh[] = {&head, &sword,&shield,&bow,&ammunition,&stateItm};
  for(auto i:mesh)
    syncAttaches(*i);
  for(auto& i:item)
    syncAttaches(i);
  for(auto& i:attach)
    syncAttaches(i);
  for(auto& i:effects) {
    i.view.setObjMatrix(pos);
    // i.view.setTarget(targetPos);
    }
  pfx.view.setObjMatrix(pos);
  hnpcVisual.view.setObjMatrix(pos);
  if(torch.view!=nullptr) {
    auto& pose = *skInst;
    auto  p    = pos;
    if(torch.boneId<pose.boneCount())
      p = pose.bone(torch.boneId);
    torch.view->setObjMatrix(p);
    }
  }

const Skeleton* MdlVisual::visualSkeleton() const {
  return skeleton;
  }

std::string_view MdlVisual::visualSkeletonScheme() const {
  if(skeleton==nullptr)
    return "";
  auto ret = skeleton->name();
  auto end = ret.find_first_of("._");
  return ret.substr(0, end);
  }

template<class View>
void MdlVisual::syncAttaches(Attach<View>& att) {
  if(att.view.isEmpty())
    return;
  auto& pose = *skInst;
  auto  p    = pos;
  if(att.boneId<pose.boneCount())
    p = pose.bone(att.boneId);
  att.view.setObjMatrix(p);
  }

const Animation::Sequence* MdlVisual::startAnimItem(Npc &npc, std::string_view scheme, int state) {
  return skInst->setAnimItem(solver,npc,scheme,state);
  }

const Animation::Sequence* MdlVisual::startAnimSpell(Npc &npc, std::string_view scheme, bool invest) {
  const bool run = (skInst->bodyState()&BS_MAX)==BS_RUN; // not really the case, as in Gothic player can't cast spell, while running

  const Animation::Sequence *sq = solver.solveAnim(scheme,run,invest);
  if(skInst->startAnim(solver,sq,0,BS_CASTING,Pose::NoHint,npc.world().tickCount())) {
    return sq;
    }
  return nullptr;
  }

bool MdlVisual::startAnimDialog(Npc &npc) {
  if((npc.bodyState()&BS_FLAG_FREEHANDS)==0 || fgtMode!=WeaponState::NoWeapon)
    return true;
  if(npc.bodyStateMasked()!=BS_STAND)
    return true;

  const uint16_t count = Gothic::inst().version().dialogGestureCount();
  const int      id    = std::rand()%count + 1;

  char name[32]={};
  std::snprintf(name,sizeof(name),"T_DIALOGGESTURE_%02d",id);

  const Animation::Sequence *sq = solver.solveFrm(name);
  if(skInst->startAnim(solver,sq,0,BS_STAND,Pose::NoHint,npc.world().tickCount()))
    return true;
  return false;
  }

void MdlVisual::startMMAnim(Npc&, std::string_view anim, std::string_view bone) {
  MdlVisual::MeshAttach* mesh[] = {&head,&sword,&shield,&bow,&ammunition,&stateItm};
  for(auto i:mesh) {
    if(i->bone!=bone)
      continue;
    i->view.startMMAnim(anim,1,uint64_t(-1));
    }
  }

void MdlVisual::startFaceAnim(Npc& npc, std::string_view anim, float intensity, uint64_t duration) {
  if(duration!=uint64_t(-1) && duration!=0)
    duration += npc.world().tickCount();
  head.view.startMMAnim(anim,intensity,duration);
  }

void MdlVisual::stopDlgAnim(Npc& npc) {
  const uint16_t count = Gothic::inst().version().dialogGestureCount();
  for(uint16_t i=0; i<count; i++){
    char buf[32]={};
    std::snprintf(buf,sizeof(buf),"T_DIALOGGESTURE_%02d",i+1);
    skInst->stopAnim(buf);
    }

  if(npc.processPolicy()<=Npc::AiNormal) {
    // avoid PCI traffic on distant npc's
    startFaceAnim(npc,"VISEME",1,0);
    }
  }

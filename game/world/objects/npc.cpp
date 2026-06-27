#include "npc.h"

#include <Tempest/Matrix4x4>
#include <Tempest/Log>

#include "graphics/mesh/skeleton.h"
#include "graphics/visualfx.h"
#include "game/damagecalculator.h"
#include "game/serialize.h"
#include "game/gamescript.h"
#include "utils/string_frm.h"
#include "world/objects/interactive.h"
#include "world/objects/item.h"
#include "world/world.h"
#include "utils/versioninfo.h"
#include "utils/fileext.h"
#include "utils/dbgpainter.h"
#include "camera.h"
#include "gothic.h"
#include "resources.h"

using namespace Tempest;

static std::string_view humansTorchOverlay = "_TORCH.MDS";

std::string_view Npc::Routine::wayPointName() const {
  return point!=nullptr ? point->name : fallbackName;
  }


void Npc::GoTo::save(Serialize& fout) const {
  fout.write(npc, uint8_t(flag), wp, pos);
  }

void Npc::GoTo::load(Serialize& fin) {
  fin.read(npc, reinterpret_cast<uint8_t&>(flag), wp, pos);
  //NOTE: no real need to version check
  //if(fin.version()<53) {
    if(flag==GoToHint::GT_Enemy || flag==GoToHint::GT_EnemyG)
      clear(); // not persistent flags, and should be cleared by FAI
  //  }
  }

Vec3 Npc::GoTo::target() const {
  if(npc!=nullptr)
    return npc->centerPosition();
  if(wp!=nullptr)
    return wp->position();
  return pos;
  }

bool Npc::GoTo::isClose(const Npc& self, float dist) const {
  if(flag==GT_Enemy)
    return self.fghAlgo.isInWRange(self, *npc, self.owner.script()); //need to be consistent with implAttack
  if(npc!=nullptr)
    return MoveAlgo::isClose(self, *npc, dist);
  if(wp!=nullptr)
    return MoveAlgo::isClose(self, *wp, dist);
  return MoveAlgo::isClose(self, pos, dist);
  }

bool Npc::GoTo::empty() const {
  return flag==Npc::GT_No;
  }

void Npc::GoTo::clear() {
  npc  = nullptr;
  wp   = nullptr;
  flag = Npc::GT_No;
  }

void Npc::GoTo::set(Npc* to, Npc::GoToHint hnt) {
  npc  = to;
  wp   = nullptr;
  flag = hnt;
  }

void Npc::GoTo::set(const WayPoint* to, GoToHint hnt) {
  npc  = nullptr;
  wp   = to;
  flag = hnt;
  }

void Npc::GoTo::set(const Item* to) {
  pos  = to->position();
  flag = Npc::GT_Item;
  }

void Npc::GoTo::set(const Vec3& to) {
  pos  = to;
  flag = GT_Point;
  }

void Npc::GoTo::setFlee() {
  flag = GT_Flee;
  }


struct Npc::TransformBack {
  TransformBack(Npc& self) {
    hnpc        = std::make_shared<zenkit::INpc>(*self.hnpc);
    invent      = std::move(self.invent);
    self.invent = Inventory(); // cleanup

    std::memcpy(talentsSk,self.talentsSk,sizeof(talentsSk));
    std::memcpy(talentsVl,self.talentsVl,sizeof(talentsVl));

    body     = std::move(self.body);
    head     = std::move(self.head);
    vHead    = self.vHead;
    vTeeth   = self.vTeeth;
    vColor   = self.vColor;
    bdColor  = self.bdColor;

    skeleton = self.visual.visualSkeleton();
    }

  TransformBack(Npc& owner, zenkit::DaedalusVm& vm, Serialize& fin) {
    hnpc           = std::make_shared<zenkit::INpc>();
    hnpc->user_ptr = this;
    fin.readNpc(vm, hnpc);
    invent.load(fin,owner);
    fin.read(talentsSk,talentsVl);
    fin.read(body,head,vHead,vTeeth,vColor,bdColor);

    std::string sk;
    fin.read(sk);
    skeleton = Resources::loadSkeleton(sk);
    }

  void undo(Npc& self) {
    int32_t aivar[zenkit::INpc::aivar_count]={};

    auto exp      = self.hnpc->exp;
    auto exp_next = self.hnpc->exp_next;
    auto lp       = self.hnpc->lp;
    auto level    = self.hnpc->level;
    std::memcpy(aivar,self.hnpc->aivar,sizeof(aivar));

    self.hnpc           = hnpc;
    self.hnpc->exp      = exp;
    self.hnpc->exp_next = exp_next;
    self.hnpc->lp       = lp;
    self.hnpc->level    = level;
    std::memcpy(self.hnpc->aivar,aivar,sizeof(aivar));

    self.invent = std::move(invent);
    std::memcpy(self.talentsSk,talentsSk,sizeof(talentsSk));
    std::memcpy(self.talentsVl,talentsVl,sizeof(talentsVl));

    self.body    = std::move(body);
    self.head    = std::move(head);
    self.vHead   = vHead;
    self.vTeeth  = vTeeth;
    self.vColor  = vColor;
    self.bdColor = bdColor;
    }

  void save(Serialize& fout) {
    fout.write(*hnpc);
    invent.save(fout);
    fout.write(talentsSk,talentsVl);
    fout.write(body,head,vHead,vTeeth,vColor,bdColor);
    fout.write(skeleton!=nullptr ? skeleton->name() : "");
    }

  std::shared_ptr<zenkit::INpc>   hnpc={};
  Inventory                       invent;
  int32_t                         talentsSk[TALENT_MAX_G2]={};
  int32_t                         talentsVl[TALENT_MAX_G2]={};

  std::string                     body,head;
  int32_t                         vHead=0, vTeeth=0, vColor=0;
  int32_t                         bdColor=0;

  const Skeleton*                 skeleton = nullptr;
  };


Npc::Npc(World &owner, size_t instance, std::string_view waypoint, NpcProcessPolicy aiPolicy)
  :owner(owner),aiPolicy(aiPolicy),mvAlgo(*this) {
  outputPipe     = owner.script().openAiOuput();

  hnpc           = std::make_shared<zenkit::INpc>();
  hnpc->user_ptr = this;
  hnpc->id       = int32_t(instance & 0x7FFFFFFF);
  hnpc->wp       = std::string(waypoint);

  if(instance==size_t(-1))
    return;

  owner.script().initializeInstanceNpc(hnpc, instance);

  // vanilla behavior: equip best weapon and set non-zero damage type
  if(!isPlayer())
    invent.autoEquipWeapons(*this);
  if(hnpc->damage_type==0)
    hnpc->damage_type = 2;
  setTrueGuild(hnpc->guild); // https://worldofplayers.ru/threads/12446/post-878087
  setPerceptionTime(5000);   // https://github.com/Try/OpenGothic/pull/720#issuecomment-2602908614
  }

Npc::~Npc(){
  if(currentInteract)
    currentInteract->detach(*this,true);
  }

void Npc::save(Serialize &fout, size_t id, std::string_view directory) {
  fout.setEntry("worlds/",fout.worldName(),directory,id,"/data");
  fout.write(*hnpc);
  fout.write(body,head,vHead,vTeeth,bdColor,vColor,bdFatness);
  fout.write(x,y,z,angle,sz);
  fout.write(wlkMode,trGuild,talentsSk,talentsVl,refuseTalkMilis);
  fout.write(permAttitude,tmpAttitude);
  fout.write(perceptionTime,perceptionNextTime);
  for(auto& i:perception)
    fout.write(i.func);

  // extra state
  fout.write(lastHitType,lastHitSpell);
  if(currentSpellCast<uint32_t(-1))
    fout.write(uint32_t(currentSpellCast)); else
    fout.write(uint32_t(-1));
  fout.write(uint8_t(castLevel),castNextTime,manaInvested,aiExpectedInvest);
  fout.write(spellInfo);

  saveTrState(fout);
  saveAiState(fout);

  fout.write(currentInteract,currentOther,currentVictim);
  fout.write(currentLookAt,currentLookAtNpc,currentTarget,nearestEnemy);

  go2.save(fout);
  fout.write(currentFp,currentFpLock);
  wayPath.save(fout);

  mvAlgo.save(fout);
  fghAlgo.save(fout);
  fout.write(lastEventTime,angleY,runAng);
  fout.write(invTorch);
  fout.write(isUsingTorch());

  Vec3 phyPos = physic.position();
  fout.write(phyPos);

  fout.setEntry("worlds/",fout.worldName(),directory,id,"/visual");
  visual.save(fout,*this);

  fout.setEntry("worlds/",fout.worldName(),directory,id,"/inventory");
  if(!invent.isEmpty() || id==size_t(-1))
    invent.save(fout);
  }

void Npc::load(Serialize &fin, size_t id, std::string_view directory) {
  fin.setEntry("worlds/",fin.worldName(),directory,id,"/data");

  hnpc = std::make_shared<zenkit::INpc>();
  hnpc->user_ptr        = this;
  fin.readNpc(owner.script().getVm(), hnpc);
  fin.read(body,head,vHead,vTeeth,bdColor,vColor,bdFatness);

  auto* sym = owner.script().findSymbol(hnpc->symbol_index());
  if (sym != nullptr)
    sym->set_instance(hnpc);

  fin.read(x,y,z,angle,sz);
  fin.read(wlkMode,trGuild,talentsSk,talentsVl,refuseTalkMilis);
  durtyTranform = TR_Pos|TR_Rot|TR_Scale;
  if(fin.version()<55)
    angle -= 90;

  fin.read(permAttitude,tmpAttitude);
  fin.read(perceptionTime,perceptionNextTime);
  for(auto& i:perception)
    fin.read(i.func);

  // extra state
  fin.read(lastHitType,lastHitSpell);
  {
  uint32_t currentSpellCastU32 = uint32_t(-1);
  fin.read(currentSpellCastU32);
  currentSpellCast = (currentSpellCastU32==uint32_t(-1) ? size_t(-1) : currentSpellCastU32);
  }
  fin.read(reinterpret_cast<uint8_t&>(castLevel),castNextTime);
  if(fin.version()>44)
    fin.read(manaInvested,aiExpectedInvest);
  fin.read(spellInfo);
  loadTrState(fin);
  loadAiState(fin);

  fin.read(currentInteract,currentOther,currentVictim);
  if(fin.version()>=42)
    fin.read(currentLookAt);
  fin.read(currentLookAtNpc,currentTarget,nearestEnemy);

  go2.load(fin);
  fin.read(currentFp,currentFpLock);
  wayPath.load(fin);

  mvAlgo.load(fin);
  fghAlgo.load(fin);
  fin.read(lastEventTime,angleY,runAng);

  bool isUsingTorch = false;
  if(fin.version()>36) {
    fin.read(invTorch);
    fin.read(isUsingTorch);
    }

  Vec3 phyPos = {};
  fin.read(phyPos);

  fin.setEntry("worlds/",fin.worldName(),directory,id,"/visual");
  visual.load(fin,*this);
  physic.setPosition(phyPos);

  setVisualBody(vHead,vTeeth,vColor,bdColor,body,head);

  if(fin.setEntry("worlds/",fin.worldName(),directory,id,"/inventory"))
    invent.load(fin,*this);

  // post-alignment
  updateTransform();
  if(isUsingTorch)
    visual.setTorch(true,owner);
  // NOTE: a downed NPC (dead OR unconscious) is a walk-through corpse for NPC-vs-NPC collision (see
  // onNoHealth); an NPC saved while unconscious must also load non-blocking, so gate on isDown().
  if(isDown())
    physic.setEnable(false);
  }

void Npc::postValidate() {
  if(currentInteract!=nullptr && !currentInteract->isAttached(*this))
    currentInteract = nullptr;
  }

void Npc::saveAiState(Serialize& fout) const {
  fout.write(aniWaitTime,waitTime,faiWaitTime,outWaitTime);
  fout.write(aiOutputBarrier); // v56: SVM-overlay output barrier was lost on reload
  fout.write(uint8_t(aiPolicy));
  fout.write(aiState.funcIni,aiState.funcLoop,aiState.funcEnd,aiState.sTime,aiState.eTime,aiState.started,aiState.loopNextTime);
  fout.write(aiPrevState);

  aiQueue.save(fout);
  aiQueueOverlay.save(fout);

  fout.write(uint32_t(routines.size()));
  for(auto& i:routines) {
    fout.write(i.start,i.end,i.callback,i.point,i.fallbackName);
    }
  }

void Npc::loadAiState(Serialize& fin) {
  fin.read(aniWaitTime);
  fin.read(waitTime,faiWaitTime);
  fin.read(outWaitTime);
  if(fin.version()>55)
    fin.read(aiOutputBarrier); // v56: persist the AI-output barrier (overlay-SVM path)
  fin.read(reinterpret_cast<uint8_t&>(aiPolicy));
  fin.read(aiState.funcIni,aiState.funcLoop,aiState.funcEnd,aiState.sTime,aiState.eTime,aiState.started,aiState.loopNextTime);
  fin.read(aiPrevState);

#ifndef NDEBUG
  if(auto s = owner.script().findSymbol(aiState.funcIni.ptr)) {
    aiState.hint = s->name().c_str();
    }
#endif

  aiQueue.load(fin);
  aiQueueOverlay.load(fin);

  uint32_t size=0;
  fin.read(size);
  routines.resize(size);
  for(auto& i:routines) {
    fin.read(i.start,i.end,i.callback,i.point);
    if(fin.version()>51)
      fin.read(i.fallbackName);
    }
  }

void Npc::saveTrState(Serialize& fout) const {
  if(transformSpl!=nullptr) {
    fout.write(true);
    transformSpl->save(fout);
    } else {
    fout.write(false);
    }
  }

void Npc::loadTrState(Serialize& fin) {
  bool hasTr = false;
  fin.read(hasTr);
  if(hasTr)
    transformSpl.reset(new TransformBack(*this, owner.script().getVm(), fin));
  }

bool Npc::setPosition(float ix, float iy, float iz) {
  if(x==ix && y==iy && z==iz)
    return false;
  x = ix;
  y = iy;
  z = iz;
  durtyTranform |= TR_Pos;
  physic.setPosition(Vec3{x,y,z});
  return true;
  }

bool Npc::setPosition(const Tempest::Vec3& pos) {
  return setPosition(pos.x,pos.y,pos.z);
  }

void Npc::setViewPosition(const Tempest::Vec3& pos) {
  x = pos.x;
  y = pos.y;
  z = pos.z;
  durtyTranform |= TR_Pos;
  }

int Npc::aiOutputOrderId() const {
  return aiQueue.aiOutputOrderId();
  }

bool Npc::performOutput(const AiQueue::AiAction &act) {
  if(act.target==nullptr) //FIXME: target is null after loading
    return true;
  const int order = act.target->aiOutputOrderId();
  if(order<act.i0)
    return false;
  if(aiOutputBarrier>owner.tickCount() && act.target==this && !isPlayer())
    return false;
  if(aiPolicy>=NpcProcessPolicy::AiFar)
    return true; // don't waste CPU on far-away svm-talks
  //if(act.act!=AI_OutputSvmOverlay && bodyStateMasked()!=BS_STAND)
  //  return false;
  if(act.act==AI_Output           && outputPipe->output   (*this,act.s0))
    return true;
  auto svm = owner.script().messageFromSvm(act.s0,hnpc->voice);
  if(act.act==AI_OutputSvm        && outputPipe->outputSvm(*this,svm))
    return true;
  if(act.act==AI_OutputSvmOverlay && outputPipe->outputOv(*this,svm))
    return true;
  return false;
  }

void Npc::setDirection(const Tempest::Vec3& pos) {
  float a = angleDir(pos.x, pos.z);
  setDirection(a);
  }

void Npc::setDirection(float rotation) {
  durtyTranform |= TR_Rot;
  angle = rotation;
  physic.setRotation(angle);
  }

void Npc::setDirectionY(float rotation) {
  if(rotation>90)
    rotation = 90;
  if(rotation<-90)
    rotation = -90;
  rotation = std::fmod(rotation,360.f);
  if(!mvAlgo.isDive() && !(interactive()!=nullptr && interactive()->isLadder()))
    return;
  angleY = rotation;
  durtyTranform |= TR_Rot;
  }

void Npc::setRunAngle(float angle) {
  durtyTranform |= TR_Rot;
  runAng = angle;
  }

float Npc::angleDir(float x, float z) {
  float a = 0;
  if(x!=0.f || z!=0.f)
    a = 180.f*std::atan2(z,x)/float(M_PI);
  return a;
  }

bool Npc::resetPositionToTA() {
  const bool g2       = owner.version().game==2;
  const bool isDragon = (g2 && guild()==GIL_DRAGON);
  const bool isDead   = this->isDead();

  if(isDead && !invent.hasMissionItems() && !isDragon)
    return false;

  invent.clearSlot(*this,"",currentInteract!=nullptr);
  if(!isPlayer())
    setInteraction(nullptr,true);

  // return monsters to their way-points
  // if(routines.empty() && !isPlayer())
  //   return currentTaPoint()!=nullptr;

  attachToPoint(nullptr);
  clearAiQueue();

  if(!isDead) {
    visual.stopAnim(*this,"");
    clearState(true);
    }

  if(isPlayer())
    return true;

  auto at = currentTaPoint();
  if(at==nullptr)
    return false;

  if(at->isLocked() && !isDead) {
    auto p = owner.findNextPoint(*at);
    if(p!=nullptr)
      at = p;
    }
  setPosition (at->position() );
  setDirection(at->direction());
  owner.script().fixNpcPosition(*this,0,0);

  if(!isDead) {
    attachToPoint(at);
    invent.autoEquipWeapons(*this);
    }

  owner.script().invokeRefreshAtInsert(*this);
  return true;
  }

void Npc::stopDlgAnim() {
  visual.stopDlgAnim(*this);
  }

void Npc::clearSpeed() {
  mvAlgo.clearSpeed();
  }

void Npc::setProcessPolicy(NpcProcessPolicy t) {
  if(aiPolicy==t)
    return;
  if(aiPolicy==NpcProcessPolicy::Player)
    runAng = 0;
  aiPolicy=t;
  }

void Npc::setWalkMode(WalkBit m) {
  wlkMode = m;
  }

bool Npc::isPlayer() const {
  return aiPolicy==NpcProcessPolicy::Player;
  }

bool Npc::startClimb(JumpStatus jump) {
  setPosition(physic.position());
  visual.setAnimRotate(*this,0);
  return mvAlgo.startClimb(jump);
  }

bool Npc::checkHealth(bool onChange, bool allowUnconscious) {
  if(isDead()) {
    return false;
    }
  if(isUnconscious() && allowUnconscious) {
    return false;
    }

  const int minHp = isMonster() ? 0 : 1;
  if(hnpc->attribute[ATR_HITPOINTS]<=minHp) {
    if(currentOther==nullptr || !allowUnconscious || !isHuman() ||
       owner.script().personAttitude(*this,*currentOther)==ATT_HOSTILE){
      if(hnpc->attribute[ATR_HITPOINTS]<=0)
        onNoHealth(true,HS_Dead);
      return false;
      }

    if(onChange) {
      onNoHealth(false,HS_Dead);
      return false;
      }
    }
  physic.setEnable(true);
  return true;
  }

void Npc::onNoHealth(bool death, HitSound sndMask) {
  invent.switchActiveWeapon(*this,Item::NSLOT);
  visual.dropWeapon(*this);
  visual.dropShield(*this);
  dropTorch();
  visual.setToFightMode(WeaponState::NoWeapon);
  updateWeaponSkeleton();

  setOther(lastHit);
  clearAiQueue();
  attachToPoint(nullptr);

  const char* svm   = death ? "SVM_%d_DEAD" : "SVM_%d_AARGH";
  const char* state = death ? "ZS_Dead"     : "ZS_Unconscious";

  if(!death)
    hnpc->attribute[ATR_HITPOINTS]=1;

  size_t fdead=owner.script().findSymbolIndex(state);
  startState(fdead,"",gtime::endOfTime(),true);
  // Note: clear perceptions for William in Jarkentar
  for(size_t i=0;i<PERC_Count;++i)
    setPerceptionDisable(PercType(i));
  if(hnpc->voice>0 && sndMask!=HS_NoSound && !isDive()) {
    emitSoundSVM(svm);
    }

  setInteraction(nullptr,true);
  invent.clearSlot(*this,"",false);

  // NOTE: in original-game oCNpc::DropUnconscious @0x00735eb0 sets the same lying body-state as the
  // dead path and never re-flags dynamic collision; a downed NPC (dead OR unconscious) is a
  // walk-through corpse for NPC-vs-NPC movement. onNoHealth is only entered when the NPC goes down,
  // so drop the soft npc capsule in both cases (checkHealth re-enables setEnable(true) only once the
  // NPC is alive and no longer unconscious). OpenGothic dropped it on death only, so a fist-fight-KO'd
  // body kept a standing-height capsule and blocked/deflected other NPCs.
  physic.setEnable(false);

  if(death)
    setAnim(lastHitType=='A' ? Anim::DeadA        : Anim::DeadB); else
    setAnim(lastHitType=='A' ? Anim::UnconsciousA : Anim::UnconsciousB);
  }

bool Npc::hasAutoroll() const {
  auto gl = std::min<uint32_t>(guild(),GIL_MAX);
  return owner.script().guildVal().disable_autoroll[gl]==0;
  }

void Npc::stopWalkAnimation() {
  if(interactive()==nullptr)
    visual.stopWalkAnim(*this);
  setAnimRotate(0);
  }

World& Npc::world() {
  return owner;
  }

Vec3 Npc::position() const {
  return {x,y,z};
  }

Matrix4x4 Npc::transform() const {
  return visual.transform();
  }

Vec3 Npc::cameraBone(bool isFirstPerson) const {
  const size_t head = visual.pose().findNode("BIP01 HEAD");

  Vec3 r = {};
  if(isFirstPerson && head!=size_t(-1)) {
    r = visual.mapBone(head);
    } else {
    auto mt = visual.pose().rootBone();
    mt.project(r);
    }

  return r;
  }

Matrix4x4 Npc::cameraMatrix(bool isFirstPerson) const {
  const size_t head = visual.pose().findNode("BIP01 HEAD");
  if(isFirstPerson && head!=size_t(-1)) {
    return visual.pose().bone(head);
    }
  return visual.pose().rootBone();
  }

float Npc::rotation() const {
  return angle;
  }

float Npc::rotationRad() const {
  return angle*float(M_PI)/180.f;
  }

float Npc::rotationY() const {
  return angleY;
  }

float Npc::rotationYRad() const {
  return angleY*float(M_PI)/180.f;
  }

Bounds Npc::bounds() const {
  return visual.bounds();
  }

auto Npc::bBoxCol() const -> const Vec3* {
  if(visual.visualSkeleton()==nullptr)
    return nullptr;
  return visual.visualSkeleton()->bboxCol;
  }

auto Npc::bBox() const -> const Vec3* {
  if(visual.visualSkeleton()==nullptr)
    return nullptr;
  return visual.visualSkeleton()->bbox;
  }

Vec3 Npc::centerPosition() const {
  auto p = position();
  // p.y += 15; // seem to be off by ~15 centimeters, according to comparations vanilla testing
  p.y += visual.pose().translateY();
  return p;
  }

Vec3 Npc::collosionCenter() const {
  auto p = position();
  p += physic.centerAsym();
  return p;
  }

Npc* Npc::lookAtTarget() const {
  return currentLookAtNpc;
  }

std::string_view Npc::portalName() {
  return mvAlgo.portalName();
  }

std::string_view Npc::formerPortalName() {
  return mvAlgo.formerPortalName();
  }

float Npc::qDistTo(const Vec3 pos) const {
  auto dp = pos - centerPosition();
  return dp.quadLength();
  }

float Npc::qDistTo(const WayPoint *f) const {
  if(f==nullptr)
    return 0.f;
  return qDistTo(f->position());
  }

float Npc::qDistTo(const Npc &p) const {
  return qDistTo(p.centerPosition());
  }

float Npc::qDistTo(const Interactive &p) const {
  auto pos = p.nearestPoint(*this);
  return qDistTo(pos);
  }

float Npc::qDistTo(const Item& p) const {
  auto pos = p.midPosition();
  return qDistTo(pos);
  }

Tempest::Vec3 Npc::fightDistanceTo(const Npc& tg) const {
  //NOTE: game script decribe comabt distance as distance between BIP01
  // however, in practice, it's easier and more relieble to use rootTr
  Vec3 cen, tgCen;
  if(auto sk = visual.visualSkeleton()) {
    cen = sk->rootTr;
    transform().project(cen);
    }
  cen += position();

  if(auto sk = tg.visual.visualSkeleton()) {
    tgCen = sk->rootTr;
    tg.transform().project(tgCen);   // NOTE: target's root-bone must use the target's
                                     // transform, not the attacker's (#fight-distance bug)
    }
  tgCen += tg.position();
  return (cen-tgCen);
  }

uint8_t Npc::calcAniComb() const {
  if(currentTarget==nullptr)
    return 0;
  auto dpos = currentTarget->position() - position();
  return Pose::calcAniComb(dpos,angle);
  }

std::string_view Npc::displayName() const {
  return hnpc->name[0];
  }

Tempest::Vec3 Npc::displayPosition() const {
  auto p = visual.displayPosition();
  return p+position();
  }

void Npc::setVisual(std::string_view visual) {
  auto skelet = Resources::loadSkeleton(visual);
  setVisual(skelet);
  setPhysic(owner.physic()->ghostObj(skelet));
  }

bool Npc::hasOverlay(std::string_view sk) const {
  auto skelet = Resources::loadSkeleton(sk);
  return hasOverlay(skelet);
  }

bool Npc::hasOverlay(const Skeleton* sk) const {
  return visual.hasOverlay(sk);
  }

void Npc::addOverlay(std::string_view sk, uint64_t time) {
  auto skelet = Resources::loadSkeleton(sk);
  addOverlay(skelet,time);
  }

void Npc::addOverlay(const Skeleton* sk, uint64_t time) {
  if(time!=0)
    time+=owner.tickCount();
  visual.addOverlay(sk,time);
  }

void Npc::delOverlay(std::string_view sk) {
  visual.delOverlay(sk);
  }

void Npc::delOverlay(const Skeleton *sk) {
  visual.delOverlay(sk);
  }

bool Npc::toggleTorch() {
  string_frm overlay(visual.visualSkeletonScheme(), humansTorchOverlay);
  if(isUsingTorch()) {
    visual.setTorch(false,owner);
    delOverlay(overlay);
    return false;
    }
  visual.setTorch(true,owner);
  addOverlay(overlay,0);
  return true;
  }

void Npc::setTorch(bool use) {
  if(isUsingTorch()==use)
    return;

  string_frm overlay(visual.visualSkeletonScheme(), humansTorchOverlay);
  visual.setTorch(use,owner);
  if(use) {
    addOverlay(overlay,0);
    } else {
    delOverlay(overlay);
    }
  }

bool Npc::isUsingTorch() const {
  return visual.isUsingTorch();
  }

void Npc::dropTorch(bool burnout) {
  auto sk = visual.visualSkeleton();
  if(sk==nullptr)
    return;

  if(!isUsingTorch())
    return;

  string_frm overlay(visual.visualSkeletonScheme(), humansTorchOverlay);
  visual.setTorch(false,owner);
  delOverlay(overlay);

  size_t torchId = 0;
  if(burnout)
    torchId = owner.script().findSymbolIndex("ItLsTorchburned"); else
    torchId = owner.script().findSymbolIndex("ItLsTorchburning");

  size_t leftHand = sk->findNode("ZS_LEFTHAND");
  if(torchId!=size_t(-1) && leftHand!=size_t(-1)) {

    auto mat = visual.transform();
    if(leftHand<visual.pose().boneCount())
      mat = visual.pose().bone(leftHand);

    owner.addItemDyn(torchId,mat,hnpc->symbol_index());
    }
  }

Tempest::Vec3 Npc::animMoveSpeed(uint64_t dt) const {
  // NOTE: in original-game zCModel::GetTrafoNodeToModel @0x0057a9c0 post-multiplies the node-to-model
  // trafo by Alg_Scaling3D(model_scale) when the scaled flag (zCModel::SetModelScale @0x0057dc30) is
  // active, so anim-derived root motion is scaled by model_scale (in model-local space, before the
  // model-to-world rotation). OpenGothic applied sz only to the render matrix, so a scaled NPC played
  // its walk/run cycle stretched but translated at base speed. Scale the delta before applyRotation
  // (the consumer rotates after this call); a no-op for the default sz=={1,1,1}.
  auto dp = visual.pose().animMoveSpeed(owner.tickCount(),dt);
  dp.x *= sz[0];
  dp.y *= sz[1];
  dp.z *= sz[2];
  return dp;
  }

void Npc::setVisual(const Skeleton* v) {
  visual.setVisual(v);
  invalidateTalentOverlays();
  }

void Npc::setVisualBody(int32_t headTexNr, int32_t teethTexNr, int32_t bodyTexNr, int32_t bodyTexColor,
                        std::string_view ibody, std::string_view ihead) {
  body    = ibody;
  head    = ihead;
  vHead   = headTexNr;
  vTeeth  = teethTexNr;
  vColor  = bodyTexNr;
  bdColor = bodyTexColor;

  auto  vhead = head.empty() ? MeshObjects::Mesh() : owner.addView(FileExt::addExt(head,".MMB"),vHead,vTeeth,bdColor);
  auto  vbody = body.empty() ? MeshObjects::Mesh() : owner.addView(FileExt::addExt(body,".ASC"),vColor,0,bdColor);
  visual.setVisualBody(*this,std::move(vhead),std::move(vbody),bdColor);
  updateArmor();

  durtyTranform|=TR_Pos; // update obj matrix
  }

void Npc::updateArmor() {
  auto  ar = invent.currentArmor();
  auto& w  = owner;

  if(ar==nullptr) {
    auto  vbody = body.empty() ? MeshObjects::Mesh() : w.addView(FileExt::addExt(body,".ASC"),vColor,0,bdColor);
    visual.setBody(*this,std::move(vbody),bdColor);
    } else {
    auto& itData = ar->handle();
    auto  flag   = ItmFlags(itData.main_flag);
    if(flag & ITM_CAT_ARMOR){
      auto& asc   = itData.visual_change;
      auto  vbody = asc.empty() ? MeshObjects::Mesh() : w.addView(asc,vColor,0,bdColor);
      visual.setArmor(*this,std::move(vbody));
      }
    }
  }

void Npc::setSword(MeshObjects::Mesh&& s) {
  visual.setSword(std::move(s));
  updateWeaponSkeleton();
  }

void Npc::setRangedWeapon(MeshObjects::Mesh&& b) {
  visual.setRangedWeapon(std::move(b));
  updateWeaponSkeleton();
  }

void Npc::setShield(MeshObjects::Mesh&& s) {
  visual.setShield(std::move(s));
  updateWeaponSkeleton();
  }

void Npc::setMagicWeapon(Effect&& s) {
  s.setOrigin(this);
  visual.setMagicWeapon(std::move(s),owner);
  updateWeaponSkeleton();
  }

void Npc::setSlotItem(MeshObjects::Mesh&& itm, std::string_view slot) {
  visual.setSlotItem(std::move(itm),slot);
  }

void Npc::setStateItem(MeshObjects::Mesh&& itm, std::string_view slot) {
  visual.setStateItem(std::move(itm),slot);
  }

void Npc::setAmmoItem(MeshObjects::Mesh&& itm, std::string_view slot) {
  visual.setAmmoItem(std::move(itm),slot);
  }

void Npc::clearSlotItem(std::string_view slot) {
  visual.clearSlotItem(slot);
  }

void Npc::updateWeaponSkeleton() {
  visual.updateWeaponSkeleton(invent.currentMeleeWeapon(),invent.currentRangedWeapon());
  }

void Npc::setPhysic(DynamicWorld::NpcItem &&item) {
  physic = std::move(item);
  physic.setUserPointer(this);
  physic.setPosition(Vec3{x,y,z});
  physic.setRotation(angle);
  }

void Npc::setFatness(float f) {
  bdFatness = f;
  visual.setFatness(f);
  }

void Npc::setScale(float x, float y, float z) {
  sz[0]=x;
  sz[1]=y;
  sz[2]=z;
  durtyTranform |= TR_Scale;
  physic.setScale(Vec3{x,y,z});
  }

const Animation::Sequence* Npc::playAnimByName(std::string_view name, BodyState bs) {
  return visual.startAnimAndGet(*this,name,calcAniComb(),bs);
  }

bool Npc::setAnim(Npc::Anim a) {
  return setAnimAngGet(a)!=nullptr;
  }

const Animation::Sequence* Npc::setAnimAngGet(Anim a) {
  return setAnimAngGet(a,calcAniComb());
  }

const Animation::Sequence* Npc::setAnimAngGet(Anim a, uint8_t comb) {
  auto st  = weaponState();
  auto wlk = walkMode();
  if(mvAlgo.isDive())
    wlk = WalkBit::WM_Dive;
  else if(mvAlgo.isSwim())
    wlk = WalkBit::WM_Swim;
  else if(mvAlgo.isInWater())
    wlk = WalkBit::WM_Water;
  return visual.startAnimAndGet(*this,a,comb,st,wlk);
  }

void Npc::setAnimRotate(int rot) {
  visual.setAnimRotate(*this,rot);
  }

bool Npc::setAnimItem(std::string_view scheme, int state) {
  if(scheme.empty())
    return true;
  if(bodyStateMasked()!=BS_STAND) {
    setAnim(Anim::Idle);
    return false;
    }
  if(auto sq = visual.startAnimItem(*this,scheme,state)) {
    implAniWait(uint64_t(sq->totalTime()));
    return true;
    }
  return false;
  }

void Npc::stopAnim(std::string_view ani) {
  visual.stopAnim(*this,ani);
  }

void Npc::startFaceAnim(std::string_view anim, float intensity, uint64_t duration) {
  visual.startFaceAnim(*this,anim,intensity,duration);
  }

bool Npc::stopItemStateAnim() {
  return visual.stopItemStateAnim(*this);
  }

bool Npc::hasAnim(std::string_view scheme) const {
  return visual.hasAnim(scheme);
  }

bool Npc::hasAnim(Anim a) const {
  auto st  = weaponState();
  auto wlk = walkMode();
  if(mvAlgo.isDive())
    wlk = WalkBit::WM_Dive;
  else if(mvAlgo.isSwim())
    wlk = WalkBit::WM_Swim;
  else if(mvAlgo.isInWater())
    wlk = WalkBit::WM_Water;
  return visual.hasAnim(a,st,wlk);
  }

bool Npc::hasSwimAnimations() const {
  return hasAnim("S_SWIM") && hasAnim("S_SWIMF");
  }

bool Npc::isFinishingMove() const {
  if(weaponState()==WeaponState::NoWeapon)
    return false;
  return visual.pose().isInAnim("T_1HSFINISH") || visual.pose().isInAnim("T_2HSFINISH");
  }

bool Npc::isStanding() const {
  return visual.isStanding();
  }

bool Npc::isSwim() const {
  return mvAlgo.isSwim();
  }

bool Npc::isInWater() const {
  return mvAlgo.isInWater();
  }

bool Npc::isDive() const {
  return mvAlgo.isDive();
  }

bool Npc::isCasting() const {
  return castLevel!=CS_NoCast;
  }

bool Npc::isJumpAnim() const {
  return visual.pose().isJumpAnim();
  }

bool Npc::isFlyAnim() const {
  return visual.pose().isFlyAnim();
  }

bool Npc::isFalling() const {
  return mvAlgo.state()==MoveAlgo::Falling;
  }

bool Npc::isFallingDeep() const {
  return (mvAlgo.isInAir() || mvAlgo.isFalling()) && (visual.pose().isInAnim("S_FALL") || visual.pose().isInAnim("S_FALLB"));
  }

bool Npc::isSlide() const {
  return mvAlgo.state()==MoveAlgo::Slide;
  }

bool Npc::isInAir() const {
  return mvAlgo.state()==MoveAlgo::InAir;
  }

bool Npc::isJump() const {
  return mvAlgo.state()==MoveAlgo::Jump;
  }

bool Npc::isJumpUp() const {
  return mvAlgo.state()==MoveAlgo::JumpUp;
  }

void Npc::invalidateTalentOverlays() {
  const Talent tl[] = {TALENT_1H, TALENT_2H, TALENT_BOW, TALENT_CROSSBOW, TALENT_ACROBAT};
  for(Talent i:tl) {
    invalidateTalentOverlays(i);
    }
  }

void Npc::invalidateTalentOverlays(Talent t) {
  const auto scheme = visual.visualSkeletonScheme();
  if(scheme.empty())
    return;

  const auto lvl = talentsSk[t];
  if(t==TALENT_1H){
    if(lvl==0){
      delOverlay(string_frm(scheme,"_1HST1.MDS"));
      delOverlay(string_frm(scheme,"_1HST2.MDS"));
      }
    else if(lvl==1){
      addOverlay(string_frm(scheme,"_1HST1.MDS"),0);
      delOverlay(string_frm(scheme,"_1HST2.MDS"));
      }
    else if(lvl==2){
      delOverlay(string_frm(scheme,"_1HST1.MDS"));
      addOverlay(string_frm(scheme,"_1HST2.MDS"),0);
      }
    }
  else if(t==TALENT_2H){
    if(lvl==0){
      delOverlay(string_frm(scheme,"_2HST1.MDS"));
      delOverlay(string_frm(scheme,"_2HST2.MDS"));
      }
    else if(lvl==1){
      addOverlay(string_frm(scheme,"_2HST1.MDS"),0);
      delOverlay(string_frm(scheme,"_2HST2.MDS"));
      }
    else if(lvl==2){
      delOverlay(string_frm(scheme,"_2HST1.MDS"));
      addOverlay(string_frm(scheme,"_2HST2.MDS"),0);
      }
    }
  else if(t==TALENT_BOW){
    if(lvl==0){
      delOverlay(string_frm(scheme,"_BOWT1.MDS"));
      delOverlay(string_frm(scheme,"_BOWT2.MDS"));
      }
    else if(lvl==1){
      addOverlay(string_frm(scheme,"_BOWT1.MDS"),0);
      delOverlay(string_frm(scheme,"_BOWT2.MDS"));
      }
    else if(lvl==2){
      delOverlay(string_frm(scheme,"_BOWT1.MDS"));
      addOverlay(string_frm(scheme,"_BOWT2.MDS"),0);
      }
    }
  else if(t==TALENT_CROSSBOW){
    if(lvl==0){
      delOverlay(string_frm(scheme,"_CBOWT1.MDS"));
      delOverlay(string_frm(scheme,"_CBOWT2.MDS"));
      }
    else if(lvl==1){
      addOverlay(string_frm(scheme,"_CBOWT1.MDS"),0);
      delOverlay(string_frm(scheme,"_CBOWT2.MDS"));
      }
    else if(lvl==2){
      delOverlay(string_frm(scheme,"_CBOWT1.MDS"));
      addOverlay(string_frm(scheme,"_CBOWT2.MDS"),0);
      }
    }
  else if(t==TALENT_ACROBAT){
    if(lvl==0)
      delOverlay(string_frm(scheme,"_ACROBATIC.MDS")); else
      addOverlay(string_frm(scheme,"_ACROBATIC.MDS"),0);
    }
  }

void Npc::setTalentSkill(Talent t, int32_t lvl) {
  if(t>=TALENT_MAX_G2)
    return;
  talentsSk[t] = lvl;
  invalidateTalentOverlays(t);
  }

int32_t Npc::talentSkill(Talent t) const {
  if(t<TALENT_MAX_G2)
    return talentsSk[t];
  return 0;
  }

void Npc::setTalentValue(Talent t, int32_t lvl) {
  if(t<TALENT_MAX_G2)
    talentsVl[t] = lvl;
  }

int32_t Npc::talentValue(Talent t) const {
  if(t<TALENT_MAX_G2)
    return talentsVl[t];
  return 0;
  }

int32_t Npc::hitChance(Talent t) const {
  // NOTE: the hit-chance array has exactly hitchance_count (5) slots, indexed only by combat
  // talents (1H/2H/bow/crossbow); the bound must be strict. `<=` read one past the array end
  // for t==hitchance_count (TALENT_PICKLOCK=5), an out-of-bounds read reachable from the stats
  // menu that printed a garbage "%" for the picklock row.
  if(t<zenkit::INpc::hitchance_count)
    return hnpc->hitchance[t];
  return 0;
  }

bool Npc::isRefuseTalk() const {
  return refuseTalkMilis>=owner.tickCount();
  }

int32_t Npc::mageCycle() const {
  return talentSkill(TALENT_MAGE);
  }

bool Npc::canSneak() const {
  return talentSkill(TALENT_SNEAK)!=0;
  }

void Npc::setRefuseTalk(uint64_t milis) {
  refuseTalkMilis = owner.tickCount()+milis;
  }

int32_t Npc::attribute(Attribute a) const {
  if(a<ATR_MAX)
    return hnpc->attribute[a];
  return 0;
  }

void Npc::changeAttribute(Attribute a, int32_t val, bool allowUnconscious) {
  if(a>=ATR_MAX || val==0)
    return;

  // NOTE: in original-game oCNpc::ChangeAttribute (Gothic2.exe 0x0072ff60) the godmode guard has
  // no attribute-index test: for the godmode player EVERY negative delta is rejected (HP, MANA,
  // STRENGTH, ...), not just HITPOINTS. (The cutscene clause is OpenGothic-only, kept HP-scoped.)
  if(val<0 && isPlayer() && Gothic::inst().isGodMode())
    return;
  if(val<0 && a==ATR_HITPOINTS && isPlayer() && owner.currentCs()!=nullptr)
    return;

  // NOTE: in original-game oCNpc::ChangeAttribute (Gothic2.exe 0x0072ff60) the IMMORTAL flag
  // (oCNpc+0x1b4 bit1) blocks EVERY HITPOINTS change regardless of sign -- damage AND
  // heals/regeneration -- except the val==-999 kill sentinel, which scripts/cutscenes use to
  // force-kill an immortal NPC. OpenGothic only checked the flag on val<0, so positive heals
  // and natural regen leaked through and raised an immortal NPC's HP.
  if(a==ATR_HITPOINTS && isImmortal() && val!=-999)
    return;

  hnpc->attribute[a]+=val;
  if(hnpc->attribute[a]<0)
    hnpc->attribute[a]=0;
  if(a==ATR_HITPOINTS && hnpc->attribute[a]>hnpc->attribute[ATR_HITPOINTSMAX])
    hnpc->attribute[a] = hnpc->attribute[ATR_HITPOINTSMAX];
  if(a==ATR_MANA && hnpc->attribute[a]>hnpc->attribute[ATR_MANAMAX])
    hnpc->attribute[a] = hnpc->attribute[ATR_MANAMAX];

  if(val<0)
    invent.invalidateCond(*this);

  if(a==ATR_HITPOINTS) {
    checkHealth(true,allowUnconscious);
    if(aiPolicy==NpcProcessPolicy::AiFar || aiPolicy==NpcProcessPolicy::AiFar2)
      aiState.started = true;
    }
  }

int32_t Npc::protection(Protection p) const {
  if(p<PROT_MAX)
    return hnpc->protection[p];
  return 0;
  }

void Npc::changeProtection(Protection p, int32_t val) {
  if(p<PROT_MAX)
    hnpc->protection[p]=val;
  }

uint32_t Npc::instanceSymbol() const {
  return uint32_t(hnpc->symbol_index());
  }

uint32_t Npc::guild() const {
  return std::min(uint32_t(hnpc->guild), uint32_t(GIL_MAX-1));
  }

bool Npc::isMonster() const {
  const bool g2 = owner.version().game==2;
  const auto SEPERATOR_ORC = g2 ? GIL_SEPERATOR_ORC : GIL_G1_SEPERATOR_ORC;
  const auto SEPERATOR_HUM = g2 ? GIL_SEPERATOR_HUM : GIL_G1_SEPERATOR_HUM;
  // NOTE: in original-game oCNpc::IsMonster/IsHuman (Gothic2.exe 0x742600/0x742640) classify on
  // the TRUE guild (field 0x766), not the live script-mutable C_Npc.guild; OG read the live
  // guild, so a runtime guild change (disguise) wrongly flipped monster/human status. (#656)
  if(!(SEPERATOR_HUM<trueGuild() && trueGuild()<SEPERATOR_ORC))
    return false;
  // NOTE: in original-game oCNpc::IsMonster (Gothic2.exe 0x00742600) additionally excludes
  // GIL_FIREGOLEM(0x28), GIL_ICEGOLEM(0x29) and GIL_DRAGON(0x2f) from monster classification:
  // these talk/boss-capable creatures are NOT monsters, so they must not get the monster unarmed
  // auto-crit (damagecalculator) nor the monster minHp=0 death path. (The original's missing orc
  // upper bound is a separate, broader divergence, left deferred.)
  if(g2) {
    const auto tg = trueGuild();
    if(tg==GIL_FIREGOLEM || tg==GIL_ICEGOLEM || tg==GIL_DRAGON)
      return false;
    }
  return true;
  }

bool Npc::isHuman() const {
  const bool g2 = owner.version().game==2;
  const auto SEPERATOR_HUM = g2 ? GIL_SEPERATOR_HUM : GIL_G1_SEPERATOR_HUM;
  return trueGuild() < SEPERATOR_HUM; // #656: classify on true guild, not live C_Npc.guild
  }

void Npc::setTrueGuild(int32_t g) {
  trGuild = g;
  }

int32_t Npc::trueGuild() const {
  if(trGuild==GIL_NONE)
    return hnpc->guild;
  return trGuild;
  }

int32_t Npc::magicCyrcle() const {
  return talentSkill(TALENT_RUNES);
  }

int32_t Npc::level() const {
  return hnpc->level;
  }

int32_t Npc::experience() const {
  return hnpc->exp;
  }

int32_t Npc::experienceNext() const {
  return hnpc->exp_next;
  }

int32_t Npc::learningPoints() const {
  return hnpc->lp;
  }

int32_t Npc::diveTime() const {
  return mvAlgo.diveTime();
  }

void Npc::setAttitude(Attitude att) {
  permAttitude = att;
  }

bool Npc::isFriend() const {
  bool g2 = owner.version().game==2;
  return ( g2 && hnpc->type==zenkit::NpcType::G2_FRIEND) ||
         (!g2 && hnpc->type==zenkit::NpcType::G1_FRIEND);
  }

void Npc::setTempAttitude(Attitude att) {
  tmpAttitude = att;
  }

bool Npc::implPointAt(const Tempest::Vec3& to) {
  auto    dpos = to-position();
  uint8_t comb = Pose::calcAniComb(dpos,angle);

  return (setAnimAngGet(Npc::Anim::PointAt,comb)!=nullptr);
  }

bool Npc::implLookAtWp(uint64_t dt) {
  if(currentLookAt==nullptr)
    return false;
  auto dvec = currentLookAt->position();
  return implLookAt(dvec.x,dvec.y,dvec.z,dt);
  }

bool Npc::implLookAtNpc(uint64_t dt) {
  if(currentLookAtNpc==nullptr)
    return false;
  auto selfHead  = visual.mapHeadBone();
  auto otherHead = currentLookAtNpc->visual.mapHeadBone();
  auto dvec = otherHead - selfHead;
  return implLookAt(dvec.x,dvec.y,dvec.z,dt);
  }

bool Npc::implLookAt(float dx, float dy, float dz, uint64_t dt) {
  static const float rotSpeed = 200; // deg per second
  static const float maxRot   = 80; // maximum rotation
  Vec2 dst;

  dst.x = visual.viewDirection()-angleDir(dx,dz);
  while(dst.x>180)
    dst.x -= 360;
  while(dst.x<-180)
    dst.x += 360;

  dst.y = std::atan2(dy,std::sqrt(dx*dx+dz*dz));
  dst.y = dst.y*180.f/float(M_PI);

  if(dst.x<-maxRot || dst.x>maxRot) {
    dst.x = 0;
    dst.y = 0;
    }

  if(dst.y<-20)
    dst.y = -20;
  if(dst.y>20)
    dst.y = 20;

  auto rot  = visual.headRotation();
  auto drot = dst-rot;

  drot.x = std::min(std::abs(drot.x),rotSpeed*float(dt)/1000.f);
  drot.y = std::min(std::abs(drot.y),rotSpeed*float(dt)/1000.f);
  if(dst.x<rot.x)
    drot.x = -drot.x;
  if(dst.y<rot.y)
    drot.y = -drot.y;

  rot+=drot;
  visual.setHeadRotation(rot.x,rot.y);

  return false;
  }

bool Npc::implTurnAway(const Npc &oth, uint64_t dt) {
  if(&oth==this)
    return true;

  // turn npc's back to oth, so calculate direction from oth to npc
  auto dx = x-oth.x;
  auto dz = z-oth.z;
  auto  gl   = guild();
  float step = float(owner.script().guildVal().turn_speed[gl]);
  return rotateTo(dx,dz,step,AnimationSolver::TurnType::Std,dt);
  }

bool Npc::implTurnToFai(const Npc& oth, uint64_t dt) {
  if(&oth==this || oth.isDown())
    return false;

  auto ws = weaponState();
  if(ws==WeaponState::NoWeapon)
    return false;

  auto  gl   = guild();
  auto& gv   = owner.script().guildVal();
  float step = float(gv.turn_speed[gl]);
  //auto  dpos = fghAlgo.distVec(*currentTarget, *this);
  auto dpos = currentTarget->collosionCenter() - collosionCenter();

  // vanilla has a bug(or quirk) apparently, for that
  // also would need to fallthru in FAI code, if no animation is performed
  bool skipAnim = gv.turn_speed[gl] >= 100;
  auto anim = skipAnim ? AnimationSolver::TurnType::None : AnimationSolver::TurnType::Std;
  if(ws==WeaponState::Bow || ws==WeaponState::CBow || ws==WeaponState::Mage) {
    anim = AnimationSolver::TurnType::None;
    }

  auto bs = bodyStateMasked();
  if(bs!=BS_HIT) {
    //NOTE: Troll rotates during the hit, but not very fast - seem to be regulat speed
    step *= 2.f; // faster in combat
    }
  return rotateTo(dpos.x,dpos.z,step,anim,dt) && !skipAnim;
  }

bool Npc::implTurnTo(const Npc &oth, uint64_t dt) {
  if(&oth==this)
    return false;
  auto dx = oth.x-x;
  auto dz = oth.z-z;
  return implTurnTo(dx,dz,AnimationSolver::TurnType::Std,dt);
  }

bool Npc::implTurnTo(const Npc& oth, AnimationSolver::TurnType anim, uint64_t dt) {
  if(&oth==this)
    return false;
  auto dx = oth.x-x;
  auto dz = oth.z-z;
  return implTurnTo(dx,dz,anim,dt);
  }

bool Npc::implTurnTo(const WayPoint* wp, AnimationSolver::TurnType anim, uint64_t dt) {
  if(wp==nullptr)
    return false;
  return implTurnTo(wp->dir.x,wp->dir.z,anim,dt);
  }

bool Npc::implTurnTo(float dx, float dz, AnimationSolver::TurnType anim, uint64_t dt) {
  auto  gl   = guild();
  float step = float(owner.script().guildVal().turn_speed[gl]);
  return rotateTo(dx,dz,step,anim,dt);
  }

bool Npc::implWhirlTo(const Npc &oth, uint64_t dt) {
  return implTurnTo(oth,AnimationSolver::TurnType::Whirl,dt);
  }

bool Npc::implGoTo(uint64_t dt) {
  float dist = 0;
  if(go2.npc) {
    // NOTE: in original-game oCNpc::EV_GotoVob @0x00685580 / RobustTrace @0x00686960 the AI_GotoNpc
    // (oCMsgMovement sub-type 2) follow arrival radius is the fixed engine constant 200 units
    // (reached when dist^2 < 40000.0); it does NOT use fight/weapon range. Enemy approach uses
    // GT_Enemy + isInWRange (GoTo::isClose short-circuits), so go2.npc here is the AI_GotoNpc follow
    // path only -- using prefferedAttackDistance made companions stop at a weapon/guild-dependent
    // distance (too far with a 2H weapon) instead of the canonical fixed 200.
    dist = 200.f;
    } else {
    // use smaller threshold, to avoid edge-looping in script
    dist = MoveAlgo::closeToPointThreshold*0.5f;
    if(!mvAlgo.checkLastBounce())
      dist = MoveAlgo::closeToPointThreshold*1.5f;
    if(go2.wp!=nullptr && go2.wp->useCounter()>1)
      dist = float(MAX_AI_USE_DISTANCE);
    }
  return implGoTo(dt,dist);
  }

bool Npc::implGoTo(uint64_t dt, float destDist) {
  if(go2.flag==GT_No)
    return false;

  if(isInAir() || interactive()!=nullptr) {
    mvAlgo.tick(dt);
    return true;
    }

  auto target = go2.target();
  auto dpos   = target - position();

  if(go2.flag==GT_Flee) {
    // nop
    }
  else if(go2.isClose(*this, destDist)) {
    bool finished = true;
    if(go2.flag==GT_Way) {
      go2.wp = go2.wp->hasLadderConn(wayPath.first()) ? wayPath.first() : wayPath.pop();
      if(go2.wp!=nullptr) {
        attachToPoint(go2.wp);
        if(setGoToLadder()) {
          mvAlgo.tick(dt);
          return true;
          }
        finished = false;
        }
      }
    if(finished) {
      if(go2.flag==Npc::GT_NextFp && implTurnTo(go2.wp,AnimationSolver::TurnType::Std,dt))
        return true;
      // NOTE: in original-game oCNpc::RbtMoveToExactPosition @0x686880 (oNpc_Move.cpp)
      // hard-SetPositionWorld's the NPC onto the (SearchNpcPosition-validated) target on
      // arrival rather than leaving it wherever the radius test first tripped --
      // closeToPointThreshold is only a trigger, not the final pose. Snap X/Z onto the
      // waypoint/freepoint, keep Y for the existing ground path, and revert on collision
      // (mirrors the original's geometry guard, like Interactive::setPos). #585
      if(go2.npc==nullptr) {
        auto prev = position();
        setPosition(target.x, prev.y, target.z);
        if(hasCollision())
          setPosition(prev);
        }
      clearGoTo();
      }
    }
  else {
    if(setGoToLadder()) {
      mvAlgo.tick(dt);
      return true;
      }
    if(mvAlgo.checkLastBounce() && implTurnTo(dpos.x,dpos.z,AnimationSolver::TurnType::Std,dt)) {
      mvAlgo.tick(dt);
      return true;
      }
    }

  if(!go2.empty()) {
    setAnim(AnimationSolver::Move);
    mvAlgo.tick(dt);
    return true;
    }
  return false;
  }

bool Npc::implAttack(uint64_t dt) {
  if(currentTarget==nullptr || isPlayer() || isTalk())
    return false;

  if(currentTarget->isDown()){
    // NOTE: don't clear internal target, to make scripts happy
    // currentTarget=nullptr;
    fghAlgo.onClearTarget();
    return false;
    }

  if(aiQueue.size()>0) {
    // do not messup weapon change animations by MOVE intruction
    return false;
    }

  const auto ws = weaponState();
  const auto bs = bodyStateMasked();

  if(bs==BS_HIT && (ws==WeaponState::Fist || ws==WeaponState::W1H || ws==WeaponState::W2H)) {
    //NOTE: 'storm' attack has BS_RUN state and not meant to be auto-rotated
    implTurnToFai(*currentTarget,dt);
    mvAlgo.tick(dt,MoveAlgo::FaiMove);
    return true;
    }

  if(!fghAlgo.hasInstructions())
    return false;

  if(bs==BS_LIE) {
    setAnim(Npc::Anim::Idle);
    mvAlgo.tick(dt,MoveAlgo::FaiMove);
    return true;
    }
  if(bs==BS_STUMBLE || bs==BS_FALL || isInAir()) {
    mvAlgo.tick(dt,MoveAlgo::FaiMove);
    return true;
    }

  if(faiWaitTime>=owner.tickCount() || waitTime>=owner.tickCount()) {
    implTurnToFai(*currentTarget,dt);
    mvAlgo.tick(dt,MoveAlgo::FaiMove);
    return true;
    }

  const auto act = fghAlgo.nextFromQueue(*this,*currentTarget,owner.script());

  // NOTE: in original-game, this behaviour seem to be hardcoded
  // test case: wolf jump-back quite often when close, but programmed to jump only if attacked
  // so far promoting wait to jump seem to work best
  const bool jmp = fghAlgo.isInCloseupRange(*this,*currentTarget,owner.script()) && fghAlgo.isInFocusAngle(*this,*currentTarget);

  // vanilla behavior, required for orcs in G1 orcgraveyard
  if(ws==WeaponState::NoWeapon && isAiQueueEmpty() && canSwitchWeapon()) {
    drawWeaponMelee();
    return true;
    }

  if(act==FightAlgo::MV_BLOCK) {
    if(!fghAlgo.isInFocusAngle(*this, *currentTarget)) {
      fghAlgo.consumeAction();
      return true;
      }

    switch(ws) {
      case WeaponState::Fist: {
        if(blockFist())
          fghAlgo.consumeAction();
        break;
        }
      case WeaponState::W1H:
      case WeaponState::W2H: {
        if(blockSword())
          fghAlgo.consumeAction();
        break;
        }
      default:
        fghAlgo.consumeAction();
        break;
      }
    return true;
    }

  if(act==FightAlgo::MV_ATTACK || act==FightAlgo::MV_ATTACKL || act==FightAlgo::MV_ATTACKR) {
    //NOTE: FIGHT_DIST_CANCEL in scipts is often longer, than senses_range of npc
    const auto sense = fghAlgo.isInFocusAngle(*this,*currentTarget,5.f);
    if(!sense) {
      implTurnToFai(*currentTarget,dt);
      mvAlgo.tick(dt,MoveAlgo::FaiMove);
      return true;
      }
#if 0
    fghAlgo.consumeAction(); //debug
    return true;
#endif

    static const Anim ani[4] = {Anim::Attack, Anim::AttackL, Anim::AttackR};
    if((act!=FightAlgo::MV_ATTACK && bodyState()!=BS_RUN) &&
       !fghAlgo.isInWRange(*this,*currentTarget,owner.script())) {
      fghAlgo.consumeAction();
      return true;
      }

    if(ws==WeaponState::Bow || ws==WeaponState::CBow || ws==WeaponState::Mage) {
      bool obsticle = false;
      if(currentTarget!=nullptr) {
        auto hit = owner.physic()->rayNpc(this->mapWeaponBone(),currentTarget->centerPosition(),this);
        if(hit.hasCol && hit.npcHit!=currentTarget) {
          obsticle = true;
          // if(hit.npcHit!=nullptr && owner.script().personAttitude(*this,*hit.npcHit)==ATT_HOSTILE)
          //   obsticle = false;
          if(hit.npcHit!=nullptr && hit.npcHit!=currentTarget && owner.script().isFriendlyFire(*this,*hit.npcHit))
            obsticle = false;
          }
        }
      if(auto spl = activeWeapon()) {
        if(spl->isSpell() && !spl->isSpellShoot())
          obsticle = false;
        }
      if(obsticle) {
        auto anim = (owner.script().rand(2)==0 ? Npc::Anim::MoveL : Npc::Anim::MoveR);
        if(setAnim(anim)){
          visual.setAnimRotate(*this,0);
          implFaiWait(visual.pose().animationTotalTime());
          fghAlgo.consumeAction();
          return true;
          }
        }
      }

    if(ws==WeaponState::Mage) {
      const auto cast = beginCastSpell();
      if(cast==BeginCastResult::BC_No)
        return false;
      fghAlgo.consumeAction();
      }
    else if(ws==WeaponState::Bow || ws==WeaponState::CBow) {
      if(shootBow()) {
        fghAlgo.consumeAction();
        }
      else if(!implTurnToFai(*currentTarget,dt)) {
        aimBow();
        }
      }
    else if(ws==WeaponState::Fist || ws==WeaponState::W1H || ws==WeaponState::W2H) {
      const auto hit = owner.physic()->ray(this->collosionCenter(),currentTarget->collosionCenter());
      if(hit.hasCol) {
        // blocked by wall
        fghAlgo.consumeAction();
        return true;
        }
      const auto atkType = (ws==WeaponState::Fist) ? Anim::Attack : ani[act-FightAlgo::MV_ATTACK];
      const bool atk     = doAttack(atkType, BS_HIT);

      if(atk || mvAlgo.isSwim() || mvAlgo.isDive()) {
        uint64_t aniTime = visual.pose().atkTotalTime()+1;
        implFaiWait(aniTime);
        if(bs==BS_RUN)
          implAniWait(aniTime);
        fghAlgo.consumeAction();
        } else {
        implTurnToFai(*currentTarget,dt);
        }
      }
    else {
      // Attack action without any weapon. Can happend at weapon transition(orc shaman) - skip it.
      fghAlgo.consumeAction();
      }
    return true;
    }

  if(act==FightAlgo::MV_TURN2HIT) {
    if(!implTurnTo(*currentTarget,dt))
      fghAlgo.consumeAction();
    return true;
    }

  if(act==FightAlgo::MV_STRAFEL) {
    if(setAnim(Npc::Anim::MoveL)) {
      visual.setAnimRotate(*this,0);
      implFaiWait(visual.pose().animationTotalTime());
      fghAlgo.consumeAction();
      }
    else if(!hasAnim(Npc::Anim::MoveL)) {
      // avoid soft-locks
      visual.setAnimRotate(*this,0);
      fghAlgo.consumeAction();
      }
    return true;
    }

  if(act==FightAlgo::MV_STRAFER) {
    if(setAnim(Npc::Anim::MoveR)) {
      visual.setAnimRotate(*this,0);
      implFaiWait(visual.pose().animationTotalTime());
      fghAlgo.consumeAction();
      }
    else if(!hasAnim(Npc::Anim::MoveR)) {
      // avoid soft-locks
      visual.setAnimRotate(*this,0);
      fghAlgo.consumeAction();
      }
    return true;
    }

  if(act==FightAlgo::MV_STRAFE_E) {
    // finalize strafe
    if(!setAnim(Npc::Anim::Idle))
      return false;
    fghAlgo.consumeAction();
    return true;
    }

  if(act==FightAlgo::MV_JUMPBACK || (act==FightAlgo::MV_WAIT && jmp) || (act==FightAlgo::MV_TURN && jmp)) {
    if(isSwim()) {
      fghAlgo.consumeAction();
      return true;
      }
    if(bodyStateMasked()==BS_PARADE) {
      fghAlgo.consumeAction();
      return true;
      }
    if(!fghAlgo.isInFocusAngle(*this, *currentTarget) && !jmp) {
      //NOTE: jump-back is ultimate defence, so better to use it only if npc face player directly
      fghAlgo.consumeAction();
      aiState.loopNextTime = owner.tickCount(); // force ZS_MM_Attack_Loop call
      return true;
      }
    if(setAnim(Npc::Anim::MoveBack)) {
      implFaiWait(visual.pose().animationTotalTime());
      fghAlgo.consumeAction();
      }
    return true;
    }

  if(act==FightAlgo::MV_MOVE || act==FightAlgo::MV_TURN) {
    if(currentTarget->isDown()) {
      if(setAnim(Anim::Idle))
        fghAlgo.consumeAction();
      return true;
      }

    const bool prGRange = fghAlgo.isInGRange(*this, *currentTarget, owner.script());
    const bool prWRange = fghAlgo.isInWRange(*this, *currentTarget, owner.script());
    const auto prBs     = bs;

    const float distance = qDistTo(*currentTarget);
    const float range    = float(handle().senses_range);

    if(!prGRange && distance<range*range) {
      // if npc is reasonably far, we can switch to propper pathfinding
      const auto hit = owner.physic()->ray(this->collosionCenter(), currentTarget->collosionCenter());
      if(hit.hasCol) {
        auto near = owner.findWayPoint(currentTarget->position(), [this](const WayPoint &wp) {
          if(!currentTarget->canRayHitPoint(wp.pos))
            return false;
          return true;
          });
        if(near!=nullptr) {
          if(near!=wayPath.last()) {
            wayPath = owner.wayTo(*this,*near);
            go2.set(wayPath.first(), GT_Way);
            }
          return false;
          }
        }
      }

    if(prWRange) {
      //NOTE: bloodfly and other monsters may run to close to player otherwise
      //NOTE2: also for bloodfly we have to use 'hard-stop', to avoid trailing flight
      visual.stopWalkAnim(*this);
      //setAnim(Anim::Idle);
      implTurnToFai(*currentTarget,dt);
      } else {
      if(mvAlgo.checkLastBounce()) {
        if(implTurnToFai(*currentTarget,dt))
          return true;
        }
      setAnim(AnimationSolver::Move);
      go2.set(currentTarget, GT_Enemy);
      mvAlgo.tick(dt, MoveAlgo::FaiMove);
      go2.clear();
      wayPath.clear();
      }

    const bool isGRange = fghAlgo.isInGRange(*this, *currentTarget, owner.script());
    const bool isWRange = fghAlgo.isInWRange(*this, *currentTarget, owner.script());
    const bool isFocus  = fghAlgo.isInFocusAngle(*this, *currentTarget, 5.f);

    if((isWRange || (isGRange!=prGRange) || prBs!=bodyStateMasked()) && isFocus) {
      visual.setAnimRotate(*this, 0);
      fghAlgo.consumeAction();
      aiState.loopNextTime = owner.tickCount(); // force ZS_MM_Attack_Loop call
      implAiTick(dt);
      return true;
      }

    implAiTick(dt);
    return true;
    }

  if(act==FightAlgo::MV_WAIT) {
    implFaiWait(200);
    fghAlgo.consumeAction();
    stopWalkAnimation();
    return true;
    }

  if(act==FightAlgo::MV_WAITLONG) {
    implFaiWait(300);
    fghAlgo.consumeAction();
    stopWalkAnimation();
    return true;
    }

  if(act==FightAlgo::MV_NULL) {
    fghAlgo.consumeAction();
    stopWalkAnimation();
    return true;
    }

  return true;
  }

bool Npc::implAiTick(uint64_t dt) {
  // Note AI-action queue takes priority, test case: Vatras pray at night
  if(aiQueue.size()==0) {
    tickRoutine();
    if(aiQueue.size()>0)
      nextAiAction(aiQueue,dt);
    return false;
    }
  nextAiAction(aiQueue,dt);
  return true;
  }

void Npc::implAiWait(uint64_t dt) {
  auto w = owner.tickCount()+dt;
  if(w>waitTime)
    waitTime = w;
  }

void Npc::implAniWait(uint64_t dt) {
  auto w = owner.tickCount()+dt;
  if(w>aniWaitTime)
    aniWaitTime = w;
  }

void Npc::implFaiWait(uint64_t dt) {
  faiWaitTime          = owner.tickCount()+dt;
  aiState.loopNextTime = faiWaitTime;
  }

void Npc::implSetFightMode(const Animation::EvCount& ev) {
  const auto ws = visual.fightMode();
  if(!visual.setFightMode(ev.weaponCh))
    return;

  if(ev.weaponCh==zenkit::MdsFightMode::NONE && (ws==WeaponState::W1H || ws==WeaponState::W2H)) {
    if(auto melee = invent.currentMeleeWeapon()) {
      auto at = centerPosition();
      if(melee->handle().material==ItemMaterial::MAT_METAL)
        sfxWeapon = ::Sound(owner,::Sound::T_Regular,"UNDRAWSOUND_ME.WAV",at,2500,false); else
        sfxWeapon = ::Sound(owner,::Sound::T_Regular,"UNDRAWSOUND_WO.WAV",at,2500,false);
      sfxWeapon.play();
      }
    }
  else if(ev.weaponCh==zenkit::MdsFightMode::SINGLE_HANDED || ev.weaponCh==zenkit::MdsFightMode::DUAL_HANDED) {
    if(auto melee = invent.currentMeleeWeapon()) {
      auto at = centerPosition();
      if(melee->handle().material==ItemMaterial::MAT_METAL)
        sfxWeapon = ::Sound(owner,::Sound::T_Regular,"DRAWSOUND_ME.WAV",at,2500,false); else
        sfxWeapon = ::Sound(owner,::Sound::T_Regular,"DRAWSOUND_WO.WAV",at,2500,false);
      sfxWeapon.play();
      }
    }
  // NOTE: in original-game DoDoAniEvents @0x00742a20 the bow/crossbow draw sound is NOT
  // engine-hardcoded like the melee DEF_DRAWSOUND path -- the bow/crossbow draw & holster transition
  // anims (Humans_BowT*/CBowT*.mds) carry an ordinary *eventSFX "Drawsound_Bow" that the generic SFX
  // path already plays (processSfx -> emitSoundEffect). OpenGothic ALSO hardcoded "DRAWSOUND_BOW"
  // here, so the draw "shing" played twice on the same frame. Rely on the data-driven eventSFX only.
  dropTorch();
  visual.stopDlgAnim(*this);
  updateWeaponSkeleton();
  // NOTE: in original-game oCNpc::EV_DrawWeapon2 @0x0074d580 fires
  // CreatePassivePerception(self, 24=PERC_DRAWWEAPON, self, NULL) when a draw transition
  // completes -- the mirror of EV_RemoveWeapon2 @0x0074e630 / PERC_ASSESSREMOVEWEAPON, which
  // OpenGothic already sends from closeWeapon. PERC_DRAWWEAPON was declared but never sent, so
  // "sheathe your weapon" guard reactions never ran. Player-guarded to match the remove path.
  if(ev.weaponCh!=zenkit::MdsFightMode::NONE && isPlayer())
    owner.sendPassivePerc(*this,*this,PERC_DRAWWEAPON);
  }

bool Npc::implAiFlee(uint64_t dt) {
  if(currentTarget==nullptr)
    return true;

  if(isFalling())
    return true;

  auto& oth = *currentTarget;

  const WayPoint* wp      = nullptr;
  const float     maxDist = 5*100; // 5 meters

  owner.findWayPoint(position(),[&](const WayPoint& p) {
    if(p.useCounter()>0 || qDistTo(&p)>maxDist*maxDist)
      return false;
    if(p.underWater)
      return false;
    if(!canRayHitPoint(p.position() + Vec3(0,10,0),true))
      return false;
    if(wp==nullptr || oth.qDistTo(&p)>oth.qDistTo(wp))
      wp = &p;
    return false;
    });

  if(go2.flag!=GT_Flee && go2.flag!=GT_No) {
    clearGoTo();
    }

  auto anim = (go2.flag!=GT_No)?AnimationSolver::TurnType::None:AnimationSolver::TurnType::Std;
  if(wp==nullptr || oth.qDistTo(wp)<oth.qDistTo(*this)) {
    auto  dx  = oth.x-x;
    auto  dz  = oth.z-z;
    if(implTurnTo(-dx,-dz,anim,dt))
      return (go2.flag==GT_Flee);
    } else {
    auto  dx  = wp->pos.x-x;
    auto  dz  = wp->pos.z-z;
    if(implTurnTo(dx,dz,anim,dt))
      return (go2.flag==GT_Flee);
    }

  go2.setFlee();
  setAnim(Anim::Move);
  return true;
  }

bool Npc::setGoToLadder() {
  if(go2.wp==nullptr || go2.wp!=wayPath.first())
    return false;
  auto inter = go2.wp->ladder;
  if(inter==nullptr)
    return false;
  auto pos   = inter->nearestPoint(*this);
  if(MoveAlgo::isClose(*this,pos,MAX_AI_USE_DISTANCE)) {
    if(!inter->isAvailable())
      setAnim(AnimationSolver::Idle);
    else if(setInteraction(inter))
      wayPath.pop();
    return true;
    }
  return false;
  }

void Npc::commitDamage() {
  if(currentTarget==nullptr)
    return;
  if(!fghAlgo.isInAttackRange(*this,*currentTarget,owner.script()))
    return;
  if(!fghAlgo.isInFocusAngle(*this,*currentTarget))
    return;
  currentTarget->takeDamage(*this,nullptr);
  }

void Npc::takeDamage(Npc &other, const Bullet* b) {
  if(isDown())
    return;

  assert(b==nullptr || !b->isSpell());
  const auto& pose    = visual.pose();
  const bool  isJumpb = pose.isJumpBack(owner.tickCount()) && fghAlgo.isInJumpBackAngle(*this,other);
  // NOTE: in original-game oCNpc::CanParade (Gothic2.exe 0x006b15b0) blocks an incoming strike
  // when the attacker is within a +-90-degree front cone of the defender (not the 30-degree
  // attack focus cone). OpenGothic used the 30-degree isInFocusAngle, so off-axis (30-90deg)
  // attacks against a parrying NPC were unblockable and dealt full damage.
  const bool  isBlock = (!other.isMonster() || other.inventory().activeWeapon()!=nullptr) &&
                         fghAlgo.isInFocusAngle(*this,other,90) &&
                         pose.isDefence(owner.tickCount());
  // NOTE: in original-game oCAniCtrl_Human::HitCombo (Gothic2.exe 0x006b0260) the parade path is
  // reached only when oCNpc::GetDamageByType(attacker, DAM_FLY/0x10)==0; a FLY (knockback) attack
  // skips CanParade entirely and can be neither parried nor jump-dodged. OpenGothic omitted this
  // gate, so a defender could block FLY-type attacks.
  const bool  flyAtk  = (DamageCalculator::damageTypeMask(other) & (1<<zenkit::DamageType::FLY))!=0;

  lastHit = &other;
  if(!isPlayer())
    setOther(&other);
  owner.sendPassivePerc(*this,other,*this,PERC_ASSESSFIGHTSOUND);

  if(!(isBlock || isJumpb) || b!=nullptr || flyAtk) {
    takeDamage(other,b,COLL_DOEVERYTHING,0,false);
    } else {
    if(invent.activeWeapon()!=nullptr)
      visual.emitBlockEffect(*this,other);
    // NOTE: in original-game oCNpc::EV_Parade (Gothic2.exe 0x007522d0) a started parade ends with
    // oCNpc::AssessDamage_S(defender, attacker, value=0) (Gothic2.exe 0x0075c280), which runs the
    // defender's own PERC_ASSESSDAMAGE and unconditionally broadcasts PERC_ASSESSOTHERSDAMAGE
    // (CreatePassivePerception perc 9, OTHER=attacker, VICTIM=defender) to nearby witnesses. So a
    // blocked blow still makes the defender react and still recruits its guild-mates. OpenGothic ran
    // neither on a pure block, so a perfectly-parried attacker stayed un-assessed and no allies were
    // alerted. Mirror the damage-path calls (lines ~2145 / ~2193) for the parade (not isJumpb dodge).
    if(isBlock) {
      perceptionProcess(other,this,0,PERC_ASSESSDAMAGE);
      owner.sendPassivePerc(*this,other,*this,PERC_ASSESSOTHERSDAMAGE);
      }
    }
  }

void Npc::takeDamage(Npc& other, const Bullet* b, const VisualFx* vfx, int32_t splId) {
  if(isDown())
    return;

  lastHitSpell = splId;
  lastHit      = &other;
  if(!isPlayer())
    setOther(&other);

  CollideMask bMask = owner.script().canNpcCollideWithSpell(*this,&other,splId);
  if(bMask!=COLL_DONOTHING)
    Effect::onCollide(owner,vfx,position(),this,&other,splId);
  takeDamage(other,b,bMask,splId,true);
  }

void Npc::takeDamage(Npc& other, const Bullet* b, const CollideMask bMask, int32_t splId, bool isSpell) {
  float a  = angleDir(other.x-x,other.z-z);
  float da = a-angle;
  if(std::cos(da*M_PI/180.0)<0)
    lastHitType='A'; else
    lastHitType='B';

  DamageCalculator::Damage dmg={};
  DamageCalculator::Val    hitResult;
  SpellCategory            splCat     = SpellCategory::SPELL_BAD;
  // NOTE: in original-game oCNpc::OnDamage_Condition @0x0066cf30 the drop-unconscious branch is
  // gated on oCAniCtrl_Human::IsInWater(...)==0 (@0x006b8a40), which is true for water-level 1
  // (swim) AND 2 (dive). OpenGothic's mutually-exclusive MoveAlgo states make isSwim() match only
  // Swim, so a diving victim at <=0 HP slipped past the guard and dropped unconscious instead of
  // dying; add !isDive() to cover water-level 2.
  const bool               dontKill   = ((b==nullptr && splId==0) || (bMask & COLL_DONTKILL)) && (!isSwim() && !isDive());
  int32_t                  damageType = DamageCalculator::damageTypeMask(other);

  if(isSpell) {
    auto& spl  = owner.script().spellDesc(splId);
    splCat     = SpellCategory(spl.spell_type);
    damageType = spl.damage_type;
    // NOTE: in original-game ApplyDamages (Gothic2.exe 0x0065e5a0) the spell total is split
    // equally across its damage-type bits (round(total/numTypes)); see commitSpell.
    int32_t splTypes = 0;
    for(size_t i=0; i<zenkit::DamageType::NUM; ++i)
      if((damageType&(1<<i))!=0)
        ++splTypes;
    const int32_t perType = (splTypes>0)
      ? int32_t(float(spl.damage_per_level)/float(splTypes) + 0.5f) : 0;
    for(size_t i=0; i<zenkit::DamageType::NUM; ++i)
      if((damageType&(1<<i))!=0)
        dmg[i] = perType;
    }

  if(!isSpell || splCat==SpellCategory::SPELL_BAD) {
    perceptionProcess(other,this,0,PERC_ASSESSDAMAGE);
    fghAlgo.onTakeHit();
    implFaiWait(0);
    }

  hitResult = DamageCalculator::damageValue(other,*this,b,isSpell,dmg,bMask);
  if(!isSpell && !isDown() && hitResult.hasHit)
    owner.addWeaponHitEffect(other,b,*this).play();

  if(isDown()) {
    // NOTE: in original-game oCNpc::OnDamage_Condition @0x0066cf30 a re-hit on a downed NPC goes to
    // ZS_Dead only when the victim is already dead OR the blow is lethal (C_DropUnconscious==false);
    // a non-lethal blow (fists) leaves an unconscious victim down (oCNpc::DropUnconscious @0x00735eb0
    // early-returns while IsInState(-4)). 'dontKill' is OpenGothic's non-lethal / allow-unconscious
    // flag (same role as the changeAttribute arg below), so the raw 'death=dontKill' was inverted: it
    // killed knocked-out NPCs with fists and revived corpses hit by a stray arrow/spell.
    onNoHealth(isDead() || !dontKill,HS_NoSound);
    return;
    }

  if(hitResult.hasHit) {
    auto state = bodyStateMasked();
    if(interactive()==nullptr && ((state&BS_FLAG_INTERRUPTABLE)!=BS_NONE || state==BS_RUN || state==BS_NONE)) {
      //NONE/RUN requires for monsters like waran
      const bool noInter = (hnpc->bodystate_interruptable_override!=0);
      if(!noInter) {
        //NOTE: kepp rotation animation: this results in more accurate fight with trolls
        // visual.setAnimRotate(*this,0);
        visual.interrupt(); // TODO: put down in pipeline, at Pose and merge with setAnimAngGet
        }

      if((damageType & (1<<zenkit::DamageType::FLY))==0)
        setAnimAngGet(lastHitType=='A' ? Anim::StumbleA  : Anim::StumbleB);
      }
    }

  // throw enemy
  // NOTE: in original-game oCNpc::OnDamage_Anim (Gothic2.exe 0x00675bd0) the FLY throwback
  // (oCAIHuman::StartFlyDamage) is gated on the resolved collision mask carrying the victim-state
  // bit (COLL_APPLYVICTIMSTATE or the catch-all COLL_DOEVERYTHING); a spell whose
  // C_CanNpcCollideWithSpell mask omits it (incl. COLL_DONOTHING / COLL_APPLYDAMAGE-only) deals
  // damage with no knockback. hitResult.hasHit is true even for value==0 / no-victim-state masks,
  // so gate the throwback on the same flag the perception broadcasts above already use.
  if(hitResult.hasHit && (damageType & (1<<zenkit::DamageType::FLY)) &&
     (bMask&(COLL_APPLYVICTIMSTATE|COLL_DOEVERYTHING))) {
    mvAlgo.accessDamFly(x-other.x, z-other.z, lastHitType);
    }

  // NOTE: in original-game oCNpc::AssessDamage_S (Gothic2.exe 0x0075c280) the witness broadcast
  // CreatePassivePerception(PERC_ASSESSOTHERSDAMAGE) fires together with the self PERC_ASSESSDAMAGE
  // whenever the hit lands, with no dependency on the net damage value -- so a blow fully absorbed
  // by armour (value==0) still alerts nearby NPCs. OpenGothic nested it under value>0, suppressing
  // that witness reaction. The value-dependent DEFEAT/MURDER/AARGH reactions stay under value>0.
  if(hitResult.hasHit && (bMask&(COLL_APPLYVICTIMSTATE|COLL_DOEVERYTHING)))
    owner.sendPassivePerc(*this,other,*this,PERC_ASSESSOTHERSDAMAGE);

  if(hitResult.value>0) {
    currentOther = &other;
    changeAttribute(ATR_HITPOINTS,-hitResult.value,dontKill);

    if(bMask&(COLL_APPLYVICTIMSTATE|COLL_DOEVERYTHING)) {
      if(isUnconscious()){
        owner.sendPassivePerc(*this,other,*this,PERC_ASSESSDEFEAT);
        }
      else if(isDead()) {
        owner.sendPassivePerc(*this,other,*this,PERC_ASSESSMURDER);
        }
      else {
        // NOTE: in original-game oCNpc::OnDamage_Sound (Gothic2.exe 0x0067a8a0), reached
        // unconditionally from oCNpc::OnDamage (0x006660e0) on every registered alive hit, the hurt
        // voice line is always emitted; its rand() only selects a voice variation
        // (rand()%NPC_VOICE_VARIATION_MAX), never whether to play. OpenGothic gated it on a 50%
        // coin-flip, silencing half of all pain reactions. Always emit.
        emitSoundSVM("SVM_%d_AARGH");
        }
      }
    }
  }

void Npc::takeFallDamage(const Vec3& fallSpeed) {
  if(bodyStateMasked()==BS_FALL) {
    if(!isFallingDeep()) {
      // small fall
      setAnim(Anim::Idle);
      } else {
      const float a  = angleDir(-fallSpeed.x,-fallSpeed.z);
      const float da = a-angle;
      if(std::cos(da*M_PI/180.0)<0 || Vec2(fallSpeed.x,fallSpeed.z).length()<0.1f)
        lastHitType='A'; else
        lastHitType='B';
      setAnim(lastHitType=='A' ? Anim::FallenA : Anim::FallenB);
      }
    }
  // NOTE: in original-game oCNpc::CreateFallDamage (Gothic2.exe 0x00681da0) scales damage by the
  // vertical fall-drop height only (the scalar passed by oCAniCtrl_Human::CheckFallStates
  // @0x006b5810), never by horizontal speed. damageFall() reconstructs height = speed^2/(2g), which
  // equals the vertical drop only when fed the vertical impact velocity; fallSpeed.length() adds
  // horizontal jump/slide/bounce velocity and over-counts damage. Use the vertical component,
  // consistent with the FallDeep animation test in MoveAlgo (fallSpeed.y/gravity).
  auto dmg = DamageCalculator::damageFall(*this,std::abs(fallSpeed.y));
  if(!dmg.hasHit)
    return;
  int32_t hp = attribute(ATR_HITPOINTS);
  if(hp>dmg.value) {
    emitSoundSVM("SVM_%d_AARGH");
    clearState(true);
    }
  changeAttribute(ATR_HITPOINTS,-dmg.value,false);
  }

void Npc::takeDrownDamage() {
  changeAttribute(Attribute::ATR_HITPOINTS, -attribute(Attribute::ATR_HITPOINTSMAX), false);
  }

Npc *Npc::updateNearestEnemy() {
  if(aiPolicy!=NpcProcessPolicy::AiNormal)
    return nullptr;

  Npc*  ret  = nullptr;
  float dist = std::numeric_limits<float>::max();
  if(nearestEnemy!=nullptr &&
     (!nearestEnemy->isDown() && canSenseNpc(*nearestEnemy,true)!=SensesBit::SENSE_NONE)) {
    ret  = nearestEnemy;
    dist = qDistTo(*ret);
    }

  owner.detectNpcNear([this,&ret,&dist](Npc& n){
    if(!isEnemy(n) || n.isDown() || &n==this)
      return;

    float d = qDistTo(n);
    if(d<dist && canSenseNpc(n,true)!=SensesBit::SENSE_NONE) {
      ret  = &n;
      dist = d;
      }
    });
  nearestEnemy = ret;
  return nearestEnemy;
  }

Npc* Npc::updateNearestBody() {
  if(aiPolicy!=NpcProcessPolicy::AiNormal)
    return nullptr;

  Npc*  ret  = nullptr;
  float dist = std::numeric_limits<float>::max();

  owner.detectNpcNear([this,&ret,&dist](Npc& n){
    // NOTE: in original-game oCNpc::PerceptionCheck @0x0075dd30 classifies a vob as a "body" when
    // IsDead() (hp<1) OR IsUnconscious() (oCNpc_States::IsInState(-4), see oCNpc::IsUnconscious
    // @0x00736750); unconscious NPCs keep hp>0, so a dead-only test silently dropped them as
    // PERC_ASSESSBODY candidates (guards never ran B_AssessBody on a knocked-out NPC/player).
    if(!n.isDown())
      return;

    float d = qDistTo(n);
    if(d<dist && canSenseNpc(n,true)!=SensesBit::SENSE_NONE) {
      ret  = &n;
      dist = d;
      }
    });
  return ret;
  }

void Npc::tickTimedEvt(Animation::EvCount& ev) {
  if(ev.timed.empty())
    return;

  std::sort(ev.timed.begin(),ev.timed.end(),[](const Animation::EvTimed& a,const Animation::EvTimed& b){
    return a.time<b.time;
    });

  // https://auronen.cokoliv.eu/gmc/zengin/anims/events/
  for(auto& i:ev.timed) {
    switch(i.def) {
      case zenkit::MdsEventType::ITEM_CREATE: {
        if(auto it = invent.addItem(i.item,1,world())) {
          invent.putToSlot(*this,it->clsId(),i.slot[0]);
          }
        break;
        }
      case zenkit::MdsEventType::ITEM_INSERT: {
        invent.putCurrentToSlot(*this,i.slot[0]);
        break;
        }
      case zenkit::MdsEventType::ITEM_REMOVE:
      case zenkit::MdsEventType::ITEM_DESTROY: {
        invent.clearSlot(*this, "", i.def != zenkit::MdsEventType::ITEM_REMOVE);
        break;
        }
      case zenkit::MdsEventType::ITEM_PLACE: {
        if(currentInteract!=nullptr)
          Inventory::moveItem(*this, invent, *currentInteract);
        break;
        }
      case zenkit::MdsEventType::ITEM_EXCHANGE: {
        if(!invent.clearSlot(*this,i.slot[0],true)) {
          // fallback for cooking animations
          invent.putCurrentToSlot(*this,i.slot[0]);
          invent.clearSlot(*this,"",true);
          }
        if(auto it = invent.addItem(i.item,1,world())) {
          invent.putToSlot(*this,it->clsId(),i.slot[0]);
          }
        break;
        }
      case zenkit::MdsEventType::SET_FIGHT_MODE:
        break;
      case zenkit::MdsEventType::MUNITION_PLACE: {
        auto active=invent.activeWeapon();
        if(active!=nullptr) {
          const int32_t munition = active->handle().munition;
          invent.putAmmunition(*this,uint32_t(munition),i.slot[0]);
          }
        break;
        }
      case zenkit::MdsEventType::MUNITION_REMOVE: {
        invent.putAmmunition(*this,0,"");
        break;
        }
      case zenkit::MdsEventType::TORCH_DRAW:
        setTorch(true);
        break;
      case zenkit::MdsEventType::TORCH_INVENTORY:
        processDefInvTorch();
        break;
      case zenkit::MdsEventType::TORCH_DROP:
        dropTorch();
        break;
      case zenkit::MdsEventType::SOUND_DRAW:
        break;
      case zenkit::MdsEventType::SOUND_UNDRAW:
        break;
      case zenkit::MdsEventType::MESH_SWAP:
        break;
      case zenkit::MdsEventType::HIT_LIMB:
        break;
      case zenkit::MdsEventType::HIT_DIRECTION:
        break;
      case zenkit::MdsEventType::DAMAGE_MULTIPLIER:
        break;
      case zenkit::MdsEventType::PARRY_FRAME:
        break;
      case zenkit::MdsEventType::OPTIMAL_FRAME:
        break;
      case zenkit::MdsEventType::HIT_END:
        break;
      case zenkit::MdsEventType::COMBO_WINDOW:
        break;
      case zenkit::MdsEventType::UNKNOWN:
        break;
      }
    }
  }

void Npc::tickRegen(int32_t& v, const int32_t max, const int32_t chg, const uint64_t dt) {
  uint64_t tick = owner.tickCount();
  if(tick<dt || chg==0)
    return;
  int32_t time0 = int32_t(tick%1000);
  int32_t time1 = time0+int32_t(dt);

  int32_t val0 = (time0*chg)/1000;
  int32_t val1 = (time1*chg)/1000;

  int32_t nextV = std::max(0,std::min(v+val1-val0,max));
  if(v!=nextV) {
    v = nextV;
    // check health, in case of negative chg
    checkHealth(true,false);
    }
  }

void Npc::tickAnimationTags() {
  Animation::EvCount ev;
  const bool hasEvents = visual.processEvents(owner,lastEventTime,ev);
  visual.processLayers(owner);
  visual.setNpcEffect(owner,*this,hnpc->effect,hnpc->flags);
  if(!hasEvents)
    return;

  for(auto& i:ev.morph)
    visual.startMMAnim(*this,i.anim,i.node);
  // NOTE: in original-game the quiet-sound footstep perception is suppressed by the
  // persistent sneak walk-mode, not the transient body-state: during a mobsi interaction
  // (e.g. lock-picking) the body-state leaves BS_SNEAK, which wrongly let footstep sounds
  // wake nearby NPCs while still sneaking (#639). Key it off the WM_Sneak walk-flag.
  // NOTE: in original-game oCAIHuman::CreateFootStepSound @0x0069b180 the quiet-sound perception
  // (AssessQuietSound_S) only runs while IsWalking() and the AI water-mode is not the dive value --
  // never airborne, swimming, or diving. The gfx ground-events driving ev.groundSounds still fire
  // underwater (see the audible-footstep gate at animation.cpp processSfx), so without this gate a
  // swimming/diving player kept broadcasting silent footstep perceptions; mirror the audible gate.
  if(ev.groundSounds>0 && isPlayer() && (wlkMode&WalkBit::WM_Sneak)!=WalkBit::WM_Sneak &&
     !isInAir() && !isSwim() && !isDive())
    world().sendImmediatePerc(*this,*this,*this,PERC_ASSESSQUIETSOUND);
  if(ev.def_opt_frame>0)
    commitDamage();
  implSetFightMode(ev);
  tickTimedEvt(ev);
  }

void Npc::tick(uint64_t dt) {
  static bool dbg = false;
  static int  kId = 432;
  if(dbg && !isPlayer() && hnpc->id!=kId)
    return;

  assert(go2.flag!=GoToHint::GT_Enemy && go2.flag!=GoToHint::GT_EnemyG);

  tickAnimationTags();

  if(!visual.pose().hasAnim())
    setAnim(AnimationSolver::Idle);

  if(isDive()) {
    uint32_t gl = guild();
    int32_t  v  = world().script().guildVal().dive_time[gl]*1000;
    int32_t  t  = diveTime();
    if(v>=0 && t>v+int(dt)) {
      int tickSz = world().script().npcDamDiveTime();
      if(tickSz>0) {
        t-=v;
        int dmg = t/tickSz - (t-int(dt))/tickSz;
        if(dmg>0) {
          lastHit = nullptr;
          changeAttribute(ATR_HITPOINTS,-dmg,false);
          }
        }
      }
    }

  nextAiAction(aiQueueOverlay,dt);

  if(tickCast(dt))
    return;

  if(!isDead()) {
    // NOTE: in original-game oCNpc::Regenerate @0x00741fd0 the per-tick HP regen is applied via
    // oCNpc::ChangeAttribute(HITPOINTS,+1) @0x0072ff60, whose IMMORTAL guard rejects every HP change
    // except the -999 kill sentinel -- so an immortal NPC never regenerates HP. MANA regen routes
    // through ChangeAttribute(MANA,+1), which that guard does NOT block. OpenGothic's tickRegen writes
    // the attribute directly, bypassing the guard (and the changeAttribute() immortal fix), so an
    // immortal NPC left below max regenerated back to full. Gate only the HP regen on !isImmortal().
    if(!isImmortal())
      tickRegen(hnpc->attribute[ATR_HITPOINTS],hnpc->attribute[ATR_HITPOINTSMAX],
                hnpc->attribute[ATR_REGENERATEHP],dt);
    tickRegen(hnpc->attribute[ATR_MANA],hnpc->attribute[ATR_MANAMAX],
              hnpc->attribute[ATR_REGENERATEMANA],dt);
    }

  if(waitTime>=owner.tickCount() || aniWaitTime>=owner.tickCount() || outWaitTime>owner.tickCount()) {
    if(!isPlayer() && go2.flag!=GT_Flee && faiWaitTime<owner.tickCount() && currentTarget!=nullptr) {
      implTurnToFai(*currentTarget,dt);
      }
    mvAlgo.tick(dt,MoveAlgo::WaitMove);
    return;
    }

  if(!isDown()) {
    implLookAtNpc(dt);
    implLookAtWp(dt);

    if(implAttack(dt))
      return;

    if(implGoTo(dt)) {
      if(go2.flag==GT_Flee)
        implAiTick(dt);
      return;
      }
    }

  mvAlgo.tick(dt);
  implAiTick(dt);
  }

bool Npc::prepareTurn() {
  const auto st = bodyStateMasked();
  if(interactive()==nullptr && (st==BS_WALK || st==BS_SNEAK)) {
    visual.stopWalkAnim(*this);
    setAnimRotate(0);
    return false;
    }
  if(interactive()==nullptr) {
    visual.stopWalkAnim(*this);
    visual.stopDlgAnim(*this);
    }
  return true;
  }

void Npc::nextAiAction(AiQueue& queue, uint64_t dt) {
  if(isInAir())
    return;
  if(queue.size()==0)
    return;
  auto act = queue.pop();
  switch(act.act) {
    case AI_None: break;
    case AI_LookAtNpc:{
      currentLookAt=nullptr;
      currentLookAtNpc=act.target;
      break;
      }
    case AI_LookAt:{
      currentLookAtNpc=nullptr;
      currentLookAt=act.point;
      break;
      }
    case AI_TurnAway: {
      if(!prepareTurn()) {
        queue.pushFront(std::move(act));
        break;
        }
      if(act.target!=nullptr && implTurnAway(*act.target,dt)) {
        queue.pushFront(std::move(act));
        break;
        }
      break;
      }
    case AI_TurnToNpc: {
      if(!prepareTurn()) {
        queue.pushFront(std::move(act));
        break;
        }
      if(act.target!=nullptr && implTurnTo(*act.target,dt)) {
        queue.pushFront(std::move(act));
        break;
        }
      // Not looking quite correct in dialogs, when npc turns around
      // Example: Esteban dialog
      // currentLookAt    = nullptr;
      // currentLookAtNpc = nullptr;
      break;
      }
    case AI_WhirlToNpc: {
      if(!prepareTurn()) {
        queue.pushFront(std::move(act));
        break;
        }
      if(act.target!=nullptr && implWhirlTo(*act.target,dt)) {
        queue.pushFront(std::move(act));
        break;
        }
      break;
      }
    case AI_GoToNpc:
      if(!setInteraction(nullptr)) {
        queue.pushFront(std::move(act));
        break;
        }
      attachToPoint(nullptr);
      go2.set(act.target);
      wayPath.clear();
      break;
    case AI_GoToNextFp: {
      if(!setInteraction(nullptr)) {
        queue.pushFront(std::move(act));
        break;
        }
      auto fp = owner.findNextFreePoint(*this,act.s0);
      if(fp!=nullptr) {
        attachToPoint(fp);
        go2.set(fp,GoToHint::GT_NextFp);
        wayPath.clear();
        }
      break;
      }
    case AI_GoToPoint: {
      if(isInAir() || !setInteraction(nullptr)) {
        queue.pushFront(std::move(act));
        break;
        }
      if(wayPath.last()!=act.point) {
        wayPath     = owner.wayTo(*this,*act.point);
        auto wpoint = wayPath.pop();

        if(wpoint!=nullptr) {
          go2.set(wpoint);
          attachToPoint(wpoint);
          } else {
          attachToPoint(act.point);
          clearGoTo();
          }
        }
      break;
      }
    case AI_StopLookAt:
      currentLookAtNpc=nullptr;
      currentLookAt=nullptr;
      visual.setHeadRotation(0,0);
      break;
    case AI_RemoveWeapon:
      if(!isDead()) {
        if(closeWeapon(false)) {
          stopWalkAnimation();
          }
        auto ws = weaponState();
        if(ws!=WeaponState::NoWeapon){
          queue.pushFront(std::move(act));
          }
        }
      break;
    case AI_StartState:
      // NOTE: a new state can be stater within a daly routiine, such as TA_Sleep, with: ZS_GotoBed -> ZS_Sleep.
      // In such cases it's important to preserve aiState.eTime.
      if(startState(act.func,act.s0,aiState.eTime,act.i0==0)) {
        setOther(act.target);
        setVictim(act.victim);
        }
      break;
    case AI_PlayAnim:{
      owner.script().eventPlayAni(*this, act.s0);
      if(auto sq = playAnimByName(act.s0,BS_NONE)) {
        implAniWait(uint64_t(sq->totalTime()));
        implFaiWait(uint64_t(sq->totalTime()));
        } else {
        if(visual.hasAnim(act.s0))
          queue.pushFront(std::move(act));
        }
      break;
      }
    case AI_PlayAnimBs:{
      BodyState bs = BodyState(act.i0);
      if(auto sq = playAnimByName(act.s0,bs)) {
        implAniWait(uint64_t(sq->totalTime()));
        implFaiWait(uint64_t(sq->totalTime()));
        } else {
        if(visual.hasAnim(act.s0)) {
          queue.pushFront(std::move(act));
          } else {
          /* ZS_MM_Rtn_Sleep will set NPC_WALK mode and run T_STAND_2_SLEEP animation.
           * The problem is: T_STAND_2_SLEEP may not exists, in that case only NPC_WALK should be applied,
           * we will do so by playing Idle anim.
           */
          setAnim(Anim::Idle);
          }
        }
      break;
      }
    case AI_Wait:
      // NOTE: in original-game oCNpc::EV_Wait (Gothic2.exe 0x00756820) the wait handler stops
      // walking and cancels any in-progress turn animation before counting down, so the NPC
      // idles in place rather than drifting/rotating through the wait.
      stopWalkAnimation();
      implAiWait(uint64_t(act.i0));
      break;
    case AI_StandUp:
    case AI_StandUpQuick: {
      const auto bs = bodyStateMasked();
      // NOTE: B_ASSESSTALK calls AI_StandUp, to make npc stand, if it's not on a chair or something
      if(interactive()!=nullptr) {
        if((interactive()->isLadder() && !isPlayer()) || !setInteraction(nullptr,false)) {
          queue.pushFront(std::move(act));
          }
        break;
        }
      else if(bs==BS_UNCONSCIOUS || bs==BS_LIE) {
        if(!setAnim(Anim::Idle))
          queue.pushFront(std::move(act)); else
          implAniWait(visual.pose().animationTotalTime());
        }
      else if(bs!=BS_DEAD) {
        visual.stopAnim(*this,"");
        setStateItem(MeshObjects::Mesh(),"");
        setAnim(Anim::Idle);
        }
      break;
      }
    case AI_EquipArmor:
      invent.equipArmor(act.i0,*this);
      break;
    case AI_EquipBestArmor:
      invent.equipBestArmor(*this);
      break;
    case AI_EquipMelee:
      invent.equipBestMeleeWeapon(*this);
      break;
    case AI_EquipRange:
      invent.equipBestRangedWeapon(*this);
      break;
    case AI_UseMob: {
      if(act.i0<0) {
        if(!setInteraction(nullptr))
          queue.pushFront(std::move(act));
        break;
        }
      /*
       * Rhademes doesn't quit talk properly
      if(owner.script().isTalk(*this)) {
        queue.pushFront(std::move(act));
        break;
        }*/

      auto inter = owner.availableMob(*this,act.s0);
      if(inter==nullptr) {
        /* in L`Hiver, version 1.3 there is a typo: "COOL" instead of "BSCOOL"
         * maybe 'scheme' need to be checked loosely, or maybe ignored.
         *
         * For now, if no mob found - discard command, to avoid npc soft-lock.
         */
        // queue.pushFront(std::move(act));
        break;
        }

      if(currentInteract!=nullptr && inter!=currentInteract) {
        setInteraction(nullptr);
        queue.pushFront(std::move(act));
        break;
        }

      if(inter!=nullptr) {
        auto pos = inter->nearestPoint(*this);
        if(currentInteract==nullptr && !MoveAlgo::isClose(*this, pos, MAX_AI_USE_DISTANCE)) { // too far
          // NOTE: in original-game oCNpc::EV_UseMob (@0x00754290) calls MarkAsUsed(mob,20000ms,npc)
          // as the NPC starts walking to the mob, reserving it so other NPCs don't also target it.
          inter->reserveFor(*this);
          go2.set(pos);
          // go to MOBSI and then complete AI_UseMob
          queue.pushFront(std::move(act));
          return;
          }
        if(!setInteraction(inter)) {
          // queue.pushFront(std::move(act));
          }
        }

      // NOTE: in original-game oCMobInter::AI_UseMobToState (Gothic2.exe 0x00721f00) clamps the
      // requested target to the mob's top state index (target = min(target, stateNum)) before
      // stepping toward it. Without the clamp an AI_UseMob target above stateNum saturates at
      // stateNum but never equals act.i0, so OpenGothic re-pushed the command forever and the NPC
      // soft-locked on the mob (the act.i0<0 detach is handled above).
      const int32_t goal = (currentInteract!=nullptr) ? std::min(act.i0,currentInteract->stateCount()) : act.i0;
      if(currentInteract==nullptr || currentInteract->stateId()!=goal) {
        queue.pushFront(std::move(act));
        return;
        }

      clearGoTo();
      break;
      }
    case AI_UseItem: {
      if(!isStanding()) {
        setAnim(Npc::Anim::Idle);
        queue.pushFront(std::move(act));
        break;
        }
      if(act.i0!=0)
        useItem(uint32_t(act.i0));
      break;
      }
    case AI_UseItemToState:
      if(act.i0!=0) {
        uint32_t itm   = uint32_t(act.i0);
        int      state = act.i1;
        if(state>0)
          visual.stopDlgAnim(*this);
        if(!invent.putState(*this,state>=0 ? itm : 0,state))
          queue.pushFront(std::move(act));
        }
      break;
    case AI_Teleport: {
      setPosition (act.point->position() );
      setDirection(act.point->direction());
      // NOTE: in original-game AI_Teleport (FUN_006de400) always runs oCNpc::BeamTo @0x00736ee0,
      // which after SetPositionWorld/SetHeadingAtWorld calls the virtual oCNpc::ResetPos @0x006824d0
      // -> Interrupt + stop ani layers + restart idle + SetMovLock(0). OpenGothic only moved the NPC,
      // so it kept sliding toward its old go-to target or finished its prior animation at the
      // destination. Interrupt the in-progress locomotion/animation (the visible subset of ResetPos).
      clearGoTo();
      setAnim(Npc::Anim::Idle);
      if(isPlayer()) {
        updateTransform();
        Gothic::inst().camera()->reset(this);
        }
      }
      break;
    case AI_DrawWeapon:
      if(canSwitchWeapon()) {
        if(!drawWeaponMelee() &&
           !drawWeaponBow())
          queue.pushFront(std::move(act));
        }
      break;
    case AI_DrawWeaponMelee:
      if(canSwitchWeapon()) {
        if(!drawWeaponMelee())
          queue.pushFront(std::move(act));
        }
      break;
    case AI_DrawWeaponRange:
      if(canSwitchWeapon()) {
        if(!drawWeaponBow())
          queue.pushFront(std::move(act));
        }
      break;
    case AI_DrawSpell: {
      if(canSwitchWeapon()) {
        const int32_t spell = act.i0;
        if(drawSpell(spell))
          aiExpectedInvest = act.i1; else
          queue.pushFront(std::move(act));
        }
      break;
      }
    case AI_Attack:
      if(currentTarget!=nullptr) {
        if(!fghAlgo.fetchInstructions(*this,*currentTarget,owner.script()))
          queue.pushFront(std::move(act));
        }
      break;
    case AI_Flee:
      if(!implAiFlee(dt))
        queue.pushFront(std::move(act));
      break;
    case AI_Dodge:
      if(auto sq = setAnimAngGet(Anim::MoveBack)) {
        visual.setAnimRotate(*this,0);
        implAniWait(uint64_t(sq->totalTime()));
        } else {
        queue.pushFront(std::move(act));
        }
      break;
    case AI_UnEquipWeapons:
      invent.unequipWeapons(owner.script(),*this);
      break;
    case AI_UnEquipArmor:
      invent.unequipArmor(owner.script(),*this);
      break;
    case AI_Output:
    case AI_OutputSvm:
    case AI_OutputSvmOverlay:{
      if(performOutput(act)) {
        if(aiPolicy!=NpcProcessPolicy::AiFar2) {
          uint64_t msgTime = 0;
          if(act.act==AI_Output) {
            msgTime = owner.script().messageTime(act.s0);
            } else {
            auto svm  = owner.script().messageFromSvm(act.s0,hnpc->voice);
            msgTime   = owner.script().messageTime(svm);
            }
          visual.startFaceAnim(*this,"VISEME",1,msgTime);
          }
        if(act.act!=AI_OutputSvmOverlay) {
          visual.startAnimDialog(*this);
          visual.setAnimRotate(*this,0);
          }
        } else {
        queue.pushFront(std::move(act));
        }
      break;
      }
    case AI_ProcessInfo: {
      const int PERC_DIST_DIALOG = 2000;

      if(act.target==nullptr)
        break;

      if(owner.isInDialog()) {
        queue.pushFront(std::move(act));
        break;
        }

      if(this!=act.target && act.target->isPlayer() && act.target->currentInteract!=nullptr) {
        //queue.pushFront(std::move(act));
        break;
        }

      if(act.target->qDistTo(*this)>PERC_DIST_DIALOG*PERC_DIST_DIALOG) {
        break;
        }

      if(act.target->interactive()==nullptr && !act.target->isAiBusy())
        act.target->stopWalkAnimation();
      if(interactive()==nullptr && !isAiBusy())
        stopWalkAnimation();

      if(auto p = owner.script().openDlgOuput(*this,*act.target)) {
        outputPipe = p;
        setOther(act.target);
        act.target->setOther(this);
        act.target->outputPipe = p;
        } else {
        queue.pushFront(std::move(act));
        }
      }
      break;
    case AI_StopProcessInfo:
      if(outputPipe->close()) {
        outputPipe = owner.script().openAiOuput();
        if(currentOther!=nullptr)
          currentOther->outputPipe = owner.script().openAiOuput();
        } else {
        queue.pushFront(std::move(act));
        }
      break;
    case AI_ContinueRoutine:
      resumeAiRoutine();
      break;
    case AI_AlignToWp:{
      // NOTE: in original-game AI_AlignToWP external (Gothic2.exe @0x006ee3a0) aligns to the
      // direction of the *nearest waynet waypoint* (zCWayNet::GetNearestWaypoint @0x007ad660), not
      // to the current free-point. OpenGothic shared one body with AI_AlignToFp reading currentFp,
      // which is a free-point (or null) after a prior goto -- the wrong heading source for AlignToWp.
      if(auto wp = owner.findWayPoint(position())){
        if(wp->dir.x!=0.f || wp->dir.z!=0.f){
          if(implTurnTo(wp->dir.x,wp->dir.z,AnimationSolver::TurnType::Std,dt))
            queue.pushFront(std::move(act));
          }
        }
      break;
      }
    case AI_AlignToFp:{
      if(auto fp = currentFp){
        if(fp->dir.x!=0.f || fp->dir.z!=0.f){
          if(implTurnTo(fp->dir.x,fp->dir.z,AnimationSolver::TurnType::Std,dt))
            queue.pushFront(std::move(act));
          }
        }
      break;
      }
    case AI_SetNpcsToState:{
      const int32_t r = act.i0*act.i0;
      owner.detectNpc(position(),float(hnpc->senses_range),[&act,this,r](Npc& other) {
        if(&other==this)
          return;
        if(other.isDead())
          return;
        if(qDistTo(other)>float(r))
          return;
        other.aiPush(AiQueue::aiStartState(act.func,1,other.currentOther,other.currentVictim,other.hnpc->wp));
        });
      break;
      }
    case AI_SetWalkMode:{
      setWalkMode(WalkBit(act.i0));
      break;
      }
    case AI_FinishingMove:{
      if(act.target==nullptr || !act.target->isUnconscious())
        break;

      if(!fghAlgo.isInFinishRange(*this,*act.target,owner.script())){
        queue.pushFront(std::move(act));
        go2.set(act.target);
        setAnim(Npc::Anim::Move);
        implGoTo(dt,fghAlgo.attackFinishDistance(owner.script()));
        }
      else if(!isStanding()) {
        clearGoTo();
        queue.pushFront(std::move(act));
        }
      else if(!implTurnTo(*act.target,dt)) {
        queue.pushFront(std::move(act));
        }
      else if(canFinish(*act.target)){
        setTarget(act.target);
        if(!finishingMove())
          queue.pushFront(std::move(act));
        }
      break;
      }
    case AI_TakeItem:{
      if(act.item==nullptr)
        break;
      if(takeItem(*act.item)==nullptr)
        queue.pushFront(std::move(act));
      break;
      }
    case AI_GotoItem:{
      go2.set(act.item);
      break;
      }
    case AI_PointAt:{
      if(act.point==nullptr)
        break;
      if(!implPointAt(act.point->position()))
        queue.pushFront(std::move(act));
      break;
      }
    case AI_PointAtNpc:{
      if(act.target==nullptr)
        break;
      if(!implPointAt(act.target->position()))
        queue.pushFront(std::move(act));
      break;
      }
    case AI_StopPointAt:{
      visual.stopAnim(*this,"T_POINT");
      break;
      }
    case AI_PrintScreen:{
      auto  msg     = act.s0;
      auto  posx    = act.i0;
      auto  posy    = act.i1;
      int   timesec = act.i2;
      auto  font    = act.s1;

      bool complete = false;
      if(aiOutputBarrier<=owner.tickCount()) {
        if(outputPipe->printScr(*this,timesec,msg,posx,posy,font))
          complete = true;
        }

      if(!complete)
        queue.pushFront(std::move(act));
      break;
      }
    }
  }

bool Npc::startState(ScriptFn id, std::string_view wp) {
  return startState(id,wp,gtime::endOfTime(),false);
  }

bool Npc::startState(ScriptFn id, std::string_view wp, gtime endTime, bool noFinalize) {
  if(!id.isValid())
    return false;

  if(aiState.funcIni==id) {
    if(!noFinalize) {
      // NOTE: B_AssessQuietSound can cause soft-lock on npc without this
      aiState.started = false;
      }
    if(!wp.empty())
      hnpc->wp = wp;
    return true;
    }

  clearAiQueue();
  clearState(noFinalize);
  if(!wp.empty())
    hnpc->wp = wp;

  {
    // ZS_GotoBed -> ZS_Sleep relie on clean state
    for(size_t i=0; i<PERC_Count; ++i)
      setPerceptionDisable(PercType(i));
  }

  if(wp=="TOT" && aiPolicy!=NpcProcessPolicy::Player && aiPolicy!=NpcProcessPolicy::AiNormal) {
    // workaround for Pedro removal script
    auto& point = owner.deadPoint();
    attachToPoint(nullptr);
    setPosition(point.position());
    }

  auto& st = owner.script().aiState(id);
  if(isPlayer() && !isPlayerEnabledState(st)) {
    // disable for now, as it causes infinite lock in freeze state
    // https://github.com/Try/OpenGothic/issues/906
    // extra 'aiStandup' to avoid issue with B_StopMagicFreeze
    aiPush(AiQueue::aiStandup());
    return false;
    }

  aiState.started      = false;
  aiState.funcIni      = st.funcIni;
  aiState.funcLoop     = st.funcLoop;
  aiState.funcEnd      = st.funcEnd;
  aiState.sTime        = owner.tickCount();
  aiState.eTime        = endTime;
  aiState.loopNextTime = owner.tickCount();
  aiState.hint         = st.name();
  return true;
  }

void Npc::clearState(bool noFinalize) {
  if(aiState.funcIni.isValid() && aiState.started) {
    if(!noFinalize)
      owner.script().invokeState(this,currentOther,currentVictim,aiState.funcEnd);  // cleanup
    aiPrevState = aiState.funcIni;
    invent.putState(*this,0,0);
    visual.stopItemStateAnim(*this);
    }
  aiState = AiState();
  }

bool Npc::isPlayerEnabledState(const ::AiState& st) const {
  // allowed player states are hard-coded
  // https://forum.worldofplayers.de/forum/threads/1533803-G1-AI_StartState-hardcoded-ZS-states-for-Player?p=26034737&viewfull=1#post26034737

  static const std::array playerEnabledStatesG1 = {
    "ZS_DEAD",   "ZS_UNCONSCIOUS", "ZS_MAGICFREEZE",
    "ZS_PYRO",   "ZS_ASSESSMAGIC", "ZS_ASSESSSTOPMAGIC",
    "ZS_ZAPPED", "ZS_SHORTZAPPED", "ZS_MAGICSLEEP",
    "ZS_MAGICFEAR"
    };
  static const std::array playerEnabledStatesG2 = {
    "ZS_DEAD",   "ZS_UNCONSCIOUS", "ZS_MAGICFREEZE",
    "ZS_PYRO",   "ZS_ASSESSMAGIC", "ZS_ASSESSSTOPMAGIC",
    "ZS_ZAPPED", "ZS_SHORTZAPPED", "ZS_MAGICSLEEP",
    "ZS_WHIRLWIND"
    };

  const auto* sym = owner.script().findSymbol(st.funcIni);
  if(sym==nullptr)
    return false;

  const auto& playerEnabledStates = (owner.version().game==2 ? playerEnabledStatesG2 : playerEnabledStatesG1);
  for(auto* pState:playerEnabledStates)
    if(sym->name()==pState)
      return true;
  return false;
  }

void Npc::tickRoutine() {
  if(!aiState.funcIni.isValid() && !isPlayer()) {
    auto r = currentRoutine();
    if(r.callback.isValid()) {
      auto t = endTime(r);
      startState(r.callback, r.wayPointName(), t, false);
      }
    else if(hnpc->start_aistate!=0) {
      auto endTime = owner.time();
      endTime.addMilis(uint64_t(gtime(4, 0).toInt()));
      startState(uint32_t(hnpc->start_aistate), "", endTime, false);
      }
    }

  if(!aiState.funcIni.isValid())
    return;

  auto& sc = owner.script();
  if(!aiState.started) {
    aiState.started      = true;
    aiState.loopNextTime = owner.tickCount();
    // WA: for gothic1 dialogs
    perceptionNextTime   = owner.tickCount();
    sc.invokeState(this,currentOther,currentVictim,aiState.funcIni);
    return;
    }

  const bool fastPath = (aiPolicy==NpcProcessPolicy::AiFar2 && routines.empty()); //HACK: don't process far away Npc
  if(aiState.loopNextTime<=owner.tickCount()) {
    aiState.loopNextTime = owner.tickCount() + perceptionTimeClampt();
    int loop = LOOP_CONTINUE;
    if(aiState.funcLoop.isValid()) {
      static const float MAX_DIST = 300;
      if(fastPath && currentFp!=nullptr && qDistTo(currentFp) < MAX_DIST*MAX_DIST) {
        loop = LOOP_CONTINUE;
        }
      else if(fastPath && currentFp!=nullptr) {
        // for debugging
        loop = sc.invokeState(this,currentOther,currentVictim,aiState.funcLoop);
        }
      else {
        loop = sc.invokeState(this,currentOther,currentVictim,aiState.funcLoop);
        }
      } else {
      // ZS_DEATH   have no loop-function, in G1, G2-classic
      // ZS_GETMEAT have no loop-function, in G2-notr
      loop = owner.version().hasZSStateLoop() ? 1 : 0;
      }

    if(aiState.eTime<=owner.time()) {
      // Avoid interruption of ZS_TALK/ZS_ATTACK
      if(currentTarget==nullptr && outputPipe->isFinished())
        loop = LOOP_END;
      }

    if(loop!=LOOP_CONTINUE) {
      clearState(false);
      currentOther  = nullptr;
      currentVictim = nullptr;
      }
    }
  }

void Npc::setTarget(Npc *t) {
  if(currentTarget==t)
    return;

  currentTarget = t;
  if(!go2.empty() && !isPlayer())
    clearGoTo();
  }

Npc *Npc::target() const {
  return currentTarget;
  }

void Npc::clearNearestEnemy() {
  nearestEnemy = nullptr;
  }

void Npc::setOther(Npc *ot) {
  if(isTalk() && ot && !ot->isPlayer())
    Log::e("unxepected perc acton");
  currentOther = ot;
  }

void Npc::setVictim(Npc* ot) {
  currentVictim = ot;
  }

bool Npc::haveOutput() const {
  if(owner.tickCount()<aiOutputBarrier)
    return true;
  return aiOutputOrderId()!=std::numeric_limits<int>::max();
  }

void Npc::setAiOutputBarrier(uint64_t dt, bool overlay) {
  aiOutputBarrier = owner.tickCount()+dt;
  if(!overlay)
    outWaitTime = aiOutputBarrier;
  }

void Npc::emitSoundEffect(std::string_view sound, float range, bool freeSlot) {
  auto sfx = ::Sound(owner,::Sound::T_Regular,sound,centerPosition(),range,freeSlot);
  sfx.play();
  }

void Npc::emitSoundGround(std::string_view sound, float range, bool freeSlot) {
  auto mat = mvAlgo.groundMaterial();
  string_frm buf(sound,"_",MaterialGroupNames[uint8_t(mat)]);
  auto sfx = ::Sound(owner,::Sound::T_Regular,buf,{x,y,z},range,freeSlot);
  sfx.play();
  }

void Npc::emitSoundSVM(std::string_view svm) {
  if(hnpc->voice==0)
    return;
  char frm [32]={};
  std::snprintf(frm,sizeof(frm),"%.*s",int(svm.size()),svm.data());

  char name[64]={};
  int  len = std::snprintf(name,sizeof(name),frm,int(hnpc->voice));

  // NOTE: in original-game oCNpc::OnDamage_Sound @0x0067a8a0 the non-lethal hurt voice line picks one
  // of NPC_VOICE_VARIATION_MAX (default 5) variants: v = rand()%MAX, and when v!=0 it appends "_<v>"
  // -> SVM_<voice>_AARGH_1 .. _4 (the bare SVM_<voice>_AARGH is used only when v==0; the lethal
  // "_DEAD" line is never varied). OpenGothic always emitted the bare AARGH, so pain was monotone.
  // emitSoundSVM is called only for the AARGH/DEAD hurt lines, so this is correctly scoped.
  const bool death = (svm.find("DEAD")!=std::string_view::npos);
  if(!death && len>0 && len<int(sizeof(name))) {
    const uint32_t NPC_VOICE_VARIATION_MAX = 5;
    const uint32_t v = owner.script().rand(NPC_VOICE_VARIATION_MAX);
    if(v!=0)
      std::snprintf(name+len,sizeof(name)-size_t(len),"_%u",v);
    }
  emitSoundEffect(name,2500,true);
  }

void Npc::startEffect(Npc& to, const VisualFx& vfx) {
  Effect e(vfx,owner,*this,SpellFxKey::Cast);
  e.setActive(true);
  e.setTarget(&to);
  visual.startEffect(owner, std::move(e), 0, true);
  }

void Npc::stopEffect(const VisualFx& vfx) {
  visual.stopEffect(vfx);
  }

void Npc::runEffect(Effect&& e) {
  visual.startEffect(owner, std::move(e), 0, true);
  }

bool Npc::isTargetableBySpell(TargetType t) const {
  if(bool(t&(TARGET_TYPE_ALL|TARGET_TYPE_NPCS)))
    return true;

  const Guild gil = Guild(trueGuild());

  const bool g2 = owner.version().game==2;
  const auto SEPERATOR_ORC = g2 ? GIL_SEPERATOR_ORC : GIL_G1_SEPERATOR_ORC;
  const auto G1_UNDEAD = (gil == GIL_G1_ZOMBIE || gil == GIL_G1_UNDEADORC || gil == GIL_G1_SKELETON);
  // NOTE: in original-game oCSpell::IsTargetTypeValid @0x00485fc0 the TARGET_TYPE_UNDEAD branch
  // accepts only true-guilds {20,21,31,32,34,37}; GIL_SKELETON_MAGE(33) is NOT in that set
  // (verified absent from the function's constants), so an undead-only spell must not auto-aim at
  // a Skeleton-Mage.
  const auto G2_UNDEAD = (gil == GIL_GOBBO_SKELETON ||
    gil == GIL_SUMMONED_GOBBO_SKELETON || gil == GIL_SKELETON ||
    gil == GIL_SUMMONED_SKELETON       || gil == GIL_ZOMBIE   ||
    gil == GIL_SHADOWBEAST_SKELETON);

  if(bool(t&TARGET_TYPE_HUMANS) && isHuman())
    return true;
  if(bool(t&TARGET_TYPE_ORCS) && gil>SEPERATOR_ORC)
    return true;
  if(bool(t&TARGET_TYPE_UNDEAD) && g2 && G2_UNDEAD)
    return true;
  if(bool(t&TARGET_TYPE_UNDEAD) && !g2 && G1_UNDEAD)
    return true;

  return false;
  }

void Npc::commitSpell() {
  auto active = invent.getItem(currentSpellCast);
  if(active==nullptr || !active->isSpellOrRune())
    return;

  const int32_t splId = active->spellId();
  const auto&   spl   = owner.script().spellDesc(splId);

  if(owner.version().game==2) {
    // NOTE: in original-game oCMag_Book::Spell_Cast @0x004767a0 passes oCSpell::GetLevel()
    // (@0x00486620, field 0x4c) to Spell_Cast_<tag>(var int level). That field is initialized to
    // 1 by oCSpell::InitByScript @0x00484550 and incremented per SPL_NEXTLEVEL in oCSpell::Invest
    // @0x0048525e, so the script level is 1-based. castLevel is in the CS_Emit_* range here, so
    // the 1-based level is castLevel-CS_Emit_0+1 -- matching the shoot-damage path below and
    // activeSpellLevel() (Npc_GetActiveSpellLevel), which were already 1-based.
    const int32_t splLevel = int(castLevel) - int(CS_Emit_0) + 1;
    owner.script().invokeSpell(*this,currentTarget,*active,splLevel);
    }

  if(active->isSpellShoot()) {
    const int lvl = (castLevel-CS_Emit_0)+1;
    DamageCalculator::Damage dmg={};
    // NOTE: in original-game ApplyDamages (Gothic2.exe 0x0065e5a0, from oCVisualFX::ProcessCollision
    // 0x004958d0) the spell's total damage is split equally across its selected damage-type bits:
    // each element gets round(total/numTypes) (fild;fdiv;fadd 0.5;__ftol @0x0065e76c). OpenGothic
    // wrote the full total into every element, dealing N-times damage for a multi-element spell.
    int32_t splTypes = 0;
    for(size_t i=0; i<zenkit::DamageType::NUM; ++i)
      if((spl.damage_type&(1<<i))!=0)
        ++splTypes;
    const int32_t perType = (splTypes>0)
      ? int32_t(float(spl.damage_per_level*lvl)/float(splTypes) + 0.5f) : 0;
    for(size_t i=0; i<zenkit::DamageType::NUM; ++i)
      if((spl.damage_type&(1<<i))!=0) {
        dmg[i] = perType;
        }

    auto& b = owner.shootSpell(*active, *this, currentTarget);
    b.setDamage(dmg);
    b.setOrigin(this);
    b.setTarget(nullptr);
    visual.setMagicWeaponKey(owner,SpellFxKey::Init);
    } else {
    // NOTE: use pfx_ppsIsLoopingChg ?
    const VisualFx* vfx = owner.script().spellVfx(splId);
    if(vfx!=nullptr) {
      auto e = Effect(*vfx,owner,Vec3(x,y,z),SpellFxKey::Cast);
      e.setOrigin(this);
      e.setTarget((currentTarget==nullptr) ? this : currentTarget);
      e.setSpellId(splId,owner);
      e.setActive(true);
      visual.startEffect(owner,std::move(e),0,true);
      }
    visual.setMagicWeaponKey(owner,SpellFxKey::Init);
    if(currentTarget!=nullptr) {
      currentTarget->lastHitSpell = splId;
      currentTarget->perceptionProcess(*this,nullptr,0,PERC_ASSESSMAGIC);
      }
    }

  if(active->isSpell()) {
    size_t cnt = active->count();
    invent.delItem(active->clsId(),1,*this);
    if(cnt<=1) {
      Item* spl = nullptr;
      for(uint8_t i=0;i<8;++i) {
        if(auto s = invent.currentSpell(i)) {
          spl = s;
          break;
          }
        }
      if(spl==nullptr) {
        if(spellInfo==0)
          aiPush(AiQueue::aiRemoveWeapon());
        } else {
        drawSpell(spl->spellId());
        }
      }
    }

  if(spellInfo!=0 && transformSpl==nullptr) {
    transformSpl.reset(new TransformBack(*this));
    invent.updateView(*this);
    visual.clearOverlays();

    owner.script().initializeInstanceNpc(hnpc, size_t(spellInfo));
    spellInfo  = 0;
    // NOTE: in original-game oCSpell::CastSpecificSpell @0x00486960 the transform path calls
    // oCNpc::CopyTransformSpellInvariantValuesTo @0x0073d3d0, which keeps exp/exp_next/level/lp
    // (oCNpc offsets 0x234/0x42c/0x430/0x434, confirmed via OpenScreen_Status @0x0073d980)
    // invariant across the transform -- not just level. initializeInstanceNpc re-ran the
    // creature instance ctor, zeroing exp/exp_next/lp; OpenGothic restored only level, so the
    // status screen and XP/level math were wrong while transformed and TransformBack::undo then
    // carried the corrupted base back onto the character.
    hnpc->exp      = transformSpl->hnpc->exp;
    hnpc->exp_next = transformSpl->hnpc->exp_next;
    hnpc->lp       = transformSpl->hnpc->lp;
    hnpc->level    = transformSpl->hnpc->level;
    }
  }

const Npc::Routine& Npc::currentRoutine(bool assertWp) const {
  // find routine for current time
  // if there is no such routine search counter clock-wise until one is found
  auto time = owner.time().timeInDay();
  for(auto& i:routines) {
    if(assertWp && i.point==nullptr)
      continue;
    if(i.end<i.start && (time<i.end || i.start<=time))
      return i;
    if(i.start<=time && time<i.end)
      return i;
    }

  // NOTE: in original-game oCRtnManager::FindRoutine, when no routine window contains the
  // current time, the fallback is the last entry in the start-sorted list -- i.e. the entry
  // with the largest start time -- not the routine whose end most recently passed.
  // OpenGothic does not sort `routines`, so pick the max-start entry explicitly.
  const Routine* rtn = nullptr;
  for(auto& i:routines) {
    if(assertWp && i.point==nullptr)
      continue;
    if(rtn==nullptr || rtn->start<i.start)
      rtn = &i;
    }

  if(rtn!=nullptr)
    return *rtn;
  static Routine r;
  return r;
  }

const WayPoint* Npc::currentTaPoint() const {
  if(routines.empty())
    return owner.findPoint(hnpc->wp,false);
  return currentRoutine(true).point;
  }

gtime Npc::endTime(const Npc::Routine &r) const {
  auto wtime = owner.time();
  auto time  = wtime.timeInDay();

  //NOTE: should we consider time extension for invalid routine sequences?
  if(r.end<r.start) {
    if(time<r.end)
      return gtime(wtime.day(),r.end.hour(),r.end.minute());
    return gtime(wtime.day()+1,r.end.hour(),r.end.minute());
    }
  if(r.start<r.end) {
    if(r.end.hour()==0 || r.end<time)
      return gtime(wtime.day()+1,r.end.hour(),r.end.minute()); else
      return gtime(wtime.day(),r.end.hour(),r.end.minute());
    }
  if(r.start==r.end) {
    // NOTE: in original-game a routine slot with start==end (the common all-day wrappers
    // TA_Stand_Guarding(8,0,8,0) / TA_Smalltalk(8,0,8,0), plus the all-zeros Rtn_Start_1081 in NTR)
    // is NOT an error: oCWorldTimer::IsTimeBetween @0x00781190 reports it active at the start minute
    // and oCRtnManager::FindRoutine @0x00775580 keeps it as the active/fallback routine for the rest
    // of the day, so the state runs continuously. Treat it as a full-day window (same time next day).
    // Previously only the all-zeros case was handled; a nonzero start==end slot fell through to
    // `return wtime` (now), which forced an immediate LOOP_END and restarted the state every AI tick.
    return gtime(wtime.day()+1,r.end.hour(),r.end.minute());
    }
  // error - routine is not active now
  return wtime;
  }

BodyState Npc::bodyState() const {
  if(isDead())
    return BS_DEAD;
  if(isUnconscious())
    return BS_UNCONSCIOUS;
  if(isFalling())
    return BS_FALL;

  uint32_t s = visual.pose().bodyState();
  if(auto i = interactive())
    s = i->stateMask();
  return BodyState(s);
  }

BodyState Npc::bodyStateMasked() const {
  BodyState bs = bodyState();
  return BodyState(bs & (BS_MAX | BS_FLAG_MASK));
  }

bool Npc::hasState(BodyState s) const {
  if(visual.pose().hasState(s))
    return true;
  if(auto i = interactive())
    return s==i->stateMask();
  return false;
  }

bool Npc::hasStateFlag(BodyState flg) const {
  if(visual.pose().hasStateFlag(flg))
    return true;
  if(auto i = interactive())
    return flg==(i->stateMask() & (BS_FLAG_MASK|BS_MOD_MASK));
  return false;
  }

void Npc::setToFightMode(const size_t item) {
  if(invent.itemCount(item)==0)
    addItem(item,1);

  invent.equip(item,*this,true);
  invent.switchActiveWeapon(*this,1);

  auto w = invent.currentMeleeWeapon();
  if(w==nullptr || w->clsId()!=item)
    return;

  auto weaponSt = WeaponState::W1H;
  if(w->is2H()) {
    weaponSt = WeaponState::W2H;
    } else {
    weaponSt = WeaponState::W1H;
    }

  if(visual.setToFightMode(weaponSt))
    updateWeaponSkeleton();

  auto& weapon = *currentMeleeWeapon();
  auto  st     = weapon.is2H() ? WeaponState::W2H : WeaponState::W1H;
  hnpc->weapon  = (st==WeaponState::W1H ? 3:4);
  }

void Npc::setToFistMode() {
  auto weaponSt=weaponState();
  if(weaponSt==WeaponState::Fist)
    return;
  invent.switchActiveWeaponFist();
  if(visual.setToFightMode(WeaponState::Fist))
    updateWeaponSkeleton();
  hnpc->weapon  = 1;
  }

void Npc::aiPush(AiQueue::AiAction&& a) {
  if(a.act==AI_OutputSvmOverlay)
    aiQueueOverlay.pushBack(std::move(a)); else
    aiQueue.pushBack(std::move(a));
  }

void Npc::resumeAiRoutine() {
  auto& r = currentRoutine();
  if(r.callback.isValid() && aiState.funcIni==r.callback) {
    // NOTE: in original-game oCNpc_States::ActivateRtnState @0x0076c330 the resume path (force-flag
    // 0, reached via AI_ContinueRoutine -> EV_DoState @0x00756600 -> StartRtnState @0x0076c2e0)
    // returns success WITHOUT re-entering when the active AI-state already equals the daily-routine
    // state. OpenGothic unconditionally clearState(false)+startState, so re-issuing
    // Npc_ContinueRoutine while already in the routine state re-ran ZS_<routine>_End then a fresh
    // _Ini, re-equipping item-states and restarting the goto/ambient anim. No-op when already in it.
    if(!r.wayPointName().empty())
      hnpc->wp = r.wayPointName();
    return;
    }
  clearState(false);
  if(r.callback.isValid()) {
    auto t = endTime(r);
    startState(r.callback,r.wayPointName(),t,false);
    }
  }

Item* Npc::addItem(const size_t item, size_t count) {
  return invent.addItem(item,count,owner);
  }

Item* Npc::addItem(std::unique_ptr<Item>&& i) {
  return invent.addItem(std::move(i));
  }

Item* Npc::takeItem(Item& item) {
  if(interactive()!=nullptr)
    return nullptr;
  if(item.isTorchBurn() && (isUsingTorch() || weaponState()!=WeaponState::NoWeapon))
    return nullptr;

  auto state = bodyStateMasked();
  if(state!=BS_STAND && state!=BS_SNEAK && state!=BS_SWIM && state!=BS_DIVE) {
    return nullptr;
    }

  const auto  dpos = item.midPosition()-centerPosition();
  const auto* sq   = setAnimAngGet(Npc::Anim::ItmGet, Pose::calcAniCombVert(dpos));
  if(sq==nullptr)
    return nullptr;

  std::unique_ptr<Item> ptr = owner.takeItem(item);
  if(ptr!=nullptr && ptr->isTorchBurn()) {
   if(!toggleTorch())
     return nullptr;
    size_t torchId = owner.script().findSymbolIndex("ItLsTorch");
    if(torchId!=size_t(-1))
      return nullptr;
    ptr.reset(new Item(owner,torchId,Item::T_Inventory));
    }

  auto it = ptr.get();
  if(it==nullptr)
    return nullptr;

  it = addItem(std::move(ptr));
  if(isPlayer() && it!=nullptr)
    owner.sendPassivePerc(*this,*this,*it,PERC_ASSESSTHEFT);

  implAniWait(uint64_t(sq->totalTime()));
  return it;
  }

void Npc::onWldItemRemoved(const Item& itm) {
  aiQueue.onWldItemRemoved(itm);
  aiQueueOverlay.onWldItemRemoved(itm);
  }

void Npc::addItem(size_t id, Interactive &chest, size_t count) {
  Inventory::transfer(invent,chest.inventory(),nullptr,id,count,owner);
  }

void Npc::addItem(size_t id, Npc &from, size_t count) {
  Inventory::transfer(invent,from.invent,&from,id,count,owner);
  }

void Npc::moveItem(size_t id, Interactive &to, size_t count) {
  Inventory::transfer(to.inventory(),invent,this,id,count,owner);
  }

void Npc::sellItem(size_t id, Npc &to, size_t count) {
  if(id==owner.script().goldId()->index())
    return;
  int32_t price = invent.sellPriceOf(id);
  count = Inventory::transfer(to.invent,invent,this,id,count,owner);
  invent.addItem(owner.script().goldId()->index(),size_t(price)*count,owner);
  }

void Npc::buyItem(size_t id, Npc &from, size_t count) {
  if(id==owner.script().goldId()->index())
    return;

  int32_t price = from.invent.priceOf(id);
  if(price>0 && size_t(price)*count>invent.goldCount()) {
    count = invent.goldCount()/size_t(price);
    // NOTE: in original-game oCViewDialogTrade::OnTransferRight @0x0068bb40 every requested unit the
    // player cannot pay for raises PLAYER_TRADE_NOT_ENOUGH_GOLD, even when the affordable units are
    // still bought; a partial-stack buy must warn too, not only the buy-nothing case.
    if(count!=0)
      owner.script().printCannotBuyError(*this);
    }
  if(count==0) {
    owner.script().printCannotBuyError(*this);
    return;
    }

  count = Inventory::transfer(invent,from.invent,nullptr,id,count,owner);
  if(price>=0)
    invent.delItem(owner.script().goldId()->index(),size_t( price)*count,*this); else
    invent.addItem(owner.script().goldId()->index(),size_t(-price)*count,owner);
  }

void Npc::dropItem(size_t id, size_t count) {
  if(id==size_t(-1))
    return;
  size_t cnt = invent.itemCount(id);
  if(count>cnt)
    count = cnt;
  if(count<1)
    return;

  auto sk = visual.visualSkeleton();
  if(sk==nullptr)
    return;

  size_t rightHand = sk->findNode("ZS_RIGHTHAND");
  if(rightHand==size_t(-1))
    return;

  if(!setAnim(Anim::ItmDrop))
    return;

  auto mat = visual.transform();
  if(rightHand<visual.pose().boneCount())
    mat = visual.pose().bone(rightHand);

  // NOTE: in original-game oCAIVobMove::Init @0x0069f540 (from oCNpc::DoDropVob @0x00744dd0)
  // the dropped item's rotation is reset to identity via zCVob::ResetRotationsWorld @0x0061c000
  // before physics is enabled; only the world position is taken from the hand. Keep the
  // translation, drop the hand-bone rotation, so the item starts world-axis-aligned.
  Tempest::Matrix4x4 drop;
  drop.identity();
  drop.translate(mat.at(3,0),mat.at(3,1),mat.at(3,2));

  auto it = owner.addItemDyn(id,drop,hnpc->symbol_index());
  it->setCount(count);
  invent.delItem(id,count,*this);
  }

void Npc::clearInventory() {
  invent.clear(owner.script(),*this);
  }

Item* Npc::currentArmor() {
  return invent.currentArmor();
  }

Item* Npc::currentMeleeWeapon() {
  return invent.currentMeleeWeapon();
  }

Item* Npc::currentRangedWeapon() {
  return invent.currentRangedWeapon();
  }

Item* Npc::currentShield() {
  return invent.currentShield();
  }

Vec3 Npc::mapWeaponBone() const {
  return visual.mapWeaponBone();
  }

Vec3 Npc::mapHeadBone() const {
  return visual.mapHeadBone();
  }

Vec3 Npc::mapBone(std::string_view bone) const {
  if(auto sk = visual.visualSkeleton()) {
    size_t id = sk->findNode(bone);
    if(id!=size_t(-1))
      return visual.mapBone(id);
    }

  Vec3 ret = {};
  ret.y = physic.centerY()-y;
  return ret+position();
  }

bool Npc::turnTo(float dx, float dz, bool noAnim, uint64_t dt) {
  return implTurnTo(dx,dz,noAnim?AnimationSolver::TurnType::None:AnimationSolver::TurnType::Std,dt);
  }

bool Npc::rotateTo(float dx, float dz, float step, AnimationSolver::TurnType anim, uint64_t dt) {
  //step *= (float(dt)/1000.f)*60.f/100.f;
  step *= (float(dt)/1000.f);

  if(dx==0.f && dz==0.f) {
    setAnimRotate(0);
    return false;
    }

  if(!isRotationAllowed())
    return false;

  float a  = angleDir(dx,dz);
  float da = a-angle;

  if(anim == AnimationSolver::TurnType::None || std::cos(double(da)*M_PI/180.0)>0) {
    if(float(std::abs(int(da)%360))<=(step*2.f)) {
      setAnimRotate(0);
      setDirection(a);
      return false;
      }
    } else {
    visual.stopWalkAnim(*this);
    }

  const auto sgn = std::sin(double(da)*M_PI/180.0);
  if(sgn==0) {
    setAnimRotate(0);
    } else {
    const int rot = (sgn<0) ? +1 : -1;
    switch(anim) {
      case AnimationSolver::TurnType::Std:
        setAnimRotate(rot);
        break;
      case AnimationSolver::TurnType::None:
        setAnimRotate(0);
        break;
      case AnimationSolver::TurnType::Whirl:
        visual.setAnimWhirl(*this, rot);
        break;
      }
    setDirection(angle - float(rot)*step);
    }
  return true;
  }

bool Npc::isRotationAllowed() const {
  auto bs  = bodyStateMasked();
  bool air = (!isPlayer() && isInAir()) || isFallingDeep();
  return currentInteract==nullptr && !isFinishingMove() && bs!=BS_CLIMB && bs!=BS_LIE && !air;
  }

bool Npc::checkGoToNpcdistance(const Npc &other) {
  return fghAlgo.isInAttackRange(*this,other,owner.script());
  }

size_t Npc::itemCount(size_t id) const {
  return invent.itemCount(id);
  }

Item* Npc::activeWeapon() {
  return invent.activeWeapon();
  }

Item *Npc::getItem(size_t id) {
  return invent.getItem(id);
  }

void Npc::delItem(size_t item, uint32_t amount) {
  invent.delItem(item,amount,*this);
  }

void Npc::useItem(size_t item) {
  useItem(item,Item::NSLOT,false);
  }

void Npc::useItem(size_t item, uint8_t slotHint, bool force) {
  invent.use(item,*this,slotHint,force);
  }

void Npc::setCurrentItem(size_t item) {
  invent.setCurrentItem(item);
  }

void Npc::unequipItem(size_t item) {
  invent.unequip(item,*this);
  }

bool Npc::canSwitchWeapon() const {
  if(isUnconscious())
    return false;
  // NOTE: in original-game oCNpc::CanDrawWeapon (Gothic2.exe 0x006805c0) permits a weapon
  // draw during a mob interaction (GetInteractMob()!=NULL); the draw helpers exit the mobsi.
  // OG's body-state allow-list omits BS_MOBINTERACT, so drawing at a forge/bench was ignored.
  if(interactive()!=nullptr)
    return true;
  // NOTE: in original-game oCNpc::CanDrawWeapon (Gothic2.exe 0x006805c0) also returns true when
  // GetWeaponMode()==5 (bow) / ==6 (crossbow): a readied ranged weapon can always be switched, even
  // from non-stand/walk states. IsStanding/IsWalking are false during the aim ani (BS_AIMNEAR/
  // BS_AIMFAR), so without this clause OG could not switch off (or holster-redraw) a drawn bow while
  // aiming -- every drawWeapon*/AI_DrawWeapon* path silently no-op'd.
  auto ws = weaponState();
  if(ws==WeaponState::Bow || ws==WeaponState::CBow)
    return true;
  auto bs = bodyStateMasked();
  if(bs==BS_STAND || bs==BS_WALK || bs==BS_RUN || bs==BS_SNEAK || bs==BS_NONE)
    return true;
  return false;
  // return !(mvAlgo.isFalling() || mvAlgo.isInAir() || mvAlgo.isSlide() || mvAlgo.isSwim());
  }

bool Npc::closeWeapon(bool noAnim) {
  auto weaponSt=weaponState();
  if(weaponSt==WeaponState::NoWeapon)
    return true;
  if(!noAnim && !visual.startAnim(*this,WeaponState::NoWeapon))
    return false;
  visual.setAnimRotate(*this,0);
  if(isPlayer())
    setTarget(nullptr);
  invent.switchActiveWeapon(*this,Item::NSLOT);
  invent.putAmmunition(*this,0,"");
  if(noAnim) {
    visual.setToFightMode(WeaponState::NoWeapon);
    updateWeaponSkeleton();
    }
  hnpc->weapon      = 0;
  // clear spell-cast state
  castLevel        = CS_NoCast;
  currentSpellCast = size_t(-1);
  castNextTime     = 0;
  if(isPlayer())
    owner.sendPassivePerc(*this,*this,PERC_ASSESSREMOVEWEAPON);
  return true;
  }

bool Npc::drawWeaponFist() {
  if(!canSwitchWeapon())
    return false;
  auto weaponSt=weaponState();
  if(weaponSt==WeaponState::Fist)
    return true;
  if(weaponSt!=WeaponState::NoWeapon) {
    closeWeapon(false);
    return false;
    }

  if(isMonster()) {
    if(!visual.startAnim(*this,WeaponState::Fist))
      visual.setToFightMode(WeaponState::Fist);
    } else {
    if(!visual.startAnim(*this,WeaponState::Fist))
      return false;
    }

  invent.switchActiveWeaponFist();
  hnpc->weapon = 1;
  return true;
  }

bool Npc::drawWeaponMelee() {
  if(!canSwitchWeapon())
    return false;
  auto weaponSt=weaponState();
  if(weaponSt==WeaponState::Fist || weaponSt==WeaponState::W1H || weaponSt==WeaponState::W2H)
    return true;
  if(invent.currentMeleeWeapon()==nullptr)
    return drawWeaponFist();
  if(weaponSt!=WeaponState::NoWeapon) {
    closeWeapon(false);
    return false;
    }

  if(!setInteraction(nullptr,true))
    return false;

  auto& weapon = *invent.currentMeleeWeapon();
  auto  st     = weapon.is2H() ? WeaponState::W2H : WeaponState::W1H;
  if(!visual.startAnim(*this,st))
    return false;

  invent.switchActiveWeapon(*this,1);
  hnpc->weapon = (st==WeaponState::W1H ? 3:4);
  return true;
  }

bool Npc::drawWeaponBow() {
  if(!canSwitchWeapon())
    return false;
  auto weaponSt=weaponState();
  if(weaponSt==WeaponState::Bow || weaponSt==WeaponState::CBow || invent.currentRangedWeapon()==nullptr)
    return true;
  if(weaponSt!=WeaponState::NoWeapon) {
    closeWeapon(false);
    return false;
    }

  if(!setInteraction(nullptr,true))
    return false;

  auto& weapon = *invent.currentRangedWeapon();
  auto  st     = weapon.isCrossbow() ? WeaponState::CBow : WeaponState::Bow;
  if(!visual.startAnim(*this,st))
    return false;
  invent.switchActiveWeapon(*this,2);
  hnpc->weapon = (st==WeaponState::Bow ? 5:6);
  return true;
  }

bool Npc::drawMage(uint8_t slot) {
  if(!canSwitchWeapon())
    return false;
  Item* it = invent.currentSpell(uint8_t(slot-3));
  if(it==nullptr) {
    closeWeapon(false);
    return true;
    }
  return drawSpell(it->spellId());
  }

bool Npc::drawSpell(int32_t spell) {
  if(mvAlgo.isFalling() || mvAlgo.isSwim() || bodyStateMasked()==BS_CASTING)
    return false;
  auto weaponSt=weaponState();
  if(weaponSt!=WeaponState::NoWeapon && weaponSt!=WeaponState::Mage) {
    closeWeapon(false);
    return false;
    }

  if(!setInteraction(nullptr,true))
    return false;

  if(!visual.startAnim(*this,WeaponState::Mage))
    return false;

  invent.switchActiveSpell(spell,*this);
  hnpc->weapon = 7;

  updateWeaponSkeleton();
  return true;
  }

WeaponState Npc::weaponState() const {
  return visual.fightMode();
  }

bool Npc::canFinish(Npc& oth) {
  auto ws = weaponState();
  if(ws!=WeaponState::W1H && ws!=WeaponState::W2H)
    return false;

  if(!oth.isUnconscious())
    return false;

  if(!fghAlgo.isInFinishRange(*this,oth,owner.script()))
    return false;
  return true;
  }

bool Npc::doAttack(Anim anim, BodyState bs) {
  auto weaponSt = weaponState();
  if(weaponSt==WeaponState::NoWeapon || weaponSt==WeaponState::Mage)
    return false;

  if(mvAlgo.isSwim())
    return false;

  if(bs==BS_PARADE && hasState(BS_PARADE))
    return false;

  auto wlk = walkMode();
  if(mvAlgo.isInWater())
    wlk = WalkBit::WM_Water;

  visual.setAnimRotate(*this,0);
  if(auto sq = visual.continueCombo(*this,anim,bs,weaponSt,wlk)) {
    (void)sq;
    // implAniWait(uint64_t(sq->atkTotalTime(visual.comboLength())+1));
    return true;
    }
  return false;
  }

void Npc::fistShoot() {
  doAttack(Anim::Attack,BS_HIT);
  }

bool Npc::blockFist() {
  auto weaponSt=weaponState();
  if(weaponSt!=WeaponState::Fist)
    return false;
  visual.setAnimRotate(*this,0);
  return setAnim(Anim::AttackBlock);
  }

bool Npc::finishingMove() {
  if(currentTarget==nullptr || !canFinish(*currentTarget))
    return false;

  if(doAttack(Anim::AttackFinish,BS_HIT)) {
    currentTarget->hnpc->attribute[ATR_HITPOINTS] = 0;
    currentTarget->checkHealth(true,false);
    owner.sendPassivePerc(*this,*this,*currentTarget,PERC_ASSESSMURDER);
    return true;
    }
  return false;
  }

void Npc::swingSword() {
  auto active=invent.activeWeapon();
  if(active==nullptr)
    return;
  doAttack(Anim::Attack,BS_HIT);
  }

bool Npc::swingSwordL() {
  auto active=invent.activeWeapon();
  if(active==nullptr)
    return false;
  return doAttack(Anim::AttackL,BS_HIT);
  }

bool Npc::swingSwordR() {
  auto active=invent.activeWeapon();
  if(active==nullptr)
    return false;
  return doAttack(Anim::AttackR,BS_HIT);
  }

bool Npc::blockSword() {
  auto active=invent.activeWeapon();
  if(active==nullptr)
    return false;
  return doAttack(Anim::AttackBlock,BS_PARADE);
  // return setAnimAngGet(Anim::AttackBlock,calcAniComb())!=nullptr;
  }

Npc::BeginCastResult Npc::beginCastSpell() {
  if(castLevel!=CS_NoCast)
    return BeginCastResult::BC_No;

  auto bs = bodyStateMasked();
  if(bs!=BS_STAND)
    return BeginCastResult::BC_No;

  auto active=invent.activeWeapon();
  if(active==nullptr)
    return BeginCastResult::BC_No;

  setAnimRotate(0);
  // NOTE: in original-game oCNpc::EV_CastSpell @0x0067fb20 has no engine-side mana<=0 pre-gate;
  // the no-mana decision is left to the spell script. Scrolls (oCItem::MultiSlot, consumed by
  // oCMag_Book::Spell_Cast @0x004767a0) bypass the mana-invest stage and cast via SPL_SENDCAST
  // even at 0 mana -- that is the defining scroll mechanic. invokeMana() below already returns the
  // correct status for runes, so skip the redundant pre-gate for scrolls (Item::isSpell() = the
  // multi-slot scroll predicate) to avoid wrongly refusing a 0-mana scroll cast.
  if(attribute(ATR_MANA)<=0 && !active->isSpell()) {
    setAnim(Anim::MagNoMana);
    return BeginCastResult::BC_NoMana;
    }

  // castLevel        = CS_Invest_0;
  currentSpellCast = active->clsId();
  castNextTime     = owner.tickCount();
  hnpc->aivar[88]  = 0; // HACK: clear AIV_SpellLevel
  manaInvested     = 0;

  const SpellCode code = SpellCode(owner.script().invokeMana(*this,currentTarget,manaInvested));
  switch(code) {
    case SPL_SENDSTOP:
    case SPL_DONTINVEST:
      setAnim(Anim::MagNoMana);
      castLevel        = CS_NoCast;
      currentSpellCast = size_t(-1);
      castNextTime     = 0;
      return BeginCastResult::BC_NoMana;
    case SPL_STATUS_CANINVEST_NO_MANADEC:
    case SPL_RECEIVEINVEST:
    case SPL_NEXTLEVEL: {
      ++manaInvested;
      auto ani = owner.script().spellCastAnim(*this,*active);
      if(!visual.startAnimSpell(*this,ani,true))
        Log::d("Couldn't start animation for spell '",currentSpellCast,"'");
      castLevel = CS_Invest_0;
      // NOTE: in original-game oCAIHuman::MagicInvestSpell @0x00472160 -> oCNpc::AssessCaster_S
      // @0x0075d200 -> CreatePassivePerception @0x0075b270 broadcasts PERC_ASSESSCASTER (0x1d) to
      // nearby NPCs whenever a caster (NPC or player) is investing a spell, with OTHER=the caster and
      // no VICTIM. OpenGothic never sent PERC_ASSESSCASTER, so bystanders could not react to a spell
      // being charged near them. (The original re-sends each invest cycle; broadcast once at
      // invest-start here -- receivers that did not Npc_PercEnable it are filtered out by hasPerc.)
      owner.sendPassivePerc(*this,*this,PERC_ASSESSCASTER);
      return BeginCastResult::BC_Invest;
      }
    case SPL_SENDCAST: {
      castLevel = CS_Cast_0;
      return BeginCastResult::BC_Cast;
      }
    default:
      Log::d("unexpected Spell_ProcessMana result: '",int(code),"' for spell '",currentSpellCast,"'");
      endCastSpell();
      return BeginCastResult::BC_No;
    }

  return BeginCastResult::BC_No;
  }

bool Npc::tickCast(uint64_t dt) {
  if(castLevel==CS_NoCast)
    return false;

  auto active = currentSpellCast!=size_t(-1) ? invent.getItem(currentSpellCast) : nullptr;

  if(currentSpellCast!=size_t(-1)) {
    if(active==nullptr || !active->isSpellOrRune() || isDown()) {
      // canot cast spell
      castLevel        = CS_NoCast;
      currentSpellCast = size_t(-1);
      castNextTime     = 0;
      return true;
      }

    if(!isPlayer() && currentTarget!=nullptr) {
      // NOTE: in original-game oCAIHuman::MagicMode @0x00472fd0 the caster turns toward its target
      // during channeling only when the spell's canTurnDuringInvest flag is set (oCSpell field 0x90,
      // gating the TurnToEnemy call). Spells authored with canTurnDuringInvest==0 (self-buffs /
      // non-aimed control spells) are cast without rotating; OpenGothic turned unconditionally.
      const auto& spl = owner.script().spellDesc(active->spellId());
      if(spl.can_turn_during_invest!=0)
        implTurnTo(*currentTarget,AnimationSolver::TurnType::None,dt);
      }
    }

  if(CS_Cast_0<=castLevel && castLevel<=CS_Cast_Last) {
    // cast anim
    if(active!=nullptr) {
      auto ani = owner.script().spellCastAnim(*this,*active);
      bool g2  = owner.version().game==2;
      if(g2 || visual.hasAnim(string_frm("T_MAGRUN_2_",ani,"CAST")))
        if(!visual.startAnimSpell(*this,ani,false))
          return true;
      }
    castLevel    = CastState(int(castLevel) + int(CS_Emit_0) - int(CS_Cast_0));
    castNextTime = 0;
    return true;
    }

  if((CS_Emit_0<=castLevel && castLevel<=CS_Emit_Last) || castLevel==CS_Finalize) {
    // final commit
    if(!setAnim(Npc::Anim::Idle))
      return true;
    if(castLevel!=CS_Finalize)
      commitSpell();
    castLevel        = CS_NoCast;
    currentSpellCast = size_t(-1);
    castNextTime     = 0;
    spellInfo        = 0;
    return false;
    }

  if(active==nullptr)
    return false;

  if(bodyStateMasked()!=BS_CASTING)
    return true;

  if(owner.tickCount()<castNextTime)
    return true;

  const SpellCode code = SpellCode(owner.script().invokeMana(*this,currentTarget,manaInvested));

  if(owner.version().game==1) {
    changeAttribute(ATR_MANA,-1,false);
    if(!isPlayer() && code!=SpellCode::SPL_SENDCAST)
      assert(attribute(ATR_MANA)>0);
    }

  // NOTE: in original-game oCNpc::EV_CastSpell @0x0067fb20 an NPC keeps investing while
  // aiExpectedInvest >= manaInvested and releases only once manaInvested has *exceeded*
  // aiExpectedInvest (strict `<`: branch at *(npc+0x574) < *(oCSpell+0x48), 0x574=aiExpectedInvest
  // from oCNpc::ReadySpell @0x006802e0, 0x48=manaInvested from oCSpell::Invest @0x004850d0). Using
  // `<=` released one invest tick early, so NPC spells were cast one invest level too low.
  if(!isPlayer() && aiExpectedInvest<manaInvested) {
    endCastSpell();
    return true;
    }

  switch(code) {
    case SpellCode::SPL_NEXTLEVEL:
    case SpellCode::SPL_RECEIVEINVEST:
    case SpellCode::SPL_STATUS_CANINVEST_NO_MANADEC: {
      if(code==SPL_NEXTLEVEL) {
        int32_t castLvl = int(castLevel)-int(CS_Invest_0);
        if(castLvl<15)
          castLevel = CastState(castLevel+1);
        visual.setMagicWeaponKey(owner,SpellFxKey::Invest,castLvl+1);
        }
      // NOTE: in original-game oCSpell::Invest @0x004850d0, when Spell_ProcessMana returns
      // SPL_RECEIVEINVEST and at least one mana was already invested (oCSpell+0x48 != 0), the engine
      // itself drains one mana from the caster: oCNpc::ChangeAttribute(caster,ATR_MANA,-1) @0x0072ff60.
      // SPL_NEXTLEVEL and SPL_STATUS_CANINVEST_NO_MANADEC do NOT drain (the latter's name means "no
      // mana-dec": the opt-out a script uses when it deducts itself). G1 drains above (line 4165); the
      // G2 RECEIVEINVEST path was missing, so G2 channeling/invest spells cost no mana. The first
      // invest is BeginCast (manaInvested==0, no drain), so here manaInvested is always >=1.
      if(owner.version().game==2 && code==SPL_RECEIVEINVEST && manaInvested>0)
        changeAttribute(ATR_MANA,-1,false);
      auto& spl = owner.script().spellDesc(active->spellId());
      castNextTime += uint64_t(spl.time_per_mana);
      ++manaInvested;
      return true;
      }
    case SpellCode::SPL_DONTINVEST:
    case SpellCode::SPL_SENDCAST:
    case SpellCode::SPL_SENDSTOP: {
      if(code==SPL_DONTINVEST && isPlayer())
        return true;
      endCastSpell();
      return true;
      }
    default:
      Log::d("unexpected Spell_ProcessMana result: '",int(code),"' for spell '",currentSpellCast,"'");
      return false;
    }
  return true;
  }

void Npc::endCastSpell(bool playerCtrl) {
  if(castLevel<CS_Invest_0 || castLevel>CS_Invest_Last)
    return;
  int32_t castLvl = int(castLevel)-int(CS_Invest_0);
  if(!playerCtrl) {
    castLevel = CastState(castLvl+CS_Cast_0);
    return;
    }
  SpellCode code = SpellCode(owner.script().invokeManaRelease(*this,currentTarget,manaInvested));
  if(code==SpellCode::SPL_SENDCAST)
    castLevel = CastState(castLvl+CS_Cast_0); else
    castLevel = CS_Finalize;
  }

void Npc::setActiveSpellInfo(int32_t info) {
  spellInfo = info;
  }

int32_t Npc::activeSpellLevel() const {
  if(CS_Cast_0<=castLevel && castLevel<=CS_Cast_Last)
    return int(castLevel)-int(CS_Cast_0)+1;
  if(CS_Invest_0<=castLevel && castLevel<=CS_Invest_Last)
    return int(castLevel)-int(CS_Invest_0)+1;
  return 0;
  }

bool Npc::aimBow() {
  auto active=invent.activeWeapon();
  if(active==nullptr)
    return false;
  auto bs = bodyStateMasked();
  if(bs!=BS_STAND && bs!=BS_AIMNEAR && bs!=BS_AIMFAR && bs!=BS_HIT) {
    setAnim(Anim::Idle);
    return false;
    }
  if(!setAnim(Anim::AimBow))
    return false;
  visual.setAnimRotate(*this,0);
  return true;
  }

bool Npc::shootBow(Interactive* focOverride) {
  auto active=invent.activeWeapon();
  if(active==nullptr)
    return false;

  auto bs = bodyStateMasked();
  if(bs!=BS_STAND && bs!=BS_AIMNEAR && bs!=BS_AIMFAR && bs!=BS_HIT) {
    setAnim(Anim::Idle);
    return true;
    }

  const int32_t munition = active->handle().munition;
  if(!hasAmmunition())
    return false;

  if(!setAnim(Anim::Attack))
    return false;

  auto itm = invent.getItem(size_t(munition));
  if(itm==nullptr)
    return false;

  auto& b = owner.shootBullet(*itm,*this,currentTarget,focOverride);

  invent.delItem(size_t(munition),1,*this);
  b.setOrigin(this);
  b.setDamage(DamageCalculator::rangeDamageValue(*this));

  auto rgn = currentRangedWeapon();
  if(Gothic::inst().version().game==1) {
    b.setHitChance(float(hnpc->attribute[ATR_DEXTERITY])/100.f);
    if(rgn!=nullptr && rgn->isCrossbow())
      b.setCritChance(float(talentsVl[TALENT_CROSSBOW])/100.f); else
      b.setCritChance(float(talentsVl[TALENT_BOW]     )/100.f);
    }
  else {
    if(rgn!=nullptr && rgn->isCrossbow())
      b.setHitChance(float(hnpc->hitchance[TALENT_CROSSBOW])/100.f); else
      b.setHitChance(float(hnpc->hitchance[TALENT_BOW]     )/100.f);
    }
  return true;
  }

bool Npc::hasAmmunition() const {
  auto active=invent.activeWeapon();
  if(active==nullptr)
    return false;
  const int32_t munition = active->handle().munition;
  if(munition<0 || invent.itemCount(size_t(munition))<=0)
    return false;
  return true;
  }

bool Npc::isEnemy(const Npc &other) const {
  return owner.script().personAttitude(*this,other)==ATT_HOSTILE;
  }

bool Npc::isDead() const {
  return owner.script().isDead(*this);
  }

bool Npc::isLie() const {
  return bodyStateMasked()==BS_LIE;
  }

bool Npc::isUnconscious() const {
  return owner.script().isUnconscious(*this);
  }

bool Npc::isDown() const {
  return isUnconscious() || isDead();
  }

bool Npc::isAttack() const {
  return owner.script().isAttack(*this);
  }

bool Npc::isTalk() const {
  return owner.script().isTalk(*this);
  }

bool Npc::isAttackAnim() const {
  return visual.pose().isAttackAnim();
  }

bool Npc::isPrehit() const {
  return visual.pose().isPrehit(owner.tickCount());
  }

bool Npc::isImmortal() const {
  return hnpc->flags & zenkit::NpcFlag::IMMORTAL;
  }

void Npc::setPerceptionTime(uint64_t time) {
  // NOTE: in original-game oCNpc::SetPerceptionTime (Gothic2.exe 0x0075dba0) also reduces the
  // pending perception wait so a freshly-lowered perc time takes effect this cycle: it clamps
  // the remaining wait to (remaining mod new-interval). OG left the already-armed deadline
  // untouched, so an NPC could wait out up to a full old interval before reacting.
  const uint64_t now = owner.tickCount();
  if(time>0 && perceptionNextTime>now) {
    uint64_t remaining = perceptionNextTime - now;
    if(remaining>time)
      perceptionNextTime = now + (remaining % time);
    }
  perceptionTime = time;
  }

uint64_t Npc::perceptionTimeClampt() const {
  return std::max<uint64_t>(perceptionTime, 1);
  }

void Npc::setPerceptionEnable(PercType t, size_t fn) {
  if(t>0 && t<PERC_Count)
    perception[t].func = fn;
  }

void Npc::setPerceptionDisable(PercType t) {
  if(t>0 && t<PERC_Count)
    perception[t].func = ScriptFn();
  }

void Npc::startDialog(Npc& pl) {
  if(pl.isDown() || pl.isInAir() || isPlayer())
    return;
  if(perceptionProcess(pl,nullptr,0,PERC_ASSESSTALK))
    setOther(&pl);
  }

bool Npc::perceptionProcess(Npc &pl) {
  static bool dbg = false;
  static int  kId = -1;
  if(dbg && hnpc->id!=kId)
    return false;

  if(isPlayer())
    return true;

  bool ret=false;
  if(processPolicy()!=NpcProcessPolicy::AiNormal) {
    perceptionNextTime = owner.tickCount()+perceptionTimeClampt();
    return ret;
    }

  const float quadDist = pl.qDistTo(*this);
  // NOTE: in original-game oCNpc::PerceptionCheck @0x0075dd30 a vob with hp<1 or IsUnconscious
  // (@0x00736750) is classified EXCLUSIVELY as a body (AssessBody); the player/enemy/fighter
  // (AssessPlayer) branch is the else-arm, so a downed player is never assessed via PERC_ASSESSPLAYER.
  // OpenGothic ran B_AssessPlayer (greet/turn-to/aggro) against a knocked-out or dead hero; gate it
  // on the same isDown() predicate the body/enemy branches already use.
  if(hasPerc(PERC_ASSESSPLAYER) && !pl.isDown() && canSenseNpc(pl,false)!=SensesBit::SENSE_NONE) {
    if(perceptionProcess(pl,nullptr,quadDist,PERC_ASSESSPLAYER)) {
      ret = true;
      }
    }

  Npc* enem=hasPerc(PERC_ASSESSENEMY) ? updateNearestEnemy() : nullptr;
  if(enem!=nullptr){
    float dist=qDistTo(*enem);
    if(perceptionProcess(*enem,nullptr,dist,PERC_ASSESSENEMY)){
      ret          = true;
      } else {
      nearestEnemy = nullptr;
      }
    }

  Npc* body=hasPerc(PERC_ASSESSBODY) ? updateNearestBody() : nullptr;
  if(body!=nullptr){
    float dist=qDistTo(*body);
    if(perceptionProcess(*body,nullptr,dist,PERC_ASSESSBODY)) {
      ret = true;
      }
    }

  // if(aiQueue.size()==0) // NOTE: Gothic1 fights
  perceptionNextTime = owner.tickCount()+perceptionTimeClampt();
  return ret;
  }

bool Npc::perceptionProcess(Npc &pl, Npc* victim, float quadDist, PercType perc) {
  if(!aiState.started && aiState.funcIni.isValid()) {
    // avoid ugly soft-lock (ZS_MM_Attack <-> B_MM_AssessWarn) for the orks near ramp
    return false;
    }

  float r = float(world().script().percRanges().at(perc, hnpc->senses_range));
  r = r*r;

  if(quadDist>r)
    return false;

  if(hasPerc(perc)) {
    owner.script().invokeState(this,&pl,victim,perception[perc].func);
    return true;
    }
  if(perc==PERC_ASSESSMAGIC && isPlayer()) {
    auto defaultFn = owner.script().playerPercAssessMagic();
    if(defaultFn.isValid())
      owner.script().invokeState(this,&pl,victim,defaultFn);
    return true;
    }
  return false;
  }

bool Npc::hasPerc(PercType perc) const {
  return perception[perc].func.isValid();
  }

uint64_t Npc::percNextTime() const {
  return perceptionNextTime;
  }

bool Npc::setInteraction(Interactive *id, bool quick) {
  if(currentInteract==id)
    return true;

  if(currentInteract!=nullptr) {
    return currentInteract->detach(*this,quick);
    }

  if(id==nullptr)
    return (currentInteract==nullptr);

  if(id->attach(*this)) {
    currentInteract = id;
    attachToPoint(nullptr); //NOTE: Fajeth campfire
    if(!quick) {
      visual.stopAnim(*this,"");
      setAnimRotate(0);
      }
    return true;
    }

  return false;
  }

void Npc::quitInteraction() {
  if(currentInteract==nullptr)
    return;
  if(invTorch)
    processDefInvTorch();
  setDirectionY(0);
  currentInteract=nullptr;
  }

void Npc::processDefInvTorch() {
  if(invTorch || isUsingTorch()) {
    visual.setTorch(invTorch,owner);
    invTorch = !invTorch;
    }
  }

void Npc::setDetectedMob(Interactive* id) {
  moveMob         = id;
  moveMobCacheKey = position();
  }

Interactive* Npc::detectedMob() const {
  if(currentInteract!=nullptr)
    return currentInteract;
  if((moveMobCacheKey-position()).quadLength()<10.f*10.f)
    return moveMob;
  return nullptr;
  }

bool Npc::isInState(ScriptFn stateFn) const {
  return aiState.funcIni==stateFn;
  }

bool Npc::isInRoutine(ScriptFn stateFn) const {
  // NOTE: in original-game Npc_IsInRoutine @0x006e51c0 the result is purely
  // GetLastRoutineState()==state (@0x0076e890): it inspects the active routine SLOT's state for
  // the current time and does NOT require the NPC to be executing that state. OpenGothic ANDed in
  // aiState.funcIni==stateFn, so an interrupting AI state (combat/dialog/perception) flipped the
  // answer to false, breaking the common `if(Npc_IsInRoutine(self, ZS_X))` schedule check.
  auto& rout = currentRoutine();
  return rout.callback==stateFn;
  }

bool Npc::wasInState(ScriptFn stateFn) const {
  return aiPrevState==stateFn;
  }

uint64_t Npc::stateTime() const {
  // NOTE: in original-game oCNpc_States::GetStateTime @0x0076c0a0 returns 0 unless an active script
  // state is loaded (states+0x34 != 0). OpenGothic reported the entire elapsed world time for a
  // stateless NPC. funcIni is invalid when no script state is active.
  if(!aiState.funcIni.isValid())
    return 0;
  return owner.tickCount()-aiState.sTime;
  }

void Npc::setStateTime(int64_t time) {
  // NOTE: in original-game oCNpc_States::SetStateTime @0x0076c0d0 is a no-op unless an active script
  // state is loaded (states+0x34 != 0).
  if(!aiState.funcIni.isValid())
    return;
  aiState.sTime = owner.tickCount()-uint64_t(time);
  }

void Npc::addRoutine(gtime s, gtime e, uint32_t callback, std::string_view point) {
  auto wp = world().findPoint(point,false);

  Routine r;
  r.start    = s;
  r.end      = e;
  r.callback = callback;
  r.point    = wp;
  if(wp==nullptr)
    r.fallbackName = point;
  routines.push_back(r);

  std::stable_sort(routines.begin(), routines.end(), [](const Routine& l, const Npc::Routine& r) {
    return l.start < r.start;
    });
  }

void Npc::excRoutine(size_t callback) {
  routines.clear();
  owner.script().invokeState(this,currentOther,currentVictim,callback);
  // aiState.eTime = gtime();
  }

void Npc::multSpeed(float s) {
  mvAlgo.multSpeed(s);
  }

bool Npc::testMove(const Vec3& pos) {
  DynamicWorld::CollisionTest out;
  return physic.testMove(pos,out);
  }

bool Npc::tryMove(const Vec3& dp) {
  DynamicWorld::CollisionTest out;
  return tryMove(dp, out);
  }

bool Npc::tryMove(const Vec3& dp, DynamicWorld::CollisionTest& out) {
  return tryTranslate(Vec3(x,y,z) + dp, out);
  }

bool Npc::tryTranslate(const Vec3& to) {
  DynamicWorld::CollisionTest out;
  return tryTranslate(to,out);
  }

bool Npc::tryTranslate(const Vec3& to, DynamicWorld::CollisionTest& out) {
  switch(physic.tryMove(to, out)) {
    case DynamicWorld::MoveCode::MC_Fail:
      return false;
    case DynamicWorld::MoveCode::MC_Partial:
      setViewPosition(out.partial);
      return true;
    case DynamicWorld::MoveCode::MC_Skip:
    case DynamicWorld::MoveCode::MC_OK:
      setViewPosition(to);
      return true;
    }
  return false;
  }

Npc::JumpStatus Npc::tryJump() {
  float len = MoveAlgo::climbMove;
  float rot = rotationRad();
  float s   = std::sin(rot), c = std::cos(rot);
  Vec3  dp  = Vec3{len*c, 0, len*s};

  auto& g  = owner.script().guildVal();
  auto  gl = guild();

  if(isSlide() || isSwim() || isDive()) {
    JumpStatus ret;
    ret.anim   = Anim::Idle;
    return ret;
    }

  const float stepH   = float(g.step_height[gl]);
  const float jumpLow = float(g.jumplow_height[gl]);
  const float jumpMid = float(g.jumpmid_height[gl]);
  const float jumpUp  = float(g.jumpup_height[gl]);

  auto pos0 = physic.position();

  JumpStatus ret;
  DynamicWorld::CollisionTest info;
  if(!mvAlgo.isJumpUp() && physic.testMove(pos0+dp,info)) {
    // jump forward
    ret.anim   = Anim::Jump;
    ret.noClimb = true;
    return ret;
    }

  auto  lnd   = owner.physic()->landRay(pos0 + dp + Vec3(0, jumpUp + jumpLow, 0));
  float jumpY = lnd.v.y;
  auto  pos1  = Vec3(pos0.x,jumpY,pos0.z);
  auto  pos2  = pos1 + dp;

  float dY    = jumpY - y;

  if(dY<=0.f ||
     !physic.testMove(pos2,pos1,info)) {
    ret.anim    = Anim::JumpUp;
    ret.height  = y + jumpUp;
    ret.noClimb = true;
    return ret;
    }

  if(!physic.testMove(pos1,pos0,info) ||
     !physic.testMove(pos2,pos1,info)) {
    // check approximate path of climb failed
    ret.anim    = Anim::JumpUp;
    ret.noClimb = true;
    return ret;
    }

  if(dY>=jumpUp || dY>=jumpMid) {
    // Jump to the edge, and then pull up. Height: 200-350cm
    ret.anim   = Anim::JumpUp;
    ret.height = y + jumpUp;
    return ret;
    }

  DynamicWorld::CollisionTest out;
  if(mvAlgo.testSlide(Vec3{pos0.x,jumpY,pos0.z}+dp,out)) {
    // cannot climb to non angled surface
    ret.anim    = Anim::Jump;
    ret.noClimb = true;
    return ret;
    }

  if(mvAlgo.isJumpUp() && dY<=jumpLow + visual.pose().translateY()) {
    // jumpup -> climb
    ret.anim   = Anim::JumpHang;
    ret.height = jumpY;
    return ret;
    }

  if(mvAlgo.isJumpUp()) {
    ret.anim    = Anim::Idle;
    ret.noClimb = true;
    return ret;
    }

  if(dY<=jumpLow) {
    // NOTE: in original-game oCAniCtrl_Human::JumpForward @0x006b21ed the JUMPUPLOW climb band
    // is (step_height, jumplow_height]; a ledge at or below step_height resolves to a plain
    // forward jump, not the hands-free climb-up-low pull. OpenGothic omitted the step_height
    // floor, so knee-high ledges played a spurious climb animation.
    if(dY<=stepH) {
      ret.anim    = Anim::Jump;
      ret.noClimb = true;
      return ret;
      }
    // Without using the hands, just big footstep. Height: 50-100cm
    ret.anim   = Anim::JumpUpLow;
    ret.height = jumpY;
    return ret;
    }

  if(dY<=jumpMid) {
    // Supported on the hands in one sentence. Height: 100-200cm
    ret.anim   = Anim::JumpUpMid;
    ret.height = jumpY;
    return ret;
    }

  return JumpStatus(); // error
  }

void Npc::startDive() {
  mvAlgo.startDive();
  }

void Npc::transformBack() {
  if(transformSpl==nullptr)
    return;
  transformSpl->undo(*this);
  setVisual(transformSpl->skeleton);
  setVisualBody(vHead,vTeeth,vColor,bdColor,body,head);
  closeWeapon(true);

  // invalidate tallent overlays
  for(size_t i=0; i<TALENT_MAX_G2; ++i)
    setTalentSkill(Talent(i),talentsSk[i]);

  invent.updateView(*this);
  transformSpl.reset();

  // NOTE: in original-game oCSpell::EndTimedEffect @0x00486e10 the final act of the transform revert
  // is oCNpc::CreatePassivePerception(self,0x1e,self,NULL) @0x0075b270 -- a PERC_ASSESSSURPRISE (30)
  // broadcast from the restored caster (OTHER=self, no VICTIM), so bystanders react with surprise
  // when a shapeshifter morphs back to its true form. OpenGothic restored the npc but never emitted
  // the perception.
  owner.sendPassivePerc(*this,*this,PERC_ASSESSSURPRISE);
  }

std::vector<GameScript::DlgChoice> Npc::dialogChoices(Npc& player,const std::vector<uint32_t> &except,bool includeImp) {
  return owner.script().dialogChoices(player.hnpc,this->hnpc,except,includeImp);
  }

bool Npc::isAiQueueEmpty() const {
  return aiQueue.size()==0 &&
         go2.empty() &&
         waitTime<owner.tickCount();
  }

bool Npc::isAiBusy() const {
  return !isAiQueueEmpty() ||
         aniWaitTime>=owner.tickCount() ||
         outWaitTime>=owner.tickCount();
  }

void Npc::clearAiQueue() {
  currentLookAt    = nullptr;
  currentLookAtNpc = nullptr;
  visual.setHeadRotation(0,0);

  aiQueue.clear();
  aiQueueOverlay.clear();
  aniWaitTime = 0;
  waitTime    = 0;
  faiWaitTime = 0;
  fghAlgo.onClearTarget();
  wayPath.clear();
  clearGoTo();
  }

void Npc::attachToPoint(const WayPoint *p) {
  currentFp     = p;
  currentFpLock = FpLock(currentFp);
  }

void Npc::clearGoTo() {
  if(!go2.empty()) {
    stopWalking();
    go2.clear();
    }
  }

void Npc::stopWalking() {
  if(setAnim(Anim::Idle))
    return;
  // hard stop
  stopWalkAnimation();
  }

void Npc::drawVobBox(DbgPainter& p) const {
  physic.debugDraw(p);

  p.setBrush(Tempest::Color(0,1,0));
  p.drawPoint(position());

  const auto cen = centerPosition();
  p.setBrush(Tempest::Color(0,0,1));
  p.drawPoint(cen);

  p.setPen(Tempest::Color(1,1,1));
  p.drawLine(cen, cen+Tempest::Vec3(0,25,0));
  p.setPen(Tempest::Color(1,1,0));
  p.drawLine(cen, cen+Tempest::Vec3(25,0,0));
  p.setPen(Tempest::Color(1,0.5f,0));
  p.drawLine(cen, cen+Tempest::Vec3(0,0,25));

  if(auto sk = visual.visualSkeleton()) {
    auto bbox = sk->bboxCol;

    auto tr = transform();
    tr.translate(0,visual.pose().translateY(),0);

    p.setPen(Color(1,0,0));
    p.drawObb(tr, bbox);
    }
  }

void Npc::drawVobRay(DbgPainter& p, const Npc& oth) const {
  const bool freeLos = true;
  const auto mid     = oth.physic.center();
  p.setPen(Color(0,1,0));

  if(canRayHitPoint(mid,freeLos)) {
    // mid of dead npc may endedup inside a wall; extra check for physical center
    p.drawLine(mapHeadBone(), mid);
    return;
    }
  if(oth.visual.visualSkeleton()==nullptr)
    return;
  if(oth.visual.visualSkeleton()->BIP01_HEAD==size_t(-1))
    return;
  auto head = oth.visual.mapHeadBone();
  if(canRayHitPoint(head,freeLos)) {
    p.drawLine(mapHeadBone(), head);
    return;
    }
  p.setPen(Color(1,0,0));
  p.drawLine(mapHeadBone(), head);
  }

bool Npc::canSeeNpc(const Npc &oth, bool freeLos) const {
  const auto mid = oth.physic.center();
  if(canRayHitPoint(mid,freeLos)) {
    // mid of dead npc may endedup inside a wall; extra check for physical center
    return true;
    }
  if(oth.visual.visualSkeleton()==nullptr)
    return false;
  if(oth.visual.visualSkeleton()->BIP01_HEAD==size_t(-1))
    return false;
  auto head = oth.visual.mapHeadBone();
  if(canRayHitPoint(head,freeLos))
    return true;
  return false;
  }

bool Npc::canSeeSource() const {
  const auto head = visual.mapHeadBone();
  const bool ret  = owner.sound()->canSeeSource(head);
  if(ret)
    return ret;
  if(currentLookAtNpc!=nullptr)
    return canSeeNpc(*currentLookAtNpc, false);
  return false;
  }

bool Npc::canRayHitPoint(const Tempest::Vec3 pos, bool freeLos, float extRange) const {
  float ang = freeLos ? 180.f : -1;
  return canRayHitPoint(pos, ang, extRange);
  }

bool Npc::canRayHitPoint(const Tempest::Vec3 pos, float angOverride, float extRange) const {
  const float range = float(hnpc->senses_range) + extRange;
  if(qDistTo(pos)>range*range)
    return false;
  // npc eyesight height by default
  return canRayHitPoint(visual.mapHeadBone(), pos, angOverride, extRange);
  }

bool Npc::canRayHitPoint(const Tempest::Vec3 self, const Tempest::Vec3 pos, float angOverride, float extRange) const {
  const float range = float(hnpc->senses_range) + extRange;
  if(qDistTo(pos)>range*range)
    return false;

  // NOTE: in original-game oCNpc::CanSee (Gothic2.exe 0x00741c10) accepts a target as
  // visible when |azimuth| < 91 degrees (the 0x5b constant). Here `da` is measured from the
  // reversed (target->self) direction, so the forward half-angle is (180 - refAngle):
  // cos(100) gave only a +-80 degree cone (NPCs too short-sighted laterally); cos(89) yields
  // the original's +-91 degree forward cone.
  static const double ref = std::cos(89*M_PI/180.0);
  const DynamicWorld* w   = owner.physic();
  bool freeLos = angOverride>=180.f;
  if(freeLos) {
    return !w->ray(self, pos).hasCol;
    }

  float dx  = self.x-pos.x, dz=self.z-pos.z;
  float dir = angleDir(dx,dz);
  float da  = float(M_PI)*(visual.viewDirection()-dir)/180.f;
  auto  ca  = angOverride > 0 ? std::cos(angOverride*M_PI/180.0) : ref;
  if(double(std::cos(da))<=ca) {
    if(!w->ray(self, pos).hasCol)
      return true;
    }
  return false;
  }

SensesBit Npc::canSenseNpc(const Npc &oth, bool freeLos, float extRange) const {
  // NOTE1: https://github.com/Try/OpenGothic/pull/589#issuecomment-2045897394
  // NOTE2: interacting with chest(lockpicking) or some MOBSI should not produce 'noise'
  // NOTE3: seem npc can't hear player in general case, and hearing relevant only for sendImmediatePerc cases
  const bool isNoisy = false;
  const auto mid     = oth.centerPosition();
  return canSenseNpc(mid,freeLos,isNoisy,extRange);
  }

SensesBit Npc::canSenseNpc(const Tempest::Vec3 pos, bool freeLos, bool isNoisy, float extRange) const {
  const float range = float(hnpc->senses_range)+extRange;
  if(qDistTo(pos)>range*range)
    return SensesBit::SENSE_NONE;

  SensesBit ret = SensesBit::SENSE_SMELL;

  if(isNoisy) {
    // no need to be in same room: https://github.com/Try/OpenGothic/issues/420
    ret = ret | SensesBit::SENSE_HEAR;
    }

  if((hnpc->senses & int32_t(SensesBit::SENSE_SEE))!=0 && canRayHitPoint(pos, freeLos, extRange)) {
    ret = ret | SensesBit::SENSE_SEE;
    }

  return ret & SensesBit(hnpc->senses);
  }

bool Npc::canSeeItem(const Item& it, bool freeLos) const {
  // NOTE: in original-game oCNpc::CanSee (Gothic2.exe 0x00741c10) accepts a target as
  // visible when |azimuth| < 91 degrees (the 0x5b constant). Here `da` is measured from the
  // reversed (target->self) direction, so the forward half-angle is (180 - refAngle):
  // cos(100) gave only a +-80 degree cone (NPCs too short-sighted laterally); cos(89) yields
  // the original's +-91 degree forward cone.
  static const double ref = std::cos(89*M_PI/180.0);

  const auto  itMid = it.midPosition();
  const auto  cen   = visual.mapHeadBone();
  const auto  dir   = itMid - cen;
  const float range = float(hnpc->senses_range);

  if(dir.quadLength()>range*range)
    return false;

  if(!freeLos) {
    float dx  = dir.x, dz = dir.z;
    float dir = angleDir(dx,dz);
    float da  = float(M_PI)*(visual.viewDirection()-dir)/180.f;
    if(double(std::cos(da))>ref)
      return false;
    }

  if(auto bbox = it.bBox()) {
    // npc eyesight height
    auto  at     = it.midPosition();
    auto  tMax   = (at - cen).length();
    auto  dir    = (at - cen)/tMax;
    float tHit   = DynamicWorld::rayBox(cen, dir, tMax, it.transform(), bbox[0], bbox[1]);

    const auto r = owner.physic()->ray(cen, cen+dir*tHit);
    if(r.hasCol)
      return false;
    } else {
    const auto r = owner.physic()->ray(cen, itMid);
    if(r.hasCol)
      return false;
    }

  return true;
  }

bool Npc::isAlignedToGround() const {
  auto gl = guild();
  return (owner.script().guildVal().surface_align[gl]!=0) || isDead() || isLie();
  }

Vec3 Npc::groundNormal() const {
  auto ground = mvAlgo.groundNormal();
  const bool align = isAlignedToGround();

  if(!align || mvAlgo.isInAir() || mvAlgo.isSwim())
    ground = {0,1,0};
  if(ground==Vec3())
    ground = {0,1,0};
  return ground;
  }

Matrix4x4 Npc::mkPositionMatrix() const {
  const auto ground = groundNormal();
  const bool align  = isAlignedToGround();

  float angY = mvAlgo.isDive() ? angleY : 0;
  if(align) {
    float rot  = rotationRad();
    float s    = std::sin(rot), c = std::cos(rot);
    auto  dir  = Vec3(c,0,s);
    auto  norm = Vec3::normalize(ground);

    float cx = Vec3::dotProduct(norm,dir);
    angY = -std::asin(cx)*180.f/float(M_PI);
    }

  Matrix4x4 mt = Matrix4x4();
  mt.identity();
  mt.translate(x,y,z);
  mt.rotateOY(90-angle);
  if(angY!=0)
    mt.rotateOX(-angY);
  if(isPlayer() && !align) {
    mt.rotateOZ(runAng);
    }
  mt.scale(sz[0],sz[1],sz[2]);
  return mt;
  }

void Npc::updateTransform() {
  updateAnimation(0, true);
  }

void Npc::updateAnimation(uint64_t dt, bool force) {
  const auto camera = Gothic::inst().camera();
  if(isPlayer() && camera!=nullptr && camera->isFree())
    dt = 0;

  if(durtyTranform) {
    const auto ground = groundNormal();
    if(lastGroundNormal!=ground) {
      durtyTranform |= TR_Rot;
      lastGroundNormal = ground;
      }

    sfxWeapon.setPosition(x,y,z);
    Matrix4x4 pos;
    if(durtyTranform==TR_Pos) {
      pos = visual.transform();
      pos.set(3,0,x);
      pos.set(3,1,y);
      pos.set(3,2,z);
      } else {
      pos = mkPositionMatrix();
      }

    visual.setObjMatrix(pos,false);
    durtyTranform = 0;
    }

  bool syncAtt = visual.updateAnimation(this,nullptr,owner,dt,force);
  if(syncAtt)
    visual.syncAttaches();
  }

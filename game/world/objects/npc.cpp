#include "npc.h"

#include <Tempest/Matrix4x4>
#include <Tempest/Log>

#include <zenload/zCMaterial.h>

#include "graphics/mesh/skeleton.h"
#include "graphics/mesh/animmath.h"
#include "graphics/pfx/particlefx.h"
#include "graphics/visualfx.h"
#include "game/damagecalculator.h"
#include "game/serialize.h"
#include "game/gamescript.h"
#include "world/triggers/trigger.h"
#include "world/objects/interactive.h"
#include "world/objects/item.h"
#include "world/world.h"
#include "utils/versioninfo.h"
#include "utils/fileext.h"
#include "resources.h"

using namespace Tempest;

void Npc::GoTo::save(Serialize& fout) const {
  fout.write(npc, uint8_t(flag), wp, pos);
  }

void Npc::GoTo::load(Serialize& fin) {
  fin.read(npc, reinterpret_cast<uint8_t&>(flag), wp);
  if(fin.version()>=23)
    fin.read(pos);
  }

Vec3 Npc::GoTo::target() const {
  if(npc!=nullptr)
    return npc->position();
  if(wp!=nullptr)
    return Vec3(wp->x,wp->y,wp->z);
  return pos;
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
    hnpc        = self.hnpc;
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

  TransformBack(Npc& owner, Serialize& fin) {
    fin.read(hnpc);
    invent.load(owner,fin);
    fin.read(talentsSk,talentsVl);
    fin.read(body,head,vHead,vTeeth,vColor,bdColor);

    std::string sk;
    fin.read(sk);
    skeleton = Resources::loadSkeleton(sk.c_str());
    }

  void undo(Npc& self) {
    int32_t aivar[100]={};

    auto ucnt     = self.hnpc.useCount;
    auto exp      = self.hnpc.exp;
    auto exp_next = self.hnpc.exp_next;
    auto lp       = self.hnpc.lp;
    auto level    = self.hnpc.level;
    std::memcpy(aivar,self.hnpc.aivar,sizeof(aivar));

    self.hnpc   = hnpc;
    self.hnpc.useCount = ucnt;
    self.hnpc.exp      = exp;
    self.hnpc.exp_next = exp_next;
    self.hnpc.lp       = lp;
    self.hnpc.level    = level;
    std::memcpy(self.hnpc.aivar,aivar,sizeof(aivar));

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
    fout.write(hnpc);
    invent.save(fout);
    fout.write(talentsSk,talentsVl);
    fout.write(body,head,vHead,vTeeth,vColor,bdColor);
    fout.write(skeleton!=nullptr ? skeleton->name() : "");
    }

  Daedalus::GEngineClasses::C_Npc hnpc={};
  Inventory                       invent;
  int32_t                         talentsSk[TALENT_MAX_G2]={};
  int32_t                         talentsVl[TALENT_MAX_G2]={};

  std::string                     body,head;
  int32_t                         vHead=0, vTeeth=0, vColor=0;
  int32_t                         bdColor=0;

  const Skeleton*                 skeleton = nullptr;
  };


Npc::Npc(World &owner, size_t instance, std::string_view waypoint)
  :owner(owner),mvAlgo(*this) {
  outputPipe          = owner.script().openAiOuput();
  hnpc.userPtr        = this;
  hnpc.instanceSymbol = instance;

  if(instance==size_t(-1))
    return;

  hnpc.wp = std::string(waypoint);
  owner.script().initializeInstance(hnpc,instance);
  if(hnpc.attribute[ATR_HITPOINTS]<=1 && hnpc.attribute[ATR_HITPOINTSMAX]<=1) {
    onNoHealth(true,HS_NoSound);
    }
  }

Npc::Npc(World &owner, Serialize &fin)
  :owner(owner),mvAlgo(*this) {
  outputPipe   = owner.script().openAiOuput();
  hnpc.userPtr = this;

  load(fin);
  }

Npc::~Npc(){
  if(currentInteract)
    currentInteract->dettach(*this,true);
  owner.script().clearReferences(hnpc);
  assert(hnpc.useCount==0);
  }

void Npc::save(Serialize &fout) {
  fout.write(hnpc);
  fout.write(x,y,z,angle,sz);
  fout.write(body,head,vHead,vTeeth,bdColor,vColor);
  fout.write(wlkMode,trGuild,talentsSk,talentsVl,refuseTalkMilis);
  visual.save(fout,*this);

  fout.write(int32_t(permAttitude),int32_t(tmpAttitude));
  fout.write(perceptionTime,perceptionNextTime);
  for(auto& i:perception)
    fout.write(i.func);

  invent.save(fout);
  fout.write(lastHitType,lastHitSpell);
  if(currentSpellCast<uint32_t(-1))
    fout.write(uint32_t(currentSpellCast)); else
    fout.write(uint32_t(-1));
  fout.write(uint8_t(castLevel),castNextTime);
  fout.write(spellInfo);
  if(transformSpl!=nullptr) {
    fout.write(true);
    transformSpl->save(fout);
    } else {
    fout.write(false);
    }
  saveAiState(fout);

  fout.write(currentInteract);
  fout.write(currentOther);
  fout.write(currentVictum);
  fout.write(currentLookAt,currentTarget,nearestEnemy);

  go2.save(fout);
  fout.write(currentFp,currentFpLock);
  wayPath.save(fout);

  mvAlgo.save(fout);
  fghAlgo.save(fout);
  fout.write(lastEventTime);
  fout.write(angleY);
  }

void Npc::load(Serialize &fin) {
  fin.read(hnpc);
  auto& sym = owner.script().getSymbol(hnpc.instanceSymbol);
  sym.instance.set(&hnpc, Daedalus::IC_Npc);

  fin.read(x,y,z,angle,sz);
  fin.read(body,head,vHead,vTeeth,bdColor,vColor);
  fin.read(wlkMode,trGuild,talentsSk,talentsVl,refuseTalkMilis);
  durtyTranform = TR_Pos|TR_Rot|TR_Scale;

  visual.load(fin,*this);
  setVisualBody(vHead,vTeeth,vColor,bdColor,body,head);

  fin.read(reinterpret_cast<int32_t&>(permAttitude),reinterpret_cast<int32_t&>(tmpAttitude));
  fin.read(perceptionTime,perceptionNextTime);
  for(auto& i:perception)
    fin.read(i.func);
  invent.load(*this,fin);
  fin.read(lastHitType,lastHitSpell);
  if(fin.version()>=9) {
    if(fin.version()>=19) {
      uint32_t currentSpellCastU32 = uint32_t(-1);
      fin.read(currentSpellCastU32);
      currentSpellCast = (currentSpellCastU32==uint32_t(-1) ? size_t(-1) : currentSpellCastU32);
      } else {
      int32_t currentSpellCast32;
      fin.read(currentSpellCast32);
      // legacy
      }
    }
  if(fin.version()>=21)
    fin.read(reinterpret_cast<uint8_t&>(castLevel),castNextTime);
  if(fin.version()>=22) {
    fin.read(spellInfo);
    bool hasTr = false;
    fin.read(hasTr);
    if(hasTr)
      transformSpl.reset(new TransformBack(*this,fin));
    }
  loadAiState(fin);

  if(fin.version()>=24)
    fin.read(currentInteract);
  fin.read(currentOther);
  if(fin.version()>=20)
    fin.read(currentVictum);
  fin.read(currentLookAt,currentTarget,nearestEnemy);

  go2.load(fin);
  fin.read(currentFp,currentFpLock);
  wayPath.load(fin);

  mvAlgo.load(fin);
  fghAlgo.load(fin);
  fin.read(lastEventTime);

  if(fin.version()>17) {
    fin.read(angleY);
    }

  if(isDead())
    physic.setEnable(false);
  }

void Npc::postValidate() {
  if(currentInteract!=nullptr && !currentInteract->isAttached(*this))
    currentInteract = nullptr;
  }

void Npc::saveAiState(Serialize& fout) const {
  fout.write(aniWaitTime,waitTime,faiWaitTime,outWaitTime);
  fout.write(uint8_t(aiPolicy));
  fout.write(aiState.funcIni,aiState.funcLoop,aiState.funcEnd,aiState.sTime,aiState.eTime,aiState.started,aiState.loopNextTime);
  fout.write(aiPrevState);

  aiQueue.save(fout);
  aiQueueOverlay.save(fout);

  fout.write(uint32_t(routines.size()));
  for(auto& i:routines) {
    fout.write(i.start,i.end,i.callback,i.point);
    }
  }

void Npc::loadAiState(Serialize& fin) {
  if(fin.version()>=1)
    fin.read(aniWaitTime);
  fin.read(waitTime,faiWaitTime);
  if(fin.version()>=26)
    fin.read(outWaitTime);
  fin.read(reinterpret_cast<uint8_t&>(aiPolicy));
  fin.read(aiState.funcIni,aiState.funcLoop,aiState.funcEnd,aiState.sTime,aiState.eTime,aiState.started,aiState.loopNextTime);
  fin.read(aiPrevState);

  aiQueue.load(fin);
  if(fin.version()>=26)
    aiQueueOverlay.load(fin);

  uint32_t size=0;
  fin.read(size);
  routines.resize(size);
  for(auto& i:routines) {
    fin.read(i.start,i.end,i.callback,i.point);
    }
  }

bool Npc::setPosition(float ix, float iy, float iz) {
  if(x==ix && y==iy && z==iz)
    return false;
  x = ix;
  y = iy;
  z = iz;
  durtyTranform |= TR_Pos;

  physic.setPosition(Vec3{x,y,z});
  updatePos();
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
  if(aiPolicy>=AiFar)
    return true; // don't waste CPU on far-away svm-talks
  if(act.act==AI_Output           && outputPipe->output   (*this,act.s0))
    return true;
  auto& svm = owner.script().messageFromSvm(act.s0,hnpc.voice);
  if(act.act==AI_OutputSvm        && outputPipe->outputSvm(*this,svm))
    return true;
  if(act.act==AI_OutputSvmOverlay && outputPipe->outputOv(*this,svm))
    return true;
  return false;
  }

void Npc::setDirection(float x, float /*y*/, float z) {
  float a=angleDir(x,z);
  setDirection(a);
  }

void Npc::setDirection(float rotation) {
  angle = rotation;
  durtyTranform |= TR_Rot;
  }

void Npc::setDirectionY(float rotation) {
  if(rotation>90)
    rotation = 90;
  if(rotation<-90)
    rotation = -90;
  rotation = std::fmod(rotation,360.f);
  if(!mvAlgo.isSwim())
    return;
  angleY = rotation;
  durtyTranform |= TR_Rot;
  }

void Npc::setRunAngle(float angle) {
  durtyTranform |= TR_Rot;
  runAng = angle;
  }

float Npc::angleDir(float x, float z) {
  float a=-90;
  if(x!=0.f || z!=0.f)
    a = 90+180.f*std::atan2(z,x)/float(M_PI);
  return a;
  }

bool Npc::resetPositionToTA() {
  const auto npcType   = hnpc.npcType;
  const bool isMainNpc = (npcType==Daedalus::GEngineClasses::NPCTYPE_MAIN ||
                          npcType==Daedalus::GEngineClasses::NPCTYPE_OCMAIN ||
                          npcType==Daedalus::GEngineClasses::NPCTYPE_BL_MAIN);
  const bool isDead = this->isDead();

  if(isDead && !isMainNpc && !invent.hasMissionItems()) {
    const bool isDragon         = (owner.version().game==2 && guild()==GIL_DRAGON);
    const bool isBackgroundBody = (hnpc.attribute[ATR_HITPOINTSMAX]==1);
    if(!isBackgroundBody && !isDragon)
      return false;
    }

  invent.clearSlot(*this,"",false);
  if(routines.size()==0)
    return true;

  attachToPoint(nullptr);
  if(!isPlayer())
    setInteraction(nullptr,true);
  clearAiQueue();

  if(!isDead) {
    visual.stopAnim(*this,"");
    clearState(true);
    }

  auto& rot = currentRoutine();
  auto  at  = rot.point;

  if(at==nullptr) {
    const auto time  = owner.time().timeInDay();
    const auto day   = gtime(24,0).toInt();
    int64_t    delta = std::numeric_limits<int64_t>::max();

    // closest time-point
    for(auto& i:routines) {
      int64_t d=0;
      if(i.start<i.end) {
        d = time.toInt()-i.start.toInt();
        } else {
        d = time.toInt()-i.end.toInt();
        }
      if(d<=0)
        d+=day;

      if(i.point && d<delta) {
        at    = i.point;
        delta = d;
        }
      }

    if(at==nullptr)
      return false;
    }

  if(at->isLocked() && !isDead){
    auto p = owner.findNextPoint(*at);
    if(p!=nullptr)
      at=p;
    }
  setPosition (at->x, at->y, at->z);
  setDirection(at->dirX,at->dirY,at->dirZ);
  if(!isDead)
    attachToPoint(at);
  return true;
  }

void Npc::stopDlgAnim() {
  visual.stopDlgAnim(*this);
  }

void Npc::clearSpeed() {
  mvAlgo.clearSpeed();
  }

void Npc::setProcessPolicy(ProcessPolicy t) {
  if(aiPolicy==t)
    return;
  if(aiPolicy==ProcessPolicy::Player)
    runAng = 0;
  aiPolicy=t;
  }

void Npc::setWalkMode(WalkBit m) {
  wlkMode = m;
  }

bool Npc::isPlayer() const {
  return aiPolicy==Npc::ProcessPolicy::Player;
  }

bool Npc::startClimb(JumpStatus jump) {
  visual.setAnimRotate(*this,0);
  return mvAlgo.startClimb(jump);
  }

bool Npc::checkHealth(bool onChange,bool allowUnconscious) {
  if(isDead()) {
    return false;
    }
  if(isUnconscious() && allowUnconscious) {
    return false;
    }

  const int minHp = isMonster() ? 0 : 1;
  if(hnpc.attribute[ATR_HITPOINTS]<=minHp) {
    if(hnpc.attribute[ATR_HITPOINTSMAX]<=1) {
      size_t fdead=owner.script().getSymbolIndex("ZS_Dead");
      startState(fdead,"");
      physic.setEnable(false);
      return false;
      }

    if(currentOther==nullptr ||
       !allowUnconscious ||
       owner.script().personAttitude(*this,*currentOther)==ATT_HOSTILE ||
       guild()>GIL_SEPERATOR_HUM){
      if(hnpc.attribute[ATR_HITPOINTS]<=0)
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
  visual.dropWeapon(*this);
  dropTorch();
  visual.setToFightMode(WeaponState::NoWeapon);
  updateWeaponSkeleton();

  setOther(lastHit);
  clearAiQueue();

  const char* svm   = death ? "SVM_%d_DEAD" : "SVM_%d_AARGH";
  const char* state = death ? "ZS_Dead"     : "ZS_Unconscious";

  if(!death)
    hnpc.attribute[ATR_HITPOINTS]=1;

  size_t fdead=owner.script().getSymbolIndex(state);
  startState(fdead,"",gtime::endOfTime(),true);
  if(hnpc.voice>0 && sndMask!=HS_NoSound) {
    emitSoundSVM(svm);
    }

  setInteraction(nullptr,true);
  invent.clearSlot(*this,"",false);

  if(death)
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
    r = visual.mapBone(head) + position();
    } else {
    if(!mvAlgo.isSwim())
      r.y = visual.pose().translateY();
    auto mt = visual.transform();
    mt.project(r);
    }

  return r;
  }

float Npc::collisionRadius() const {
  return physic.radius();
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

float Npc::translateY() const {
  return visual.pose().translateY();
  }

float Npc::centerY() const {
  return physic.centerY();
  }

Npc *Npc::lookAtTarget() const {
  return currentLookAt;
  }

std::string_view Npc::portalName() {
  return mvAlgo.portalName();
  }

std::string_view Npc::formerPortalName() {
  return mvAlgo.formerPortalName();
  }

float Npc::qDistTo(float x1, float y1, float z1) const {
  float dx=x-x1;
  float dy=y+translateY()-y1;
  float dz=z-z1;
  return dx*dx+dy*dy+dz*dz;
  }

float Npc::qDistTo(const WayPoint *f) const {
  if(f==nullptr)
    return 0.f;
  return qDistTo(f->x,f->y,f->z);
  }

float Npc::qDistTo(const Npc &p) const {
  return qDistTo(p.x,p.y+p.translateY(),p.z);
  }

float Npc::qDistTo(const Interactive &p) const {
  auto pos = p.position();
  return qDistTo(pos.x,pos.y,pos.z);
  }

uint8_t Npc::calcAniComb() const {
  if(currentTarget==nullptr)
    return 0;
  auto dpos = currentTarget->position()-position();
  return Pose::calcAniComb(dpos,angle);
  }

void Npc::updateAnimation(uint64_t dt) {
  bool syncAtt = visual.updateAnimation(this,owner,dt);
  if(durtyTranform) {
    updatePos();
    syncAtt = true;
    durtyTranform=0;
    }

  if(syncAtt)
    visual.syncAttaches();
  }

void Npc::updateTransform() {
  if(durtyTranform) {
    updatePos();
    visual.syncAttaches();
    durtyTranform=0;
    }
  }

std::string_view Npc::displayName() const {
  return hnpc.name[0].c_str();
  }

Tempest::Vec3 Npc::displayPosition() const {
  auto p = visual.displayPosition();
  return p+position();
  }

void Npc::setVisual(std::string_view visual) {
  auto skelet = Resources::loadSkeleton(visual);
  setVisual(skelet);
  setPhysic(owner.physic()->ghostObj(visual));
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

void Npc::addOverlay(const Skeleton* sk,uint64_t time) {
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

bool Npc::toogleTorch() {
  std::string_view overlay = "HUMANS_TORCH.MDS";
  if(isUsingTorch()) {
    visual.setTorch(false,owner);
    delOverlay(overlay);
    return false;
    }
  visual.setTorch(true,owner);
  addOverlay(overlay,0);
  return true;
  }

bool Npc::isUsingTorch() const {
  std::string_view overlay = "HUMANS_TORCH.MDS";
  return hasOverlay(overlay);
  }

void Npc::dropTorch(bool burnout) {
  auto sk = visual.visualSkeleton();
  if(sk==nullptr)
    return;

  std::string_view overlay = "HUMANS_TORCH.MDS";
  if(!isUsingTorch())
    return;

  visual.setTorch(false,owner);
  delOverlay(overlay);

  size_t torchId = 0;
  if(burnout)
    torchId = owner.script().getSymbolIndex("ItLsTorchburned"); else
    torchId = owner.script().getSymbolIndex("ItLsTorchburning");

  size_t leftHand = sk->findNode("ZS_LEFTHAND");
  if(torchId!=size_t(-1) && leftHand!=size_t(-1)) {

    auto mat = visual.transform();
    if(leftHand<visual.pose().boneCount())
      mat.mul(visual.pose().bone(leftHand));

    owner.addItemDyn(torchId,mat,hnpc.instanceSymbol);
    }
  }

Tempest::Vec3 Npc::animMoveSpeed(uint64_t dt) const {
  return visual.pose().animMoveSpeed(owner.tickCount(),dt);
  }

void Npc::setVisual(const Skeleton* v) {
  visual.setVisual(v);
  }

void Npc::setVisualBody(int32_t headTexNr, int32_t teethTexNr, int32_t bodyTexNr, int32_t bodyTexColor,
                        const std::string &ibody, const std::string &ihead) {
  auto& w = owner;

  body    = ibody;
  head    = ihead;
  vHead   = headTexNr;
  vTeeth  = teethTexNr;
  vColor  = bodyTexNr;
  bdColor = bodyTexColor;

  auto  vhead = head.empty() ? MeshObjects::Mesh() : w.addView(FileExt::addExt(head,".MMB"),vHead,vTeeth,bdColor);
  auto  vbody = body.empty() ? MeshObjects::Mesh() : w.addView(FileExt::addExt(body,".ASC"),vColor,0,bdColor);
  visual.setVisualBody(std::move(vhead),std::move(vbody),owner,bdColor);
  updateArmour();

  durtyTranform|=TR_Pos; // update obj matrix
  }

void Npc::updateArmour() {
  auto  ar = invent.currentArmour();
  auto& w  = owner;

  if(ar==nullptr) {
    auto  vbody = body.empty() ? MeshObjects::Mesh() : w.addView(FileExt::addExt(body,".ASC"),vColor,0,bdColor);
    visual.setBody(std::move(vbody),owner,bdColor);
    } else {
    auto& itData = ar->handle();
    auto  flag   = ItmFlags(itData.mainflag);
    if(flag & ITM_CAT_ARMOR){
      auto& asc   = itData.visual_change;
      auto  vbody = asc.empty() ? MeshObjects::Mesh() : w.addView(asc.c_str(),vColor,0,bdColor);
      visual.setArmour(std::move(vbody),owner);
      }
    }
  }

void Npc::setSword(MeshObjects::Mesh&& s) {
  visual.setSword(std::move(s));
  updateWeaponSkeleton();
  }

void Npc::setRangeWeapon(MeshObjects::Mesh&& b) {
  visual.setRangeWeapon(std::move(b));
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
  visual.updateWeaponSkeleton(invent.currentMeleWeapon(),invent.currentRangeWeapon());
  }

void Npc::setPhysic(DynamicWorld::NpcItem &&item) {
  physic = std::move(item);
  physic.setUserPointer(this);
  physic.setPosition(Vec3{x,y,z});
  }

void Npc::setFatness(float f) {
  visual.setFatness(f);
  }

void Npc::setScale(float x, float y, float z) {
  sz[0]=x;
  sz[1]=y;
  sz[2]=z;
  durtyTranform |= TR_Scale;
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
  return visual.startAnimItem(*this,scheme,state);
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

bool Npc::isFinishingMove() const {
  return visual.pose().isInAnim("T_1HSFINISH") || visual.pose().isInAnim("T_2HSFINISH");
  }

bool Npc::isStanding() const {
  return visual.isStanding();
  }

bool Npc::isSwim() const {
  return mvAlgo.isSwim();
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

bool Npc::isFaling() const {
  return mvAlgo.isFaling();
  }

bool Npc::isSlide() const {
  return mvAlgo.isSlide();
  }

bool Npc::isInAir() const {
  return mvAlgo.isInAir();
  }

void Npc::setTalentSkill(Talent t, int32_t lvl) {
  if(t<TALENT_MAX_G2) {
    talentsSk[t] = lvl;
    if(t==TALENT_1H){
      if(lvl==0){
        delOverlay("HUMANS_1HST1.MDS");
        delOverlay("HUMANS_1HST2.MDS");
        }
      else if(lvl==1){
        addOverlay("HUMANS_1HST1.MDS",0);
        delOverlay("HUMANS_1HST2.MDS");
        }
      else if(lvl==2){
        delOverlay("HUMANS_1HST1.MDS");
        addOverlay("HUMANS_1HST2.MDS",0);
        }
      }
    else if(t==TALENT_2H){
      if(lvl==0){
        delOverlay("HUMANS_2HST1.MDS");
        delOverlay("HUMANS_2HST2.MDS");
        }
      else if(lvl==1){
        addOverlay("HUMANS_2HST1.MDS",0);
        delOverlay("HUMANS_2HST2.MDS");
        }
      else if(lvl==2){
        delOverlay("HUMANS_2HST1.MDS");
        addOverlay("HUMANS_2HST2.MDS",0);
        }
      }
    else if(t==TALENT_BOW){
      if(lvl==0){
        delOverlay("HUMANS_BOWT1.MDS");
        delOverlay("HUMANS_BOWT2.MDS");
        }
      else if(lvl==1){
        addOverlay("HUMANS_BOWT1.MDS",0);
        delOverlay("HUMANS_BOWT2.MDS");
        }
      else if(lvl==2){
        delOverlay("HUMANS_BOWT1.MDS");
        addOverlay("HUMANS_BOWT2.MDS",0);
        }
      }
    else if(t==TALENT_CROSSBOW){
      if(lvl==0){
        delOverlay("HUMANS_CBOWT1.MDS");
        delOverlay("HUMANS_CBOWT2.MDS");
        }
      else if(lvl==1){
        addOverlay("HUMANS_CBOWT1.MDS",0);
        delOverlay("HUMANS_CBOWT2.MDS");
        }
      else if(lvl==2){
        delOverlay("HUMANS_CBOWT1.MDS");
        addOverlay("HUMANS_CBOWT2.MDS",0);
        }
      }
    else if(t==TALENT_ACROBAT){
      if(lvl==0)
        delOverlay("HUMANS_ACROBATIC.MDS"); else
        addOverlay("HUMANS_ACROBATIC.MDS",0);
      }
    }
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

int32_t Npc::hitChanse(Talent t) const {
  if(t<Daedalus::GEngineClasses::MAX_HITCHANCE)
    return hnpc.hitChance[t];
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
    return hnpc.attribute[a];
  return 0;
  }

void Npc::changeAttribute(Attribute a, int32_t val, bool allowUnconscious) {
  if(a>=ATR_MAX || val==0)
    return;

  hnpc.attribute[a]+=val;
  if(hnpc.attribute[a]<0)
    hnpc.attribute[a]=0;
  if(a==ATR_HITPOINTS && hnpc.attribute[a]>hnpc.attribute[ATR_HITPOINTSMAX])
    hnpc.attribute[a] = hnpc.attribute[ATR_HITPOINTSMAX];
  if(a==ATR_MANA && hnpc.attribute[a]>hnpc.attribute[ATR_MANAMAX])
    hnpc.attribute[a] = hnpc.attribute[ATR_MANAMAX];

  if(val<0)
    invent.invalidateCond(*this);

  if(a==ATR_HITPOINTS)
    checkHealth(true,allowUnconscious);
  }

int32_t Npc::protection(Protection p) const {
  if(p<PROT_MAX)
    return hnpc.protection[p];
  return 0;
  }

void Npc::changeProtection(Protection p, int32_t val) {
  if(p<PROT_MAX)
    hnpc.protection[p]=val;
  }

uint32_t Npc::instanceSymbol() const {
  return uint32_t(hnpc.instanceSymbol);
  }

uint32_t Npc::guild() const {
  return std::min(uint32_t(hnpc.guild), uint32_t(GIL_MAX-1));
  }

bool Npc::isMonster() const {
  return Guild::GIL_SEPERATOR_HUM<guild() && guild()<Guild::GIL_SEPERATOR_ORC;
  }

void Npc::setTrueGuild(int32_t g) {
  trGuild = g;
  }

int32_t Npc::trueGuild() const {
  if(trGuild==GIL_NONE)
    return hnpc.guild;
  return trGuild;
  }

int32_t Npc::magicCyrcle() const {
  return talentSkill(TALENT_RUNES);
  }

int32_t Npc::level() const {
  return hnpc.level;
  }

int32_t Npc::experience() const {
  return hnpc.exp;
  }

int32_t Npc::experienceNext() const {
  return hnpc.exp_next;
  }

int32_t Npc::learningPoints() const {
  return hnpc.lp;
  }

int32_t Npc::diveTime() const {
  return mvAlgo.diveTime();
  }

void Npc::setAttitude(Attitude att) {
  permAttitude = att;
  }

bool Npc::isFriend() const {
  return hnpc.npcType==Daedalus::GEngineClasses::ENPCType::NPCTYPE_FRIEND;
  }

void Npc::setTempAttitude(Attitude att) {
  tmpAttitude = att;
  }

bool Npc::implPointAt(const Tempest::Vec3& to) {
  auto    dpos = to-position();
  uint8_t comb = Pose::calcAniComb(dpos,angle);

  return (setAnimAngGet(Npc::Anim::PointAt,comb)!=nullptr);
  }

bool Npc::implLookAt(uint64_t dt) {
  if(currentLookAt==nullptr)
    return false;
  auto dx = currentLookAt->x-x;
  auto dy = currentLookAt->y-y;
  auto dz = currentLookAt->z-z;
  return implLookAt(dx,dy,dz,dt);
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

bool Npc::implTurnTo(const Npc &oth, uint64_t dt) {
  if(&oth==this)
    return true;
  auto dx = oth.x-x;
  auto dz = oth.z-z;
  return implTurnTo(dx,dz,false,dt);
  }

bool Npc::implTurnTo(const Npc& oth, bool noAnim, uint64_t dt) {
  if(&oth==this)
    return true;
  auto dx = oth.x-x;
  auto dz = oth.z-z;
  return implTurnTo(dx,dz,noAnim,dt);
  }

bool Npc::implTurnTo(float dx, float dz, bool noAnim, uint64_t dt) {
  auto  gl   = guild();
  float step = float(owner.script().guildVal().turn_speed[gl]);
  return rotateTo(dx,dz,step,noAnim,dt);
  }

bool Npc::implGoTo(uint64_t dt) {
  float dist = 0;
  if(go2.npc) {
    if(go2.flag==GT_EnemyA)
      dist = fghAlgo.prefferedAtackDistance(*this,*go2.npc,owner.script()); else
      dist = fghAlgo.baseDistance(*this,*go2.npc,owner.script());
    } else {
    // use smaller threshold, to avoid edge-looping in script
    dist = MoveAlgo::closeToPointThreshold*0.25f;
    if(go2.wp!=nullptr && go2.wp->useCounter()>1)
      dist += 100;
    }
  return implGoTo(dt,dist);
  }

bool Npc::implGoTo(uint64_t dt, float destDist) {
  if(go2.flag==GT_No)
    return false;

  if(isInAir()) {
    mvAlgo.tick(dt);
    return true;
    }

  auto dpos = go2.target()-position();

  if(go2.flag==GT_Flee) {
    // nop
    }
  else if(mvAlgo.isClose(go2.target(),destDist)) {
    bool finished = true;
    if(go2.flag==GT_Way) {
      go2.wp = wayPath.pop();
      if(go2.wp!=nullptr) {
        attachToPoint(go2.wp);
        finished = false;
        }
      }
    if(finished)
      clearGoTo();
    } else {
    if(mvAlgo.checkLastBounce() && implTurnTo(dpos.x,dpos.z,false,dt)){
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

bool Npc::implAtack(uint64_t dt) {
  if(currentTarget==nullptr || isPlayer() || isTalk())
    return false;

  if(currentTarget->isDown()){
    // NOTE: don't clear internal target, to make scripts happy
    // currentTarget=nullptr;
    if(go2.flag==GT_EnemyA || go2.flag==GT_EnemyG) {
      stopWalking();
      go2.clear();
      }
    fghAlgo.onClearTarget();
    return false;
    }

  auto ws = weaponState();
  if(ws==WeaponState::NoWeapon)
    return false;

  if(faiWaitTime>=owner.tickCount()) {
    adjustAtackRotation(dt);
    mvAlgo.tick(dt,MoveAlgo::FaiMove);
    return true;
    }

  if(!fghAlgo.hasInstructions())
    return false;

  FightAlgo::Action act = fghAlgo.nextFromQueue(*this,*currentTarget,owner.script());

  if(act==FightAlgo::MV_BLOCK) {
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

  if(act==FightAlgo::MV_ATACK || act==FightAlgo::MV_ATACKL || act==FightAlgo::MV_ATACKR) {
    static const Anim ani[4]={Anim::Atack,Anim::AtackL,Anim::AtackR};
    if((act!=FightAlgo::MV_ATACK && bodyState()!=BS_RUN) &&
       !fghAlgo.isInWRange(*this,*currentTarget,owner.script())) {
      fghAlgo.consumeAction();
      return true;
      }

    auto ws = weaponState();
    if(ws==WeaponState::Mage) {
      if(castSpell()) {
        fghAlgo.consumeAction();
        }
      setAnimRotate(0);
      setDirection(currentTarget->x-x,
                   currentTarget->y-y,
                   currentTarget->z-z);
      }
    else if(ws==WeaponState::Bow || ws==WeaponState::CBow) {
      if(shootBow()) {
        fghAlgo.consumeAction();
        } else {
        if(!implTurnTo(*currentTarget,true,dt)) {
          if(!aimBow())
            setAnim(Anim::Idle);
          }
        }
      }
    else if(ws==WeaponState::Fist) {
      if(doAttack(Anim::Atack))
        fghAlgo.consumeAction();
      }
    else {
      if(doAttack(ani[act-FightAlgo::MV_ATACK]))
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
    if(setAnim(Npc::Anim::MoveL)){
      visual.setAnimRotate(*this,0);
      implFaiWait(visual.pose().animationTotalTime());
      fghAlgo.consumeAction();
      }
    return true;
    }

  if(act==FightAlgo::MV_STRAFER) {
    if(setAnim(Npc::Anim::MoveR)){
      visual.setAnimRotate(*this,0);
      implFaiWait(visual.pose().animationTotalTime());
      fghAlgo.consumeAction();
      }
    return true;
    }

  if(act==FightAlgo::MV_JUMPBACK) {
    if(setAnim(Npc::Anim::MoveBack)) {
      implFaiWait(visual.pose().animationTotalTime());
      fghAlgo.consumeAction();
      }
    return true;
    }

  if(act==FightAlgo::MV_MOVEA || act==FightAlgo::MV_MOVEG) {
    float dist = 0;
    if(act==FightAlgo::MV_MOVEA)
      dist = fghAlgo.prefferedAtackDistance(*this,*currentTarget,owner.script()); else
      dist = fghAlgo.prefferedGDistance(*this,*currentTarget,owner.script());
    go2.set(currentTarget,(act==FightAlgo::MV_MOVEG) ? GoToHint::GT_EnemyG : GoToHint::GT_EnemyA);

    if((implGoTo(dt) || implTurnTo(*currentTarget,dt)) && qDistTo(*currentTarget)>dist*dist) {
      implAiTick(dt);
      return true;
      }

    go2.clear();
    fghAlgo.consumeAction();
    aiState.loopNextTime = owner.tickCount(); //force ZS_MM_Attack_Loop call
    implAiTick(dt);
    return true;
    }

  if(act==FightAlgo::MV_WAIT) {
    implFaiWait(200);
    fghAlgo.consumeAction();
    setAnim(AnimationSolver::Idle);
    return true;
    }

  if(act==FightAlgo::MV_WAITLONG) {
    implFaiWait(300);
    fghAlgo.consumeAction();
    setAnim(AnimationSolver::Idle);
    return true;
    }

  if(act==FightAlgo::MV_NULL) {
    fghAlgo.consumeAction();
    setAnim(AnimationSolver::Idle);
    return true;
    }

  return true;
  }

void Npc::adjustAtackRotation(uint64_t dt) {
  if(currentTarget!=nullptr && !currentTarget->isDown()) {
    auto ws = weaponState();
    if(!visual.pose().isInAnim("T_FISTATTACKMOVE") &&
       !visual.pose().isInAnim("T_1HATTACKMOVE")   &&
       !visual.pose().isInAnim("T_2HATTACKMOVE")   &&
       ws!=WeaponState::NoWeapon){
      bool noAnim = !hasAutoroll();
      if(ws==WeaponState::Bow || ws==WeaponState::CBow || ws==WeaponState::Mage)
         noAnim = true;
      implTurnTo(*currentTarget,noAnim,dt);
      }
    }
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
  auto ws = visual.fightMode();
  if(!visual.setFightMode(ev.weaponCh))
    return;

  if(ev.weaponCh==ZenLoad::FM_NONE && (ws==WeaponState::W1H || ws==WeaponState::W2H)) {
    if(auto melee = invent.currentMeleWeapon()) {
      if(melee->handle().material==ItemMaterial::MAT_METAL)
        sfxWeapon = ::Sound(owner,::Sound::T_Regular,"UNDRAWSOUND_ME.WAV",{x,y+translateY(),z},2500,false); else
        sfxWeapon = ::Sound(owner,::Sound::T_Regular,"UNDRAWSOUND_WO.WAV",{x,y+translateY(),z},2500,false);
      sfxWeapon.play();
      }
    }
  else if(ev.weaponCh==ZenLoad::FM_1H || ev.weaponCh==ZenLoad::FM_2H) {
    if(auto melee = invent.currentMeleWeapon()) {
      if(melee->handle().material==ItemMaterial::MAT_METAL)
        sfxWeapon = ::Sound(owner,::Sound::T_Regular,"DRAWSOUND_ME.WAV",{x,y+translateY(),z},2500,false); else
        sfxWeapon = ::Sound(owner,::Sound::T_Regular,"DRAWSOUND_WO.WAV",{x,y+translateY(),z},2500,false);
      sfxWeapon.play();
      }
    }
  else if(ev.weaponCh==ZenLoad::FM_BOW || ev.weaponCh==ZenLoad::FM_CBOW) {
    sfxWeapon = ::Sound(owner,::Sound::T_Regular,"DRAWSOUND_BOW",{x,y+translateY(),z},2500,false);
    sfxWeapon.play();
    }
  dropTorch();
  visual.stopDlgAnim(*this);
  updateWeaponSkeleton();
  }

bool Npc::implAiFlee(uint64_t dt) {
  if(currentTarget==nullptr)
    return true;

  auto& oth = *currentTarget;

  const WayPoint* wp      = nullptr;
  const float     maxDist = 5*100; // 5 meters
  owner.findWayPoint(position(),[&](const WayPoint& p) {
    if(p.useCounter()>0 || qDistTo(&p)>maxDist*maxDist)
      return false;
    if(!canSeeNpc(p.x,p.y+10,p.z,true))
      return false;
    if(wp==nullptr || oth.qDistTo(&p)>oth.qDistTo(wp))
      wp = &p;
    return false;
    });

  if(wp==nullptr || oth.qDistTo(wp)<oth.qDistTo(*this)) {
    auto  dx  = oth.x-x;
    auto  dz  = oth.z-z;
    if(implTurnTo(-dx,-dz,false,dt))
      return false;
    } else {
    auto  dx  = wp->x-x;
    auto  dz  = wp->z-z;
    if(implTurnTo(dx,dz,false,dt))
      return false;
    }

  go2.setFlee();
  setAnim(Anim::Move);
  return true;
  }

void Npc::commitDamage() {
  if(currentTarget==nullptr)
    return;
  if(!fghAlgo.isInAtackRange(*this,*currentTarget,owner.script()))
    return;
  if(!fghAlgo.isInFocusAngle(*this,*currentTarget))
    return;
  currentTarget->takeDamage(*this,nullptr);
  }

void Npc::takeDamage(Npc &other, const Bullet* b) {
  if(isDown())
    return;

  assert(b==nullptr || !b->isSpell());
  const bool isJumpb = visual.pose().isJumpBack();
  const bool isBlock = (!other.isMonster() || other.inventory().activeWeapon()!=nullptr) &&
                       fghAlgo.isInFocusAngle(*this,other) &&
                       visual.pose().isDefence(owner.tickCount());

  lastHit = &other;
  if(!isPlayer())
    setOther(&other);
  owner.sendPassivePerc(*this,other,*this,PERC_ASSESSFIGHTSOUND);

  if(!(isBlock || isJumpb) || b!=nullptr) {
    takeDamage(other,b,COLL_DOEVERYTHING,0,false);
    } else {
    if(invent.activeWeapon()!=nullptr)
      visual.emitBlockEffect(*this,other);
    }
  }

void Npc::takeDamage(Npc& other, const Bullet* b, const VisualFx* vfx, int32_t splId) {
  if(isDown())
    return;

  lastHitSpell = splId;
  lastHit = &other;
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
  const bool               dontKill   = ((b==nullptr && splId==0) || (bMask & COLL_DONTKILL)) && (!isSwim());
  int32_t                  damageType = DamageCalculator::damageTypeMask(other);

  if(isSpell) {
    auto& spl  = owner.script().spellDesc(splId);
    splCat     = SpellCategory(spl.spellType);
    damageType = spl.damageType;
    for(size_t i=0; i<DamageCalculator::DAM_INDEX_MAX; ++i)
      if((damageType&(1<<i))!=0)
        dmg[i] = spl.damage_per_level;
    }

  if(!isSpell || splCat==SpellCategory::SPELL_BAD) {
    perceptionProcess(other,this,0,PERC_ASSESSDAMAGE);
    fghAlgo.onTakeHit();
    implFaiWait(0);
    hitResult = DamageCalculator::damageValue(other,*this,b,isSpell,dmg,bMask);
    }

  if(!isSpell && !isDown() && hitResult.hasHit)
    owner.addWeaponHitEffect(other,*this).play();

  if(hitResult.hasHit) {
    if(bodyStateMasked()!=BS_UNCONSCIOUS && interactive()==nullptr && !isSwim() && !mvAlgo.isClimb()) {
      const bool noInter = (hnpc.bodyStateInterruptableOverride!=0);
      if(!noInter)
        visual.interrupt();
      setAnimAngGet(lastHitType=='A' ? Anim::StumbleA : Anim::StumbleB);
      }
    }

  if(hitResult.value>0) {
    currentOther = &other;
    changeAttribute(ATR_HITPOINTS,-hitResult.value,dontKill);

    if(bMask&(COLL_APPLYVICTIMSTATE|COLL_DOEVERYTHING)) {
      owner.sendPassivePerc(*this,other,*this,PERC_ASSESSOTHERSDAMAGE);
      if(isUnconscious()){
        owner.sendPassivePerc(*this,other,*this,PERC_ASSESSDEFEAT);
        }
      else if(isDead()) {
        owner.sendPassivePerc(*this,other,*this,PERC_ASSESSMURDER);
        }
      else {
        if(owner.script().rand(2)==0)
          emitSoundSVM("SVM_%d_AARGH");
        }
      }
    }

  if(damageType & (1<<Daedalus::GEngineClasses::DAM_INDEX_FLY))
    mvAlgo.accessDamFly(x-other.x,z-other.z); // throw enemy
  }

Npc *Npc::updateNearestEnemy() {
  if(aiPolicy!=ProcessPolicy::AiNormal)
    return nullptr;

  Npc*  ret  = nullptr;
  float dist = std::numeric_limits<float>::max();
  if(nearestEnemy!=nullptr &&
     (!nearestEnemy->isDown() && (canSenseNpc(*nearestEnemy,true)&SensesBit::SENSE_SEE)!=SensesBit::SENSE_NONE)) {
    ret  = nearestEnemy;
    dist = qDistTo(*ret);
    }

  owner.detectNpcNear([this,&ret,&dist](Npc& n){
    if(!isEnemy(n) || n.isDown() || &n==this)
      return;

    float d = qDistTo(n);
    if(d<dist && (canSenseNpc(n,true)&SensesBit::SENSE_SEE)!=SensesBit::SENSE_NONE) {
      ret  = &n;
      dist = d;
      }
    });
  nearestEnemy = ret;
  return nearestEnemy;
  }

Npc* Npc::updateNearestBody() {
  if(aiPolicy!=ProcessPolicy::AiNormal)
    return nullptr;

  Npc*  ret  = nullptr;
  float dist = std::numeric_limits<float>::max();

  owner.detectNpcNear([this,&ret,&dist](Npc& n){
    if(!n.isDead())
      return;

    float d = qDistTo(n);
    if(d<dist && (canSenseNpc(n,true)&SensesBit::SENSE_SEE)!=SensesBit::SENSE_NONE) {
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

  for(auto& i:ev.timed) {
    switch(i.def) {
      case ZenLoad::DEF_NULL:
      case ZenLoad::DEF_LAST:
        break;
      case ZenLoad::DEF_CREATE_ITEM: {
        if(auto it = invent.addItem(i.item,1,world())) {
          invent.putToSlot(*this,it->clsId(),i.slot[0]);
          }
        break;
        }
      case ZenLoad::DEF_INSERT_ITEM: {
        invent.putCurrentToSlot(*this,i.slot[0]);
        break;
        }
      case ZenLoad::DEF_REMOVE_ITEM:
      case ZenLoad::DEF_DESTROY_ITEM: {
        invent.clearSlot(*this,"",i.def!=ZenLoad::DEF_REMOVE_ITEM);
        break;
        }
      case ZenLoad::DEF_PLACE_ITEM:
        break;
      case ZenLoad::DEF_EXCHANGE_ITEM: {
        if(!invent.clearSlot(*this,i.slot[0],true))
          invent.clearSlot(*this,"",true); // fallback for cooking animations
        if(auto it = invent.addItem(i.item,1,world())) {
          invent.putToSlot(*this,it->clsId(),i.slot[0]);
          }
        break;
        }

      case ZenLoad::DEF_FIGHTMODE:
        break;
      case ZenLoad::DEF_PLACE_MUNITION: {
        auto active=invent.activeWeapon();
        if(active!=nullptr) {
          const int32_t munition = active->handle().munition;
          invent.putAmmunition(*this,uint32_t(munition),i.slot[0]);
          }
        break;
        }
      case ZenLoad::DEF_REMOVE_MUNITION: {
        invent.putAmmunition(*this,0,"");
        break;
        }
      case ZenLoad::DEF_DRAWSOUND:
      case ZenLoad::DEF_UNDRAWSOUND:
        break;
      case ZenLoad::DEF_SWAPMESH:
        break;

      case ZenLoad::DEF_DRAWTORCH:
        visual.setTorch(true,owner);
        break;
      case ZenLoad::DEF_INV_TORCH:
        // toogleTorch();
        break;
      case ZenLoad::DEF_DROP_TORCH:
        dropTorch();
        break;

      case ZenLoad::DEF_HIT_LIMB:
      case ZenLoad::DEF_HIT_DIR:
      case ZenLoad::DEF_DAM_MULTIPLY:
      case ZenLoad::DEF_PAR_FRAME:
      case ZenLoad::DEF_OPT_FRAME:
      case ZenLoad::DEF_HIT_END:
      case ZenLoad::DEF_WINDOW:
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
  visual.processEvents(owner,lastEventTime,ev);
  visual.processLayers(owner);
  visual.setNpcEffect(owner,*this,hnpc.effect,hnpc.flags);
  for(auto& i:ev.morph)
    visual.startMMAnim(*this,i.anim,i.node);

  if(ev.groundSounds>0 && isPlayer() && (bodyState()&BodyState::BS_SNEAK)==BodyState::BS_SNEAK)
    world().sendPassivePerc(*this,*this,*this,PERC_ASSESSQUIETSOUND);
  if(ev.def_opt_frame>0)
    commitDamage();
  implSetFightMode(ev);
  tickTimedEvt(ev);
  }

void Npc::tick(uint64_t dt) {
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
        if(dmg>0)
          changeAttribute(ATR_HITPOINTS,-100*dmg,false);
        }
      }
    }

  nextAiAction(aiQueueOverlay,dt);

  if(tickCast())
    return;

  if(!isDead()) {
    tickRegen(hnpc.attribute[ATR_HITPOINTS],hnpc.attribute[ATR_HITPOINTSMAX],
              hnpc.attribute[ATR_REGENERATEHP],dt);
    tickRegen(hnpc.attribute[ATR_MANA],hnpc.attribute[ATR_MANAMAX],
              hnpc.attribute[ATR_REGENERATEMANA],dt);
    }

  if(waitTime>=owner.tickCount() || aniWaitTime>=owner.tickCount() || outWaitTime>owner.tickCount()) {
    if(!isPlayer() && faiWaitTime<owner.tickCount())
      adjustAtackRotation(dt);
    mvAlgo.tick(dt,MoveAlgo::WaitMove);
    return;
    }

  if(!isDown()) {
    implLookAt(dt);

    if(implAtack(dt))
      return;

    if(implGoTo(dt))
      return;
    }

  mvAlgo.tick(dt);
  implAiTick(dt);
  }

void Npc::nextAiAction(AiQueue& queue, uint64_t dt) {
  if(queue.size()==0)
    return;
  auto act = queue.pop();
  switch(act.act) {
    case AI_None: break;
    case AI_LookAt:{
      currentLookAt=act.target;
      break;
      }
    case AI_TurnToNpc: {
      if(interactive()==nullptr)
        visual.stopWalkAnim(*this);
      if(act.target!=nullptr && implTurnTo(*act.target,dt)) {
        queue.pushFront(std::move(act));
        }
      break;
      }
    case AI_GoToNpc:
      if(!setInteraction(nullptr)) {
        queue.pushFront(std::move(act));
        break;
        }
      currentFp       = nullptr;
      currentFpLock   = FpLock();
      go2.set(act.target);
      wayPath.clear();
      break;
    case AI_GoToNextFp: {
      if(!setInteraction(nullptr)) {
        queue.pushFront(std::move(act));
        break;
        }
      auto fp = owner.findNextFreePoint(*this,act.s0.c_str());
      if(fp!=nullptr) {
        currentFp       = nullptr;
        currentFpLock   = FpLock(*fp);
        go2.set(fp,GoToHint::GT_NextFp);
        wayPath.clear();
        }
      break;
      }
    case AI_GoToPoint: {
      if(!setInteraction(nullptr)) {
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
      currentLookAt=nullptr;
      visual.setHeadRotation(0,0);
      break;
    case AI_RemoveWeapon:
      if(!isDead()) {
        if(closeWeapon(false)) {
          stopWalkAnimation();
          }
        if(weaponState()!=WeaponState::NoWeapon){
          queue.pushFront(std::move(act));
          }
        }
      break;
    case AI_StartState:
      if(startState(act.func,act.s0,aiState.eTime,act.i0==0)) {
        setOther(act.target);
        setVictum(act.victum);
        }
      break;
    case AI_PlayAnim:{
      if(auto sq = playAnimByName(act.s0.c_str(),BS_NONE)) {
        implAniWait(uint64_t(sq->totalTime()));
        } else {
        if(visual.isAnimExist(act.s0.c_str()))
          queue.pushFront(std::move(act));
        }
      break;
      }
    case AI_PlayAnimBs:{
      BodyState bs = BodyState(act.i0);
      if(auto sq = playAnimByName(act.s0.c_str(),bs)) {
        implAniWait(uint64_t(sq->totalTime()));
        } else {
        if(visual.isAnimExist(act.s0.c_str())) {
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
      implAiWait(uint64_t(act.i0));
      break;
    case AI_StandUp:
    case AI_StandUpQuick:
      // NOTE: B_ASSESSTALK calls AI_StandUp, to make npc stand, if it's not on a chair or something
      if(interactive()!=nullptr) {
        // if(interactive()->stateMask()==BS_SIT)
        //   ;
        if(!setInteraction(nullptr,act.act==AI_StandUpQuick)) {
          queue.pushFront(std::move(act));
          }
        break;
        }
      else if(bodyStateMasked()==BS_UNCONSCIOUS) {
        if(!setAnim(Anim::Idle))
          queue.pushFront(std::move(act)); else
          implAniWait(visual.pose().animationTotalTime());
        }
      else if(bodyStateMasked()!=BS_DEAD) {
        setAnim(Anim::Idle);
        }
      break;
    case AI_EquipArmor:
      invent.equipArmour(act.i0,*this);
      break;
    case AI_EquipBestArmor:
      invent.equipBestArmour(*this);
      break;
    case AI_EquipMelee:
      invent.equipBestMeleWeapon(*this);
      break;
    case AI_EquipRange:
      invent.equipBestRangeWeapon(*this);
      break;
    case AI_UseMob: {
      if(act.i0<0) {
        if(!setInteraction(nullptr))
          queue.pushFront(std::move(act));
        break;
        }
      if(owner.script().isTalk(*this)) {
        queue.pushFront(std::move(act));
        break;
        }
      auto inter = owner.aviableMob(*this,act.s0.c_str());
      if(inter==nullptr) {
        queue.pushFront(std::move(act));
        break;
        }

      if(inter!=currentInteract) {
        auto pos = inter->nearestPoint(*this);
        auto dp  = pos-position();
        dp.y = 0;
        if(dp.quadLength()>MAX_AI_USE_DISTANCE*MAX_AI_USE_DISTANCE) { // too far
          go2.set(pos);
          // go to MOBSI and then complete AI_UseMob
          queue.pushFront(std::move(act));
          return;
          }
        if(!setInteraction(inter)) {
          // queue.pushFront(std::move(act));
          }
        }

      if(currentInteract==nullptr || currentInteract->stateId()!=act.i0) {
        queue.pushFront(std::move(act));
        }
      break;
      }
    case AI_UseItem:
      if(act.i0!=0)
        useItem(uint32_t(act.i0));
      break;
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
      setPosition(act.point->x,act.point->y,act.point->z);
      }
      break;
    case AI_DrawWeapon:
      if(!isDead()) {
        fghAlgo.onClearTarget();
        if(!drawWeaponMele() &&
           !drawWeaponBow())
          queue.pushFront(std::move(act));
        }
      break;
    case AI_DrawWeaponMele:
      if(!isDead()) {
        fghAlgo.onClearTarget();
        if(!drawWeaponMele())
          queue.pushFront(std::move(act));
        }
      break;
    case AI_DrawWeaponRange:
      if(!isDead()) {
        fghAlgo.onClearTarget();
        if(!drawWeaponBow())
          queue.pushFront(std::move(act));
        }
      break;
    case AI_DrawSpell: {
      if(!isDead()) {
        const int32_t spell = act.i0;
        fghAlgo.onClearTarget();
        if(!drawSpell(spell))
          queue.pushFront(std::move(act));
        }
      break;
      }
    case AI_Atack:
      if(currentTarget!=nullptr && weaponState()!=WeaponState::NoWeapon){
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
      invent.unequipArmour(owner.script(),*this);
      break;
    case AI_Output:
    case AI_OutputSvm:
    case AI_OutputSvmOverlay:{
      if(performOutput(act)) {
        if(aiPolicy!=ProcessPolicy::AiFar2) {
          uint64_t msgTime = 0;
          if(act.act==AI_Output) {
            msgTime = owner.script().messageTime(act.s0);
            } else {
            auto& svm = owner.script().messageFromSvm(act.s0,hnpc.voice);
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
    case AI_ProcessInfo:
      if(act.target==nullptr)
        break;

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
    case AI_ContinueRoutine:{
      clearState(false);
      auto& r = currentRoutine();
      auto  t = endTime(r);
      if(r.callback.isValid())
        startState(r.callback,r.point ? r.point->name : "",t,false);
      break;
      }
    case AI_AlignToWp:
    case AI_AlignToFp:{
      if(auto fp = currentFp){
        if(fp->dirX!=0.f || fp->dirZ!=0.f){
          if(implTurnTo(fp->dirX,fp->dirZ,false,dt))
            queue.pushFront(std::move(act));
          }
        }
      break;
      }
    case AI_SetNpcsToState:{
      const int32_t r = act.i0*act.i0;
      owner.detectNpc(position(),float(hnpc.senses_range),[&act,this,r](Npc& other) {
        if(&other==this)
          return;
        if(other.isDead())
          return;
        if(qDistTo(other)>float(r))
          return;
        other.aiPush(AiQueue::aiStartState(act.func,1,other.currentOther,other.currentVictum,other.hnpc.wp.c_str()));
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

      if(!fghAlgo.isInAtackRange(*this,*act.target,owner.script())){
        queue.pushFront(std::move(act));
        implGoTo(dt,fghAlgo.prefferedAtackDistance(*this,*act.target,owner.script()));
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
    }
  }

bool Npc::startState(ScriptFn id, const Daedalus::ZString& wp) {
  return startState(id,wp,gtime::endOfTime(),false);
  }

bool Npc::startState(ScriptFn id, const Daedalus::ZString& wp, gtime endTime, bool noFinalize) {
  if(!id.isValid())
    return false;
  if(aiState.funcIni==id)
    return false;

  clearState(noFinalize);
  if(!wp.empty())
    hnpc.wp = wp;

  for(size_t i=0;i<PERC_Count;++i)
    setPerceptionDisable(PercType(i));

  auto& st = owner.script().aiState(id);
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
      owner.script().invokeState(this,currentOther,currentVictum,aiState.funcEnd);  // cleanup
    aiPrevState = aiState.funcIni;
    invent.putState(*this,0,0);
    visual.stopItemStateAnim(*this);
    }
  aiState = AiState();
  }

void Npc::tickRoutine() {
  if(!aiState.funcIni.isValid() && !isPlayer()) {
    auto r = currentRoutine();
    if(r.callback.isValid()) {
      if(r.point!=nullptr)
        hnpc.wp = r.point->name;
      auto t = endTime(r);
      startState(r.callback,r.point ? r.point->name : "",t,false);
      }
    else if(hnpc.start_aistate!=0) {
      startState(hnpc.start_aistate,"");
      }
    }

  if(aiState.funcIni==0)
    return;

  /*HACK: don't process far away Npc*/
  if(aiPolicy==Npc::ProcessPolicy::AiFar2 && routines.size()==0)
    return;

  if(aiState.started) {
    if(aiState.loopNextTime<=owner.tickCount()){
      aiState.loopNextTime+=1000; // one tick per second?
      int loop = 0;
      if(aiState.funcLoop.isValid()) {
        loop = owner.script().invokeState(this,currentOther,currentVictum,aiState.funcLoop);
        } else {
        // ZS_DEATH   have no looping, in G1, G2 classic
        // ZS_GETMEAT have no looping, at all
        loop = owner.version().hasZSStateLoop() ? 1 : 0;
        }

      if(aiState.eTime<=owner.time()) {
        if(!isTalk()) {
          loop=1; // have to hack ZS_Talk bugs
          }
        }
      if(loop!=0) {
        clearState(false);
        currentOther  = nullptr;
        currentVictum = nullptr;
        }
      }
    } else {
    aiState.started=true;
    owner.script().invokeState(this,currentOther,currentVictum,aiState.funcIni);
    }
  }

void Npc::setTarget(Npc *t) {
  if(currentTarget==t)
    return;

  currentTarget=t;
  if(!go2.empty() && !isPlayer()) {
    stopWalking();
    go2.clear();
    }
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

void Npc::setVictum(Npc* ot) {
  currentVictum = ot;
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
  auto sfx = ::Sound(owner,::Sound::T_Regular,sound,{x,y+translateY(),z},range,freeSlot);
  sfx.play();
  }

void Npc::emitSoundGround(std::string_view sound, float range, bool freeSlot) {
  char    buf[256]={};
  uint8_t mat = mvAlgo.groundMaterial();
  std::snprintf(buf,sizeof(buf),"%.*s_%s",int(sound.size()),sound.data(),ZenLoad::zCMaterial::getMatGroupString(ZenLoad::MaterialGroup(mat)));
  auto sfx = ::Sound(owner,::Sound::T_Regular,buf,{x,y,z},range,freeSlot);
  sfx.play();
  }

void Npc::emitSoundSVM(std::string_view svm) {
  if(hnpc.voice==0)
    return;
  char frm [32]={};
  std::snprintf(frm,sizeof(frm),"%.*s",int(svm.size()),svm.data());

  char name[32]={};
  std::snprintf(name,sizeof(name),frm,int(hnpc.voice));
  emitSoundEffect(name,25,true);
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

void Npc::commitSpell() {
  auto active = invent.getItem(currentSpellCast);
  if(active==nullptr || !active->isSpellOrRune())
    return;

  const int32_t splId = active->spellId();
  const auto&   spl   = owner.script().spellDesc(splId);

  owner.script().invokeSpell(*this,currentTarget,*active);

  if(active->isSpellShoot()) {
    int   lvl = (castLevel-CS_Cast_0)+1;
    DamageCalculator::Damage dmg={};
    for(size_t i=0; i<DamageCalculator::DAM_INDEX_MAX; ++i)
      if((spl.damageType&(1<<i))!=0) {
        dmg[i] = spl.damage_per_level*lvl;
        }

    auto& b = owner.shootSpell(*active, *this, currentTarget);
    b.setDamage(dmg);
    b.setHitChance(1.f);
    b.setOrigin(this);
    b.setTarget((currentTarget==nullptr) ? this : currentTarget);
    visual.setMagicWeaponKey(owner,SpellFxKey::Init);
    } else {
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

    owner.script().initializeInstance(hnpc,size_t(spellInfo));
    spellInfo  = 0;
    hnpc.level = transformSpl->hnpc.level;
    }
  }

const Npc::Routine& Npc::currentRoutine() const {
  auto time = owner.time();
  time = gtime(int32_t(time.hour()),int32_t(time.minute()));
  for(auto& i:routines){
    if(i.end<i.start && (time<i.end || i.start<=time))
      return i;
    if(i.start<=time && time<i.end)
      return i;
    }

  static Routine r;
  return r;
  }

gtime Npc::endTime(const Npc::Routine &r) const {
  auto wtime = owner.time();
  auto time  = gtime(int32_t(wtime.hour()),int32_t(wtime.minute()));

  if(r.end<r.start){
    if(r.start<=time) {
      return gtime(wtime.day()+1,r.end.hour(),r.end.minute());
      }
    if(time<r.end) {
      return gtime(wtime.day(),r.end.hour(),r.end.minute());
      }
    }
  if(r.start<=time && time<r.end) {
    return gtime(wtime.day(),r.end.hour(),r.end.minute());
    }
  // error - routine is not active now
  return wtime;
  }

BodyState Npc::bodyState() const {
  if(isDead())
    return BS_DEAD;
  if(isUnconscious())
    return BS_UNCONSCIOUS;
  if(isFaling())
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

void Npc::setToFightMode(const size_t item) {
  if(invent.itemCount(item)==0)
    addItem(item,1);

  invent.equip(item,*this,true);
  invent.switchActiveWeapon(*this,1);

  auto w = invent.currentMeleWeapon();
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

  auto& weapon = *currentMeleWeapon();
  auto  st     = weapon.is2H() ? WeaponState::W2H : WeaponState::W1H;
  hnpc.weapon  = (st==WeaponState::W1H ? 3:4);
  }

void Npc::setToFistMode() {
  auto weaponSt=weaponState();
  if(weaponSt==WeaponState::Fist)
    return;
  invent.switchActiveWeaponFist();
  if(visual.setToFightMode(WeaponState::Fist))
    updateWeaponSkeleton();
  hnpc.weapon  = 1;
  }

void Npc::aiPush(AiQueue::AiAction&& a) {
  if(a.act==AI_OutputSvmOverlay)
    aiQueueOverlay.pushBack(std::move(a)); else
    aiQueue.pushBack(std::move(a));
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

  auto dpos = item.position()-position();
  dpos.y-=translateY();

  if(bodyStateMasked()!=BS_STAND) {
    setAnim(Anim::Idle);
    return nullptr;
    }

  const Animation::Sequence* sq = setAnimAngGet(Npc::Anim::ItmGet,Pose::calcAniCombVert(dpos));
  if(sq==nullptr)
    return nullptr;

  std::unique_ptr<Item> ptr = owner.takeItem(item);
  if(ptr!=nullptr && ptr->isTorchBurn()) {
   if(!toogleTorch())
     return nullptr;
    size_t torchId = owner.script().getSymbolIndex("ItLsTorch");
    if(torchId!=size_t(-1))
      return nullptr;
    ptr.reset(new Item(owner,torchId,Item::T_Inventory));
    }

  auto it = ptr.get();
  if(it==nullptr)
    return nullptr;

  it = addItem(std::move(ptr));
  if(isPlayer() && it!=nullptr) // && (it->handle().owner!=0 || it->handle().ownerGuild!=0))
    owner.sendPassivePerc(*this,*this,*this,*it,PERC_ASSESSTHEFT);

  implAniWait(uint64_t(sq->totalTime()));
  return it;
  }

void Npc::onWldItemRemoved(const Item& itm) {
  aiQueue.onWldItemRemoved(itm);
  aiQueueOverlay.onWldItemRemoved(itm);
  }

void Npc::addItem(size_t id, Interactive &chest, size_t count) {
  Inventory::trasfer(invent,chest.inventory(),nullptr,id,count,owner);
  }

void Npc::addItem(size_t id, Npc &from, size_t count) {
  Inventory::trasfer(invent,from.invent,&from,id,count,owner);
  }

void Npc::moveItem(size_t id, Interactive &to, size_t count) {
  Inventory::trasfer(to.inventory(),invent,this,id,count,owner);
  }

void Npc::sellItem(size_t id, Npc &to, size_t count) {
  if(id==owner.script().goldId())
    return;
  int32_t price = invent.sellPriceOf(id);
  Inventory::trasfer(to.invent,invent,this,id,count,owner);
  invent.addItem(owner.script().goldId(),size_t(price),owner);
  }

void Npc::buyItem(size_t id, Npc &from, size_t count) {
  if(id==owner.script().goldId())
    return;

  int32_t price = from.invent.priceOf(id);
  if(price>0 && size_t(price)*count>invent.goldCount()) {
    count = invent.goldCount()/size_t(price);
    }
  if(count==0) {
    owner.script().printCannotBuyError(*this);
    return;
    }

  Inventory::trasfer(invent,from.invent,nullptr,id,count,owner);
  if(price>=0)
    invent.delItem(owner.script().goldId(),size_t( price)*count,*this); else
    invent.addItem(owner.script().goldId(),size_t(-price)*count,owner);
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

  size_t leftHand = sk->findNode("ZS_LEFTHAND");
  if(leftHand==size_t(-1))
    return;

  if(!setAnim(Anim::ItmDrop))
    return;

  auto mat = visual.transform();
  if(leftHand<visual.pose().boneCount())
    mat.mul(visual.pose().bone(leftHand));

  auto it = owner.addItemDyn(id,mat,hnpc.instanceSymbol);
  it->setCount(count);
  invent.delItem(id,count,*this);
  }

void Npc::clearInventory() {
  invent.clear(owner.script(),*this);
  }

Item *Npc::currentArmour() {
  return invent.currentArmour();
  }

Item *Npc::currentMeleWeapon() {
  return invent.currentMeleWeapon();
  }

Item *Npc::currentRangeWeapon() {
  return invent.currentRangeWeapon();
  }

Vec3 Npc::mapWeaponBone() const {
  return visual.mapWeaponBone();
  }

Vec3 Npc::mapBone(std::string_view bone) const {
  if(auto sk = visual.visualSkeleton()) {
    size_t id = sk->findNode(bone);
    if(id!=size_t(-1))
      return visual.mapBone(id);
    }

  Vec3 ret = {};
  ret.y = centerY()-y;
  return ret;
  }

bool Npc::turnTo(float dx, float dz, bool anim, uint64_t dt) {
  return implTurnTo(dx,dz,anim,dt);
  }

bool Npc::rotateTo(float dx, float dz, float step, bool noAnim, uint64_t dt) {
  step *= (float(dt)/1000.f)*60.f/100.f;

  if(dx==0.f && dz==0.f) {
    setAnimRotate(0);
    return false;
    }

  if(isPrehit() || isFinishingMove() || interactive()!=nullptr)
    return false;

  float a  = angleDir(dx,dz);
  float da = a-angle;

  if(noAnim || std::cos(double(da)*M_PI/180.0)>0) {
    if(float(std::abs(int(da)%180))<=step) {
      setAnimRotate(0);
      setDirection(a);
      return false;
      }
    } else {
    visual.stopWalkAnim(*this);
    }

  const auto sgn = std::sin(double(da)*M_PI/180.0);
  if(sgn<0) {
    setAnimRotate( 1);
    setDirection(angle-step);
    } else
  if(sgn>0) {
    setAnimRotate(-1);
    setDirection(angle+step);
    } else {
    setAnimRotate(0);
    }
  return true;
  }

bool Npc::checkGoToNpcdistance(const Npc &other) {
  return fghAlgo.isInAtackRange(*this,other,owner.script());
  }

size_t Npc::hasItem(size_t id) const {
  return invent.itemCount(id);
  }

Item *Npc::getItem(size_t id) {
  return invent.getItem(id);
  }

void Npc::delItem(size_t item, uint32_t amount) {
  invent.delItem(item,amount,*this);
  }

void Npc::useItem(size_t item,bool force) {
  invent.use(item,*this,force);
  }

void Npc::setCurrentItem(size_t item) {
  invent.setCurrentItem(item);
  }

void Npc::unequipItem(size_t item) {
  invent.unequip(item,*this);
  }

bool Npc::canSwitchWeapon() const {
  return !(mvAlgo.isFaling() || mvAlgo.isInAir() || mvAlgo.isSlide() || mvAlgo.isSwim());
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
  hnpc.weapon      = 0;
  // clear spell-cast state
  castLevel        = CS_NoCast;
  currentSpellCast = size_t(-1);
  castNextTime     = 0;
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
  hnpc.weapon = 1;
  return true;
  }

bool Npc::drawWeaponMele() {
  if(!canSwitchWeapon())
    return false;
  auto weaponSt=weaponState();
  if(weaponSt==WeaponState::Fist || weaponSt==WeaponState::W1H || weaponSt==WeaponState::W2H)
    return true;
  if(invent.currentMeleWeapon()==nullptr)
    return drawWeaponFist();
  if(weaponSt!=WeaponState::NoWeapon) {
    closeWeapon(false);
    return false;
    }

  if(!setInteraction(nullptr,true))
    return false;

  auto& weapon = *invent.currentMeleWeapon();
  auto  st     = weapon.is2H() ? WeaponState::W2H : WeaponState::W1H;
  if(!visual.startAnim(*this,st))
    return false;

  invent.switchActiveWeapon(*this,1);
  hnpc.weapon = (st==WeaponState::W1H ? 3:4);
  return true;
  }

bool Npc::drawWeaponBow() {
  if(!canSwitchWeapon())
    return false;
  auto weaponSt=weaponState();
  if(weaponSt==WeaponState::Bow || weaponSt==WeaponState::CBow || invent.currentRangeWeapon()==nullptr)
    return true;
  if(weaponSt!=WeaponState::NoWeapon) {
    closeWeapon(false);
    return false;
    }

  if(!setInteraction(nullptr,true))
    return false;

  auto& weapon = *invent.currentRangeWeapon();
  auto  st     = weapon.isCrossbow() ? WeaponState::CBow : WeaponState::Bow;
  if(!visual.startAnim(*this,st))
    return false;
  invent.switchActiveWeapon(*this,2);
  hnpc.weapon = (st==WeaponState::W1H ? 5:6);
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
  if(isFaling() || mvAlgo.isSwim() || bodyStateMasked()==BS_CASTING)
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
  hnpc.weapon = 7;

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
  if(!oth.isUnconscious() || !fghAlgo.isInAtackRange(*this,oth,owner.script()))
    return false;
  return true;
  }

bool Npc::doAttack(Anim anim) {
  auto weaponSt=weaponState();
  if(weaponSt==WeaponState::NoWeapon || weaponSt==WeaponState::Mage)
    return false;

  if(mvAlgo.isSwim())
    return false;

  auto wlk = walkMode();
  if(mvAlgo.isInWater())
    wlk = WalkBit::WM_Water;

  visual.setAnimRotate(*this,0);
  if(auto sq = visual.continueCombo(*this,anim,weaponSt,wlk)) {
    implAniWait(uint64_t(sq->atkTotalTime(visual.comboLength())+1));
    return true;
    }
  return false;
  }

void Npc::fistShoot() {
  doAttack(Anim::Atack);
  }

bool Npc::blockFist() {
  auto weaponSt=weaponState();
  if(weaponSt!=WeaponState::Fist)
    return false;
  visual.setAnimRotate(*this,0);
  return setAnim(Anim::AtackBlock);
  }

bool Npc::finishingMove() {
  if(currentTarget==nullptr || !canFinish(*currentTarget))
    return false;

  if(doAttack(Anim::AtackFinish)) {
    currentTarget->hnpc.attribute[ATR_HITPOINTS] = 0;
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
  doAttack(Anim::Atack);
  }

bool Npc::swingSwordL() {
  auto active=invent.activeWeapon();
  if(active==nullptr)
    return false;
  return doAttack(Anim::AtackL);
  }

bool Npc::swingSwordR() {
  auto active=invent.activeWeapon();
  if(active==nullptr)
    return false;
  return doAttack(Anim::AtackR);
  }

bool Npc::blockSword() {
  auto active=invent.activeWeapon();
  if(active==nullptr)
    return false;
  return doAttack(Anim::AtackBlock);
  // return setAnimAngGet(Anim::AtackBlock,calcAniComb())!=nullptr;
  }

bool Npc::beginCastSpell() {
  if(castLevel!=CS_NoCast)
    return false;

  if(!isStanding())
    return false;

  auto active=invent.activeWeapon();
  if(active==nullptr)
    return false;

  castLevel        = CS_Invest_0;
  currentSpellCast = active->clsId();
  castNextTime     = owner.tickCount();
  hnpc.aivar[88]   = 0; // HACK: clear AIV_SpellLevel

  const SpellCode code = SpellCode(owner.script().invokeMana(*this,currentTarget,*active));
  switch(code) {
    case SpellCode::SPL_SENDSTOP:
    case SpellCode::SPL_DONTINVEST:
    case SpellCode::SPL_STATUS_CANINVEST_NO_MANADEC:
      setAnim(Anim::MagNoMana);
      castLevel        = CS_NoCast;
      currentSpellCast = size_t(-1);
      castNextTime     = 0;
      return false;
    case SpellCode::SPL_NEXTLEVEL: {
      auto& ani = owner.script().spellCastAnim(*this,*active);
      if(!visual.startAnimSpell(*this,ani.c_str(),true)) {
        endCastSpell();
        return false;
        }
      break;
      }
    case SpellCode::SPL_SENDCAST:{
      auto& ani = owner.script().spellCastAnim(*this,*active);
      visual.startAnimSpell(*this,ani.c_str(),false);
      endCastSpell();
      return false;
      }
    default:
      Log::d("unexpected Spell_ProcessMana result: '",int(code),"' for spell '",currentSpellCast,"'");
      endCastSpell();
      return false;
    }
  return true;
  }

bool Npc::tickCast() {
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
    }

  if(CS_Cast_0<=castLevel && castLevel<=CS_Cast_Last) {
    // cast anim
    if(active!=nullptr) {
      auto& ani = owner.script().spellCastAnim(*this,*active);
      if(!visual.startAnimSpell(*this,ani.c_str(),false))
        return true;
      }
    commitSpell();
    castLevel        = CS_Finalize;
    currentSpellCast = size_t(-1);
    castNextTime     = 0;
    return true;
    }

  if(castLevel==CS_Finalize) {
    // final commit
    if(isAiQueueEmpty()) {
      if(!setAnim(Npc::Anim::Idle))
        return true;
      }
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

  auto& spl = owner.script().spellDesc(active->spellId());
  int32_t castLvl = int(castLevel)-int(CS_Invest_0);
  if(owner.tickCount()<castNextTime)
    return true;

  if(castLvl<=15)
    castLevel = CastState(castLevel+1);

  int32_t mana = attribute(ATR_MANA);
  const SpellCode code = SpellCode(owner.script().invokeMana(*this,currentTarget,*active));
  mana = std::max(mana-attribute(ATR_MANA),0);

  switch(code) {
    case SpellCode::SPL_NEXTLEVEL: {
      visual.setMagicWeaponKey(owner,SpellFxKey::Invest,castLvl+1);
      castNextTime += uint64_t(spl.time_per_mana*float(mana));
      break;
      }
    case SpellCode::SPL_SENDSTOP:
    case SpellCode::SPL_STATUS_CANINVEST_NO_MANADEC:
    case SpellCode::SPL_DONTINVEST:
    case SpellCode::SPL_SENDCAST: {
      endCastSpell();
      return true;
      }
    default:
      Log::d("unexpected Spell_ProcessMana result: '",int(code),"' for spell '",currentSpellCast,"'");
      return false;
    }
  return true;
  }

void Npc::endCastSpell() {
  int32_t castLvl = int(castLevel)-int(CS_Invest_0);
  if(castLvl<0)
    return;
  castLevel = CastState(castLvl+CS_Cast_0);
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

bool Npc::castSpell() {
  if(!beginCastSpell())
    return true;
  endCastSpell();
  return true;
  }

bool Npc::aimBow() {
  auto active=invent.activeWeapon();
  if(active==nullptr)
    return false;
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
  if(bs==BS_RUN) {
    setAnim(Anim::Idle);
    return true;
    }

  const int32_t munition = active->handle().munition;
  if(!hasAmunition())
    return false;

  if(!setAnim(Anim::Atack))
    return false;

  auto itm = invent.getItem(size_t(munition));
  if(itm==nullptr)
    return false;
  auto& b = owner.shootBullet(*itm,*this,currentTarget,focOverride);

  invent.delItem(size_t(munition),1,*this);
  b.setOrigin(this);
  b.setDamage(DamageCalculator::rangeDamageValue(*this));

  auto rgn = currentRangeWeapon();
  if(rgn!=nullptr && rgn->isCrossbow())
    b.setHitChance(float(hnpc.hitChance[TALENT_CROSSBOW])/100.f); else
    b.setHitChance(float(hnpc.hitChance[TALENT_BOW]     )/100.f);

  return true;
  }

bool Npc::hasAmunition() const {
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

bool Npc::isUnconscious() const {
  return owner.script().isUnconscious(*this);
  }

bool Npc::isDown() const {
  return isUnconscious() || isDead();
  }

bool Npc::isAtack() const {
  return owner.script().isAtack(*this);
  }

bool Npc::isTalk() const {
  return owner.script().isTalk(*this);
  }

bool Npc::isAtackAnim() const {
  return visual.pose().isAtackAnim();
  }

bool Npc::isPrehit() const {
  return visual.pose().isPrehit(owner.tickCount());
  }

bool Npc::isImmortal() const {
  return hnpc.flags & Daedalus::GEngineClasses::C_Npc::ENPCFlag::EFLAG_IMMORTAL;
  }

void Npc::setPerceptionTime(uint64_t time) {
  perceptionTime = time;
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
  static bool disable=false;
  if(disable)
    return false;

  if(isPlayer())
    return true;

  bool ret=false;
  if(hasPerc(PERC_MOVEMOB) && interactive()==nullptr) {
    if(moveMobCacheKey!=position()) {
      moveMob         = owner.findInteractive(*this);
      moveMobCacheKey = position();
      }
    if(moveMob!=nullptr && perceptionProcess(*this,nullptr,0,PERC_MOVEMOB)) {
      ret = true;
      }
    }

  if(processPolicy()!=Npc::AiNormal)
    return ret;

  const float quadDist = pl.qDistTo(*this);

  if(hasPerc(PERC_ASSESSPLAYER) && canSenseNpc(pl,false)!=SensesBit::SENSE_NONE) {
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

  perceptionNextTime=owner.tickCount()+perceptionTime;
  return ret;
  }

bool Npc::perceptionProcess(Npc &pl, Npc* victum, float quadDist, PercType perc) {
  float r = float(hnpc.senses_range);
  r = r*r;
  if(quadDist>r)
    return false;
  if(hasPerc(perc)) {
    owner.script().invokeState(this,&pl,victum,perception[perc].func);
    return true;
    }
  if(perc==PERC_ASSESSMAGIC && isPlayer()) {
    auto defaultFn = owner.script().playerPercAssessMagic();
    if(defaultFn.isValid())
      owner.script().invokeState(this,&pl,victum,defaultFn);
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

Interactive* Npc::detectedMob() const {
  if(currentInteract!=nullptr)
    return currentInteract;
  return moveMob;
  }

bool Npc::setInteraction(Interactive *id, bool quick) {
  if(currentInteract==id)
    return true;

  if(currentInteract!=nullptr) {
    currentInteract->dettach(*this,quick);
    return false;
    }

  if(id->attach(*this)) {
    currentInteract = id;
    if(!quick) {
      visual.stopAnim(*this,"");
      setAnimRotate(0);
      }
    return true;
    }

  return false;
  }

void Npc::quitIneraction() {
  currentInteract=nullptr;
  }

bool Npc::isState(ScriptFn stateFn) const {
  return aiState.funcIni==stateFn;
  }

bool Npc::isRoutine(ScriptFn stateFn) const {
  auto& rout = currentRoutine();
  return rout.callback==stateFn && aiState.funcIni==stateFn;
  }

bool Npc::wasInState(ScriptFn stateFn) const {
  return aiPrevState==stateFn;
  }

uint64_t Npc::stateTime() const {
  return owner.tickCount()-aiState.sTime;
  }

void Npc::setStateTime(int64_t time) {
  aiState.sTime = owner.tickCount()-uint64_t(time);
  }

void Npc::addRoutine(gtime s, gtime e, uint32_t callback, const WayPoint *point) {
  Routine r;
  r.start    = s;
  r.end      = e;
  r.callback = callback;
  r.point    = point;
  routines.push_back(r);
  }

void Npc::excRoutine(size_t callback) {
  routines.clear();
  owner.script().invokeState(this,currentOther,currentVictum,callback);
  aiState.eTime = gtime();
  }

void Npc::multSpeed(float s) {
  mvAlgo.multSpeed(s);
  }

bool Npc::testMove(const Tempest::Vec3& pos) {
  DynamicWorld::CollisionTest out;
  return physic.testMove(pos,out);
  }

bool Npc::tryMove(const Tempest::Vec3& dp) {
  DynamicWorld::CollisionTest out;
  return tryMove(dp, out);
  }

bool Npc::tryMove(const Vec3& dp, DynamicWorld::CollisionTest& out) {
  return tryTranslate(Vec3(x,y,z) + dp, out);
  }

bool Npc::tryTranslate(const Tempest::Vec3& to) {
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
  Vec3  dp  = Vec3{len*s, 0, -len*c};

  auto& g  = owner.script().guildVal();
  auto  gl = guild();

  if(isSlide() || isSwim() || isDive()) {
    JumpStatus ret;
    ret.anim   = Anim::Idle;
    return ret;
    }

  const float jumpLow = float(g.jumplow_height[gl]);
  const float jumpMid = float(g.jumpmid_height[gl]);
  const float jumpUp  = float(g.jumpup_height[gl]);

  auto pos0 = position();

  JumpStatus ret;
  DynamicWorld::CollisionTest info;
  if(!isInAir() && physic.testMove(pos0+dp,info)) {
    // jump forward
    ret.anim = Anim::Jump;
    return ret;
    }

  auto  lnd   = owner.physic()->landRay(pos0 + dp + Vec3(0, jumpUp + jumpLow, 0));
  float jumpY = lnd.v.y;
  auto  pos1  = Vec3(pos0.x,jumpY,pos0.z);
  auto  pos2  = pos1 + dp;

  float dY    = jumpY - y;

  if(dY<=0.f ||
     !physic.testMove(pos2,pos1,info)) {
    ret.anim   = Anim::JumpUp;
    ret.height = y + jumpUp;
    return ret;
    }

  if(!physic.testMove(pos1,pos0,info)) {
    // check approximate path of climb failed
    ret.anim = Anim::Jump;
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
    ret.anim = Anim::Jump;
    return ret;
    }

  if(isInAir() && dY<=jumpLow + translateY()) {
    // jumpup -> climb
    ret.anim   = Anim::JumpHang;
    ret.height = jumpY;
    return ret;
    }

  if(isInAir()) {
    ret.anim   = Anim::Idle;
    return ret;
    }

  if(dY<=jumpLow) {
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
  }

std::vector<GameScript::DlgChoise> Npc::dialogChoises(Npc& player,const std::vector<uint32_t> &except,bool includeImp) {
  return owner.script().dialogChoises(&player.hnpc,&this->hnpc,except,includeImp);
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
  aiQueue.clear();
  aiQueueOverlay.clear();
  waitTime    = 0;
  faiWaitTime = 0;
  fghAlgo.onClearTarget();
  clearGoTo();
  }

bool Npc::isInState(ScriptFn fn) const {
  return aiState.funcIni==fn;
  }

void Npc::attachToPoint(const WayPoint *p) {
  currentFp     = p;
  currentFpLock = FpLock(currentFp);
  }

void Npc::clearGoTo() {
  wayPath.clear();
  if(!go2.empty()) {
    stopWalking();
    go2.clear();
    }
  }

void Npc::stopWalking() {
  if(setAnim(Anim::Idle))
    return;
  // hard stop
  visual.stopWalkAnim(*this);
  }

bool Npc::canSeeNpc(const Npc &oth, bool freeLos) const {
  return canSeeNpc(oth.x,oth.y+180,oth.z,freeLos);
  }

bool Npc::canSeeNpc(float tx, float ty, float tz, bool freeLos) const {
  SensesBit s = canSenseNpc(tx,ty,tz,freeLos,false);
  return int32_t(s&SensesBit::SENSE_SEE)!=0;
  }

SensesBit Npc::canSenseNpc(const Npc &oth, bool freeLos, float extRange) const {
  const bool isNoisy = (oth.bodyState()&BodyState::BS_SNEAK)==0;
  return canSenseNpc(oth.x,oth.y+180,oth.z,freeLos,isNoisy,extRange);
  }

SensesBit Npc::canSenseNpc(float tx, float ty, float tz, bool freeLos, bool isNoisy, float extRange) const {
  DynamicWorld* w = owner.physic();
  static const double ref = std::cos(100*M_PI/180.0); // spec requires +-100 view angle range

  const float range = float(hnpc.senses_range)+extRange;
  if(qDistTo(tx,ty,tz)>range*range)
    return SensesBit::SENSE_NONE;

  SensesBit ret=SensesBit::SENSE_NONE;
  if(owner.roomAt({tx,ty,tz})==owner.roomAt({x,y,z})) {
    ret = ret | SensesBit::SENSE_SMELL;
    if(isNoisy)
      ret = ret | SensesBit::SENSE_HEAR;
    }

  if(!freeLos){
    float dx  = x-tx, dz=z-tz;
    float dir = angleDir(dx,dz);
    float da  = float(M_PI)*(visual.viewDirection()-dir)/180.f;
    if(double(std::cos(da))<=ref)
      if(!w->ray(Vec3(x,y+180,z), Vec3(tx,ty,tz)).hasCol)
        ret = ret | SensesBit::SENSE_SEE;
    } else {
    // TODO: npc eyesight height
    if(!w->ray(Vec3(x,y+180,z), Vec3(tx,ty,tz)).hasCol)
      ret = ret | SensesBit::SENSE_SEE;
    }
  return ret & SensesBit(hnpc.senses);
  }

bool Npc::isAlignedToGround() const {
  auto gl = guild();
  return (owner.script().guildVal().surface_align[gl]!=0) || isDead();
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
    auto  dir  = Vec3(s,0,-c);
    auto  norm = Vec3::normalize(ground);

    float cx = Vec3::dotProduct(norm,dir);
    angY = -std::asin(cx)*180.f/float(M_PI);
    }

  Matrix4x4 mt = Matrix4x4();
  mt.identity();
  mt.translate(x,y,z);
  mt.rotateOY(180-angle);
  if(angY!=0)
    mt.rotateOX(-angY);
  if(isPlayer() && !align) {
    mt.rotateOZ(runAng);
    }
  mt.scale(sz[0],sz[1],sz[2]);
  return mt;
  }

void Npc::updatePos() {
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

  if(mvAlgo.isSwim()) {
    float chest = mvAlgo.canFlyOverWater() ? 0 : mvAlgo.waterDepthChest();
    float y = pos.at(3,1);
    pos.set(3,1,y+chest);
    }

  visual.setObjMatrix(pos,false);
  }


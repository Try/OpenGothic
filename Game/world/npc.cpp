#include "npc.h"

#include <Tempest/Matrix4x4>
#include <zenload/zCMaterial.h>

#include "interactive.h"
#include "graphics/visualfx.h"
#include "graphics/skeleton.h"
#include "game/serialize.h"
#include "game/gamescript.h"
#include "world/triggers/trigger.h"
#include "world.h"
#include "resources.h"

using namespace Tempest;

Npc::Npc(World &owner, size_t instance, const char* waypoint)
  :owner(owner),mvAlgo(*this){
  outputPipe          = owner.script().openAiOuput();

  hnpc.wp             = waypoint;
  hnpc.instanceSymbol = instance;
  hnpc.userPtr        = this;
  owner.script().initializeInstance(hnpc,instance);
  }

Npc::Npc(World &owner, Serialize &fin)
  :owner(owner),mvAlgo(*this){
  outputPipe = owner.script().openAiOuput();

  hnpc.userPtr = this;

  load(fin,hnpc);

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
  loadAiState(fin);

  fin.read(reinterpret_cast<uint8_t&>(currentGoToFlag),currentGoTo,currentFp,currentFpLock);
  wayPath.load(fin);

  mvAlgo.load(fin);
  fghAlgo.load(fin);
  fin.read(fghLastEventTime);
  }

Npc::~Npc(){
  if(currentInteract)
    currentInteract->dettach(*this);
  owner.script().clearReferences(hnpc);
  assert(hnpc.useCount==0);
  }

void Npc::save(Serialize &fout) {
  save(fout,hnpc);

  fout.write(x,y,z,angle,sz);
  fout.write(body,head,vHead,vTeeth,bdColor,vColor);
  fout.write(wlkMode,trGuild,talentsSk,talentsVl,refuseTalkMilis);
  visual.save(fout);

  fout.write(int32_t(permAttitude),int32_t(tmpAttitude));
  fout.write(perceptionTime,perceptionNextTime);
  for(auto& i:perception)
    fout.write(i.func);

  invent.save(fout);
  fout.write(lastHitType,lastHitSpell);
  saveAiState(fout);

  fout.write(uint8_t(currentGoToFlag),currentGoTo,currentFp,currentFpLock);
  wayPath.save(fout);

  mvAlgo.save(fout);
  fghAlgo.save(fout);
  fout.write(fghLastEventTime);
  }

void Npc::save(Serialize &fout, Daedalus::GEngineClasses::C_Npc &h) const {
  fout.write(uint32_t(h.instanceSymbol));
  fout.write(h.id,h.name,h.slot,h.effect,int32_t(h.npcType));
  save(fout,h.flags);
  fout.write(h.attribute,h.hitChance,h.protection,h.damage);
  fout.write(h.damagetype,h.guild,h.level);
  fout.write(h.mission);
  fout.write(h.fight_tactic,h.weapon,h.voice,h.voicePitch,h.bodymass);
  fout.write(h.daily_routine,h.start_aistate);
  fout.write(h.spawnPoint,h.spawnDelay,h.senses,h.senses_range);
  fout.write(h.aivar);
  fout.write(h.wp,h.exp,h.exp_next,h.lp,h.bodyStateInterruptableOverride,h.noFocus);
  }

void Npc::load(Serialize &fin, Daedalus::GEngineClasses::C_Npc &h) {
  uint32_t instanceSymbol=0;
  fin.read(instanceSymbol); h.instanceSymbol = instanceSymbol;
  fin.read(h.id,h.name,h.slot,h.effect, reinterpret_cast<int32_t&>(h.npcType));
  load(fin,h.flags);
  fin.read(h.attribute,h.hitChance,h.protection,h.damage);
  fin.read(h.damagetype,h.guild,h.level);
  fin.read(h.mission);
  fin.read(h.fight_tactic,h.weapon,h.voice,h.voicePitch,h.bodymass);
  fin.read(h.daily_routine,h.start_aistate);
  fin.read(h.spawnPoint,h.spawnDelay,h.senses,h.senses_range);
  fin.read(h.aivar);
  fin.read(h.wp,h.exp,h.exp_next,h.lp,h.bodyStateInterruptableOverride,h.noFocus);

  auto& sym = owner.script().getSymbol(hnpc.instanceSymbol);
  sym.instance.set(&hnpc, Daedalus::IC_Npc);
  }

void Npc::save(Serialize &fout, const Daedalus::GEngineClasses::C_Npc::ENPCFlag &flg) const {
  fout.write(int32_t(flg));
  }

void Npc::load(Serialize &fin, Daedalus::GEngineClasses::C_Npc::ENPCFlag &flg) {
  int32_t flags=0;
  fin.read(flags);
  flg=Daedalus::GEngineClasses::C_Npc::ENPCFlag(flags);
  }

void Npc::saveAiState(Serialize& fout) const {
  fout.write(waitTime,faiWaitTime,uint8_t(aiPolicy));
  fout.write(aiState.funcIni,aiState.funcLoop,aiState.funcEnd,aiState.sTime,aiState.eTime,aiState.started,aiState.loopNextTime);
  fout.write(aiPrevState);

  fout.write(uint32_t(aiActions.size()));
  for(auto& i:aiActions){
    fout.write(uint32_t(i.act));
    fout.write(i.point,i.func,i.i0,i.s0);
    }

  fout.write(uint32_t(routines.size()));
  for(auto& i:routines){
    fout.write(i.start,i.end,i.callback,i.point);
    }
  }

void Npc::loadAiState(Serialize& fin) {
  uint32_t size=0;

  fin.read(waitTime,faiWaitTime,reinterpret_cast<uint8_t&>(aiPolicy));
  fin.read(aiState.funcIni,aiState.funcLoop,aiState.funcEnd,aiState.sTime,aiState.eTime,aiState.started,aiState.loopNextTime);
  fin.read(aiPrevState);

  fin.read(size);
  aiActions.resize(size);
  for(auto& i:aiActions){
    fin.read(reinterpret_cast<uint32_t&>(i.act));
    fin.read(i.point,i.func,i.i0,i.s0);
    }

  fin.read(size);
  routines.resize(size);
  for(auto& i:routines){
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
  physic.setPosition(x,y,z);
  return true;
  }

bool Npc::setPosition(const std::array<float,3> &pos) {
  return setPosition(pos[0],pos[1],pos[2]);
  }

bool Npc::setViewPosition(const std::array<float,3> &pos) {
  x = pos[0];
  y = pos[1];
  z = pos[2];
  durtyTranform |= TR_Pos;
  return true;
  }

int Npc::aiOutputOrderId() const {
  int v = std::numeric_limits<int>::max();
  for(auto& i:aiActions)
    if(i.i0<v && (i.act==AI_Output || i.act==AI_OutputSvm || i.act==AI_OutputSvmOverlay))
      v = i.i0;
  return v;
  }

bool Npc::performOutput(const Npc::AiAction &act) {
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
  if(act.act==AI_OutputSvm        && outputPipe->outputSvm(*this,act.s0,hnpc.voice))
    return true;
  if(act.act==AI_OutputSvmOverlay && outputPipe->outputOv(*this,act.s0,hnpc.voice))
    return true;
  return false;
  }

void Npc::setDirection(float x, float /*y*/, float z) {
  float a=angleDir(x,z);
  setDirection(a);
  }

void Npc::setDirection(const std::array<float,3> &pos) {
  setDirection(pos[0],pos[1],pos[2]);
  }

void Npc::setDirection(float rotation) {
  if(std::fabs(angle-rotation)<0.001f)
    return;
  angle = rotation;
  durtyTranform |= TR_Rot;
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
  if(routines.size()==0 || (isDead() && !isMainNpc)) {
    // remove bodyes of dead, non-main npc
    return isMainNpc || !isDead();
    }

  attachToPoint(nullptr);
  setInteraction(nullptr);
  clearAiQueue();
  clearState(true);

  auto& rot = currentRoutine();
  auto  at  = rot.point;

  if(at==nullptr) {
    auto time = owner.time();
    time = gtime(int32_t(time.hour()),int32_t(time.minute()));
    int64_t delta=std::numeric_limits<int64_t>::max();

    // closest point
    for(auto& i:routines){
      int64_t ndelta=delta;
      if(i.end<i.start)
        ndelta = i.start.toInt()-time.toInt();
      else
        ndelta = i.end.toInt()-time.toInt();

      if(i.point && ndelta<delta)
        at = i.point;
      }
    // any point
    if(at==nullptr){
      for(auto& i:routines){
        if(i.point)
          at=i.point;
        }
      }
    if(at==nullptr)
      return false;
    }

  if(at->isLocked()){
    auto p = owner.findNextPoint(*at);
    if(p!=nullptr)
      at=p;
    }
  setPosition (at->x, at->y, at->z);
  setDirection(at->dirX,at->dirY,at->dirZ);
  attachToPoint(at);
  return true;
  }

void Npc::stopDlgAnim() {
  visual.stopDlgAnim();
  }

void Npc::clearSpeed() {
  mvAlgo.clearSpeed();
  }

void Npc::setProcessPolicy(ProcessPolicy t) {
  aiPolicy=t;
  }

void Npc::setWalkMode(WalkBit m) {
  wlkMode = m;
  }

bool Npc::isPlayer() const {
  return aiPolicy==Npc::ProcessPolicy::Player;
  }

bool Npc::startClimb(JumpCode code) {
  Anim ani = Anim::Idle;
  switch(code){
    case Npc::JumpCode::JM_Up:
      ani = Npc::Anim::JumpUp;
      break;
    case Npc::JumpCode::JM_UpLow:
      ani = Npc::Anim::JumpUpLow;
      break;
    case Npc::JumpCode::JM_UpMid:
      ani = Npc::Anim::JumpUpMid;
      break;
    case Npc::JumpCode::JM_OK:
      ani = Npc::Anim::Jump;
      break;
    }
  if(mvAlgo.startClimb(code)){
    setAnim(ani);
    return true;
    }
  return false;
  }

bool Npc::checkHealth(bool onChange,bool allowUnconscious) {
  if(onChange && lastHit!=nullptr) {
    uint32_t g = guild();
    if(g==GIL_SKELETON || g==GIL_SKELETON_MAGE || g==GIL_SUMMONED_SKELETON) {
      lastHitType='A';
      } else {
      float da = rotationRad()-lastHit->rotationRad();
      if(std::cos(da)>=0)
        lastHitType='A'; else
        lastHitType='B';
      }
    }

  if(isDead()) {
    physic.setEnable(false);
    return false;
    }
  if(isUnconscious() && allowUnconscious) {
    closeWeapon(true);
    return false;
    }

  const int minHp = isMonster() ? 0 : 1;
  if(hnpc.attribute[ATR_HITPOINTS]<=minHp) {
    if(hnpc.attribute[ATR_HITPOINTSMAX]<=1){
      size_t fdead=owner.getSymbolIndex("ZS_Dead");
      startState(fdead,"");
      return false;
      }

    if(currentOther==nullptr ||
       !allowUnconscious ||
       owner.script().personAttitude(*this,*currentOther)==ATT_HOSTILE ||
       guild()>GIL_SEPERATOR_HUM){
      if(hnpc.attribute[ATR_HITPOINTS]<=0)
        onNoHealth(true);
      return false;
      }

    if(onChange) {
      onNoHealth(false);
      return false;
      }
    }
  physic.setEnable(true);
  return true;
  }

void Npc::onNoHealth(bool death) {
  // TODO: drop the weapon instead
  visual.setToFightMode(WeaponState::NoWeapon);
  updateWeaponSkeleton();

  const char* svm   = death ? "SVM_%d_DEAD" : "SVM_%d_AARGH";
  const char* state = death ? "ZS_Dead"     : "ZS_Unconscious";

  if(!death)
    hnpc.attribute[ATR_HITPOINTS]=1;

  size_t fdead=owner.getSymbolIndex(state);
  //if(animation.current!=Anim::UnconsciousA && animation.current!=Anim::UnconsciousB)
  //  animation.resetAni();
  startState(fdead,"");
  if(hnpc.voice>0 && currentOther!=nullptr){
    // in case of battle currentOther!=nullptr,
    // if else death is scripted
    char name[32]={};
    std::snprintf(name,sizeof(name),svm,int(hnpc.voice));
    emitSoundEffect(name,25,true);
    }

  setInteraction(nullptr);
  if(death)
    setAnim(lastHitType=='A' ? Anim::DeadA        : Anim::DeadB); else
    setAnim(lastHitType=='A' ? Anim::UnconsciousA : Anim::UnconsciousB);
  }

World& Npc::world() {
  return owner;
  }

std::array<float,3> Npc::position() const {
  return {{x,y,z}};
  }

std::array<float,3> Npc::cameraBone() const {
  auto bone=visual.pose().cameraBone();
  std::array<float,3> r={{}};
  bone.project(r[0],r[1],r[2]);
  visual.pos.project (r[0],r[1],r[2]);
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

float Npc::translateY() const {
  return visual.pose().translateY();
  }

Npc *Npc::lookAtTarget() const {
  return currentLookAt;
  }

float Npc::qDistTo(float x1, float y1, float z1) const {
  float dx=x-x1;
  float dy=y-y1;
  float dz=z-z1;
  return dx*dx+dy*dy+dz*dz;
  }

float Npc::qDistTo(const WayPoint *f) const {
  if(f==nullptr)
    return 0.f;
  return qDistTo(f->x,f->y,f->z);
  }

float Npc::qDistTo(const Npc &p) const {
  return qDistTo(p.x,p.y,p.z);
  }

float Npc::qDistTo(const Interactive &p) const {
  return qDistTo(p.position()[0],p.position()[1],p.position()[2]);
  }

void Npc::updateAnimation() {
  visual.updateAnimation(*this);
  }

void Npc::updateTransform() {
  if(durtyTranform){
    updatePos();
    durtyTranform=0;
    }
  }

const char *Npc::displayName() const {
  return hnpc.name[0].c_str();
  }

std::array<float,3> Npc::displayPosition() const {
  float h = 0;
  if(visual.skeleton)
    h = visual.skeleton->colisionHeight()*1.5f;
  return {{x,y+h,z}};
  }

void Npc::setVisual(const char* visual) {
  auto skelet = Resources::loadSkeleton(visual);
  setVisual(skelet);
  setPhysic(owner.getPhysic(visual));
  }

void Npc::addOverlay(const char *sk, uint64_t time) {
  auto skelet = Resources::loadSkeleton(sk);
  addOverlay(skelet,time);
  }

void Npc::addOverlay(const Skeleton* sk,uint64_t time) {
  if(time!=0)
    time+=owner.tickCount();
  visual.addOverlay(sk,time);
  }

void Npc::delOverlay(const char *sk) {
  visual.delOverlay(sk);
  }

void Npc::delOverlay(const Skeleton *sk) {
  visual.delOverlay(sk);
  }

ZMath::float3 Npc::animMoveSpeed(uint64_t dt) const {
  return visual.pose().animMoveSpeed(owner.tickCount(),dt);
  }

void Npc::setVisual(const Skeleton* v) {
  visual.setVisual(v);
  }

static std::string addExt(const std::string& s,const char* ext){
  if(s.size()>0 && s.back()=='.')
    return s+&ext[1];
  return s+ext;
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

  auto  vhead = head.empty() ? MeshObjects::Mesh() : w.getView(addExt(head,".MMB"),vHead,vTeeth,bdColor);
  auto  vbody = body.empty() ? MeshObjects::Mesh() : w.getView(addExt(body,".MDM"),vColor,0,bdColor);
  visual.setVisualBody(std::move(vhead),std::move(vbody));
  updateArmour();

  durtyTranform|=TR_Pos; // update obj matrix
  }

void Npc::updateArmour() {
  auto  ar = invent.currentArmour();
  auto& w  = owner;

  if(ar==nullptr) {
    auto  vbody = body.empty() ? MeshObjects::Mesh() : w.getView(addExt(body,".MDM"),vColor,0,bdColor);
    visual.setArmour(std::move(vbody));
    } else {
    auto& itData = *ar->handle();
    auto  flag   = Inventory::Flags(itData.mainflag);
    if(flag & Inventory::ITM_CAT_ARMOR){
      auto asc = itData.visual_change;
      if(asc.rfind(".asc")==asc.size()-4)
        std::memcpy(&asc[asc.size()-3],"MDM",3);
      auto vbody  = asc.empty() ? MeshObjects::Mesh() : w.getView(asc,vColor,0,bdColor);
      visual.setArmour(std::move(vbody));
      }
    }
  }

void Npc::setSword(MeshObjects::Mesh &&s) {
  visual.setSword(std::move(s));
  updateWeaponSkeleton();
  }

void Npc::setRangeWeapon(MeshObjects::Mesh &&b) {
  visual.setRangeWeapon(std::move(b));
  updateWeaponSkeleton();
  }

void Npc::setMagicWeapon(PfxObjects::Emitter &&s) {
  visual.setMagicWeapon(std::move(s));
  updateWeaponSkeleton();
  }

void Npc::updateWeaponSkeleton() {
  visual.updateWeaponSkeleton(invent.currentMeleWeapon(),invent.currentRangeWeapon());
  }

void Npc::setPhysic(DynamicWorld::Item &&item) {
  physic = std::move(item);
  physic.setUserPointer(this);
  physic.setPosition(x,y,z);
  }

void Npc::setFatness(float) {
  // TODO
  }

void Npc::setScale(float x, float y, float z) {
  sz[0]=x;
  sz[1]=y;
  sz[2]=z;
  durtyTranform |= TR_Scale;
  }

bool Npc::playAnimByName(const std::string &name,BodyState bs) {
  return visual.setAnim(*this,name.c_str(),bs);
  }

bool Npc::setAnim(Npc::Anim a) {
  auto st  = invent.weaponState();
  auto wlk = walkMode();
  if(mvAlgo.isSwim())
    wlk = WalkBit::WM_Swim;
  else if(mvAlgo.isInWater())
    wlk = WalkBit::WM_Water;
  return visual.setAnim(*this,a,st,wlk);
  }

void Npc::setAnimRotate(int rot) {
  visual.setRotation(*this,rot);
  }

bool Npc::setAnimItem(const char *scheme) {
  if(scheme==nullptr || scheme[0]==0)
    return true;
  return visual.setAnimItem(*this,scheme);
  }

void Npc::stopAnim(const std::string &ani) {
  visual.stopAnim(*this,ani.c_str());
  }

bool Npc::isFinishingMove() const {
  return visual.pose().isInAnim("T_1HSFINISH") || visual.pose().isInAnim("T_2HSFINISH");
  }

bool Npc::isRunTo() const {
  return visual.isRunTo(*this);
  }

bool Npc::isStanding() const {
  return visual.isStanding();
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

void Npc::setTalentSkill(Npc::Talent t, int32_t lvl) {
  if(t<TALENT_MAX_G2) {
    talentsSk[t] = lvl;
    if(t==TALENT_1H){
      if(lvl==0){
        delOverlay("HUMANS_1HST1.MDH");
        delOverlay("HUMANS_1HST2.MDH");
        }
      else if(lvl==1){
        addOverlay("HUMANS_1HST1.MDH",0);
        delOverlay("HUMANS_1HST2.MDH");
        }
      else if(lvl==2){
        delOverlay("HUMANS_1HST1.MDH");
        addOverlay("HUMANS_1HST2.MDH",0);
        }
      }
    else if(t==TALENT_2H){
      if(lvl==0){
        delOverlay("HUMANS_2HST1.MDH");
        delOverlay("HUMANS_2HST2.MDH");
        }
      else if(lvl==1){
        addOverlay("HUMANS_2HST1.MDH",0);
        delOverlay("HUMANS_2HST2.MDH");
        }
      else if(lvl==2){
        delOverlay("HUMANS_2HST1.MDH");
        addOverlay("HUMANS_2HST2.MDH",0);
        }
      }
    else if(t==TALENT_BOW){
      if(lvl==0){
        delOverlay("HUMANS_BOWT1.MDH");
        delOverlay("HUMANS_BOWT2.MDH");
        }
      else if(lvl==1){
        addOverlay("HUMANS_BOWT1.MDH",0);
        delOverlay("HUMANS_BOWT2.MDH");
        }
      else if(lvl==2){
        delOverlay("HUMANS_BOWT1.MDH");
        addOverlay("HUMANS_BOWT2.MDH",0);
        }
      }
    else if(t==TALENT_CROSSBOW){
      if(lvl==0){
        delOverlay("HUMANS_CBOWT1.MDH");
        delOverlay("HUMANS_CBOWT2.MDH");
        }
      else if(lvl==1){
        addOverlay("HUMANS_CBOWT1.MDH",0);
        delOverlay("HUMANS_CBOWT2.MDH");
        }
      else if(lvl==2){
        delOverlay("HUMANS_CBOWT1.MDH");
        addOverlay("HUMANS_CBOWT2.MDH",0);
        }
      }
    else if(t==TALENT_ACROBAT){
      if(lvl==0)
        delOverlay("HUMANS_ACROBATIC.MDH"); else
        addOverlay("HUMANS_ACROBATIC.MDH",0);
      }
    }
  }

int32_t Npc::talentSkill(Npc::Talent t) const {
  if(t<TALENT_MAX_G2)
    return talentsSk[t];
  return 0;
  }

void Npc::setTalentValue(Npc::Talent t, int32_t lvl) {
  if(t<TALENT_MAX_G2)
    talentsVl[t] = lvl;
  }

int32_t Npc::talentValue(Npc::Talent t) const {
  if(t<TALENT_MAX_G2)
    return talentsVl[t];
  return 0;
  }

int32_t Npc::hitChanse(Npc::Talent t) const {
  if(t<Daedalus::GEngineClasses::MAX_HITCHANCE)
    return hnpc.hitChance[t];
  return 0;
  }

bool Npc::isRefuseTalk() const {
  return refuseTalkMilis>=owner.tickCount();
  }

int32_t Npc::mageCycle() const {
  return talentSkill(TALENT_RUNES);
  }

void Npc::setRefuseTalk(uint64_t milis) {
  refuseTalkMilis = owner.tickCount()+milis;
  }

int32_t Npc::attribute(Npc::Attribute a) const {
  if(a<ATR_MAX)
    return hnpc.attribute[a];
  return 0;
  }

void Npc::changeAttribute(Npc::Attribute a, int32_t val, bool allowUnconscious) {
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

int32_t Npc::protection(Npc::Protection p) const {
  if(p<PROT_MAX)
    return hnpc.protection[p];
  return 0;
  }

void Npc::changeProtection(Npc::Protection p, int32_t val) {
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

void Npc::setAttitude(Attitude att) {
  permAttitude = att;
  }

bool Npc::isFriend() const {
  return hnpc.npcType==Daedalus::GEngineClasses::ENPCType::NPCTYPE_FRIEND;
  }

void Npc::setTempAttitude(Attitude att) {
  tmpAttitude = att;
  }

bool Npc::implLookAt(uint64_t dt) {
  if(currentLookAt!=nullptr && interactive()==nullptr) {
    if(implLookAt(*currentLookAt,dt))
      return true;
    currentLookAt=nullptr;
    return false;
    }
  return false;
  }

bool Npc::implLookAt(const Npc &oth, uint64_t dt) {
  auto dx = oth.x-x;
  auto dz = oth.z-z;
  if(implLookAt(dx,dz,0,dt))
    return true;
  currentLookAt=nullptr;
  return false;
  }

bool Npc::implLookAt(float dx, float dz, int noAniAngle, uint64_t dt) {
  auto  gl   = std::min<uint32_t>(guild(),GIL_MAX);
  float step = owner.script().guildVal().turn_speed[gl]*(dt/1000.f)*60.f/100.f;

  float a    = angleDir(dx,dz);
  float da   = a-angle;

  if(std::abs(int(da)%180)<=step && std::cos(double(da)*M_PI/180.0)>0){
    setDirection(a);
    visual.setRotation(*this,0);
    return false;
    }

  if(std::abs(int(da)%180)>noAniAngle)
    visual.setRotation(*this,-int(da));

  const auto sgn = std::sin(double(da)*M_PI/180.0);
  if(sgn<0)
    setDirection(angle-step); else
    setDirection(angle+step);
  return true;
  }

bool Npc::implGoTo(uint64_t dt) {
  if(!currentGoTo && !currentGoToNpc)
    return false;

  if(currentGoTo){
    float dx = currentGoTo->x-x;
    float dz = currentGoTo->z-z;

    int needToRot = (walkMode()==WalkBit::WM_Run && !isStanding()) ? 45 : 0;
    if(implLookAt(dx,dz,needToRot,dt)){
      mvAlgo.tick(dt);
      return true;
      }

    if(!mvAlgo.aiGoTo(currentGoTo)) {
      currentGoTo = wayPath.pop();
      if(currentGoTo!=nullptr) {
        attachToPoint(currentGoTo);
        } else
      if(!visual.isStanding()) {
        mvAlgo.aiGoTo(nullptr);
        setAnim(AnimationSolver::Idle);
        }
      }

    if(mvAlgo.hasGoTo()) {
      setAnim(AnimationSolver::Move);
      mvAlgo.tick(dt);
      return true;
      }
    return false;
    } else {
    float dx = currentGoToNpc->x-x;
    float dz = currentGoToNpc->z-z;

    if(implLookAt(dx,dz,45,dt))
      return true;
    if(!mvAlgo.aiGoTo(currentGoToNpc,400)) {
      if(isStanding())
        setAnim(AnimationSolver::Idle);
      }

    if(mvAlgo.hasGoTo()) {
      setAnim(AnimationSolver::Move);
      mvAlgo.tick(dt);
      return true;
      }
    return false;
    }
  }

bool Npc::implAtack(uint64_t dt) {
  if(currentTarget==nullptr || isPlayer() || isTalk())
    return false;

  if(currentTarget->isDown()){
    // NOTE: don't clear internal target, to make scripts happy
    //currentTarget=nullptr;
    return false;
    }

  if(weaponState()==WeaponState::NoWeapon)
    return false;

  if(!visual.pose().isAtackFinished(owner.tickCount()))
    return true;

  if(faiWaitTime>=owner.tickCount()) {
    implLookAt(*currentTarget,dt);
    return true;
    }

  if(!fghAlgo.hasInstructions())
    return false;

  if(implLookAt(*currentTarget,dt)) {
    setAnim(AnimationSolver::Idle);
    return true;
    }

  FightAlgo::Action act = fghAlgo.nextFromQueue(*this,*currentTarget,owner.script());

  if(act==FightAlgo::MV_BLOCK) {
    if(setAnim(Anim::AtackBlock))
      fghAlgo.consumeAction();
    return true;
    }

  if(act==FightAlgo::MV_ATACK || act==FightAlgo::MV_ATACKL || act==FightAlgo::MV_ATACKR) {
    static const Anim ani[3]={Anim::Atack,Anim::AtackL,Anim::AtackR};
    if(act!=FightAlgo::MV_ATACK && !fghAlgo.isInAtackRange(*this,*currentTarget,owner.script())){
      fghAlgo.consumeAction();
      return true;
      }

    auto ws = weaponState();
    if(ws==WeaponState::Mage){
      if(castSpell())
        fghAlgo.consumeAction();
      }
    else if(ws==WeaponState::Bow || ws==WeaponState::CBow){
      if(shootBow())
        fghAlgo.consumeAction();
      }
    else if(ws==WeaponState::Fist){
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
    if(!implLookAt(*currentTarget,dt))
      fghAlgo.consumeAction();
    return true;
    }

  if(act==FightAlgo::MV_STRAFEL) {
    if(setAnim(Npc::Anim::MoveL)){
      implFaiWait(visual.pose().animationTotalTime());
      fghAlgo.consumeAction();
      }
    return true;
    }

  if(act==FightAlgo::MV_STRAFER) {
    if(setAnim(Npc::Anim::MoveR)){
      implFaiWait(visual.pose().animationTotalTime());
      fghAlgo.consumeAction();
      }
    return true;
    }

  if(act==FightAlgo::MV_JUMPBACK) {
    if(setAnim(Npc::Anim::MoveBack))
      fghAlgo.consumeAction();
    return true;
    }

  if(act==FightAlgo::MV_MOVEA || act==FightAlgo::MV_MOVEG) {
    const float d = act==FightAlgo::MV_MOVEG ?
          fghAlgo.prefferedGDistance(*this,*currentTarget,owner.script()) :
          fghAlgo.prefferedAtackDistance(*this,*currentTarget,owner.script());
    if(!mvAlgo.aiGoToTarget(d)) {
      fghAlgo.consumeAction();
      aiState.loopNextTime=owner.tickCount(); //force ZS_MM_Attack_Loop call
      if(act==FightAlgo::MV_MOVEA)
        setAnim(AnimationSolver::Idle);
      implAiTick(dt);
      return true;
      }

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

bool Npc::implAiTick(uint64_t dt) {
  if(!aiState.started && aiState.funcIni!=0) {
    tickRoutine();
    }
  else if(aiActions.size()==0) {
    tickRoutine();
    if(aiActions.size()>0)
      nextAiAction(dt);
    return false;
    }
  nextAiAction(dt);
  return true;
  }

void Npc::implAiWait(uint64_t dt) {
  waitTime = owner.tickCount()+dt;
  }

void Npc::implFaiWait(uint64_t dt) {
  faiWaitTime          = owner.tickCount()+dt;
  aiState.loopNextTime = faiWaitTime;
  }

void Npc::commitDamage() {
  if(currentTarget==nullptr)
    return;
  if(!fghAlgo.isInAtackRange(*this,*currentTarget,owner.script()))
    return;
  if(!fghAlgo.isInFocusAngle(*this,*currentTarget))
    return;
  currentTarget->takeDamage(*this);
  }

void Npc::takeDamage(Npc &other) {
  takeDamage(other,nullptr);
  }

void Npc::takeDamage(Npc &other, const Bullet *b) {
  if(isDown())
    return;

  setOther(&other);
  owner.sendPassivePerc(*this,other,*this,PERC_ASSESSFIGHTSOUND);

  const bool isBlock = visual.pose().isDefence(owner.tickCount());
  if(!isBlock || b!=nullptr) {
    perceptionProcess(other,this,0,PERC_ASSESSDAMAGE);
    owner.sendPassivePerc(*this,other,*this,PERC_ASSESSOTHERSDAMAGE);

    lastHit = &other;
    fghAlgo.onTakeHit();
    implFaiWait(0);
    if(!isPlayer())
      setOther(lastHit);
    auto hitResult = other.damageValue(*this,b);
    int dmg = isImmortal() ? 0 : std::get<0>(hitResult);
    if(isPlayer() && owner.script().isRamboMode())
      dmg = std::min(1,dmg);

    if(!isDown() && std::get<1>(hitResult)>0)
      owner.emitWeaponsSound(other,*this);

    if(dmg<=0)
      return;
    if(attribute(ATR_HITPOINTS)>0){
      if(lastHitType=='A')
        setAnim(Anim::StumbleA); else
        setAnim(Anim::StumbleB);
      }
    changeAttribute(ATR_HITPOINTS,-dmg,b==nullptr);

    if(isUnconscious()){
      owner.sendPassivePerc(*this,other,*this,PERC_ASSESSDEFEAT);
      }
    else if(isDead()) {
      owner.sendPassivePerc(*this,other,*this,PERC_ASSESSMURDER);
      }

    if(other.damageTypeMask() & (1<<Daedalus::GEngineClasses::DAM_INDEX_FLY))
      mvAlgo.accessDamFly(x-other.x,z-other.z); // throw enemy
    } else {
    if(invent.activeWeapon()!=nullptr)
      owner.emitBlockSound(other,*this);
    }
  }

std::tuple<int,bool> Npc::damageValue(Npc &other, const Bullet* b) const {
  int value = 0;

  if(b!=nullptr) {
    // Bow/CBow
    const float maxRange = 3500; // from Focus_Ranged
    if(b->pathLength()>maxRange*b->hitChance() && b->hitChance()<1.f)
      return std::make_tuple(0,false);

    auto dmg = b->damage();
    for(int i=0;i<Daedalus::GEngineClasses::DAM_INDEX_MAX;++i){
      int vd = std::max(dmg[size_t(i)] - other.hnpc.protection[i],0);
      if(other.hnpc.protection[i]>=0) // Filter immune
        value += vd;
      }
    } else {
    // Swords/Fists
    const int dtype = damageTypeMask();
    uint8_t   hitCh = TALENT_UNKNOWN;
    if(auto w = invent.activeWeapon()){
      if(w->is2H())
        hitCh = TALENT_2H; else
        hitCh = TALENT_1H;
      }

    int s          = attribute(Attribute::ATR_STRENGTH);
    int critChance = int(owner.script().rand(100));

    for(int i=0;i<Daedalus::GEngineClasses::DAM_INDEX_MAX;++i){
      if((dtype & (1<<i))==0)
        continue;
      int vd = std::max(s + hnpc.damage[i] - other.hnpc.protection[i],0);
      if(hnpc.hitChance[hitCh]<critChance)
        vd = (vd-1)/10;
      if(other.hnpc.protection[i]>=0) // Filter immune
        value += vd;
      }
    }

  int damage = std::max(value,3);
  return std::make_tuple(damage,true);
  }

std::array<int32_t,Daedalus::GEngineClasses::DAM_INDEX_MAX> Npc::rangeDamageValue() const {
  const int dtype = damageTypeMask();
  int d = attribute(Attribute::ATR_DEXTERITY);
  std::array<int32_t,Daedalus::GEngineClasses::DAM_INDEX_MAX> ret={};
  for(int i=0;i<Daedalus::GEngineClasses::DAM_INDEX_MAX;++i){
    if((dtype & (1<<i))==0)
      continue;
    ret[size_t(i)] = d + hnpc.damage[i];
    }
  return ret;
  }

int32_t Npc::damageTypeMask() const {
  if(auto w = invent.activeWeapon())
    return w->handle()->damageType;
  return hnpc.damagetype;
  }

Npc *Npc::updateNearestEnemy() {
  if(aiPolicy!=ProcessPolicy::AiNormal)
    return nullptr;

  Npc*  ret  = nullptr;
  float dist = std::numeric_limits<float>::max();
  if(nearestEnemy!=nullptr && (!nearestEnemy->isDown() && canSenseNpc(*nearestEnemy,true)!=SensesBit::SENSE_NONE)) {
    ret  = nearestEnemy;
    dist = qDistTo(*ret);
    }

  owner.detectNpcNear([this,&ret,&dist](Npc& n){
    if(!isEnemy(n))
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

void Npc::tick(uint64_t dt) {
  owner.tickSlot(soundSlot);

  if(!visual.pose().hasAnim())
    setAnim(AnimationSolver::Idle);

  Animation::EvCount ev;
  visual.pose().processEvents(fghLastEventTime,owner.tickCount(),ev);

  if(ev.def_opt_frame>0)
    commitDamage();
  if(visual.setFightMode(ev.weaponCh))
    updateWeaponSkeleton();

  if(!checkHealth(false,true)){
    mvAlgo.aiGoTo(nullptr);
    mvAlgo.tick(dt);
    setOther(lastHit);
    aiActions.clear();
    tickRoutine(); // tick for ZS_Death
    return;
    }

  if(waitTime>=owner.tickCount()) {
    mvAlgo.tick(dt);
    return;
    }

  if(implAtack(dt)) {
    mvAlgo.tick(dt,true);
    return;
    }

  if(implLookAt(dt))
    return;

  if(implGoTo(dt))
    return;

  mvAlgo.tick(dt);

  if(interactive()!=nullptr)
    setAnim(AnimationSolver::Interact);
  /*
  else
  if(currentGoTo==nullptr && currentGoToNpc==nullptr &&
     aiPolicy!=ProcessPolicy::Player &&
     !isInAnim(Anim::Pray) && !isInAnim(Anim::PrayRand) && !isInAnim(Anim::Talk) && !isInAnim(Anim::Sleep)) {
    if(weaponState()==WeaponState::NoWeapon)
      setAnim(animation.lastIdle); else
    if(animation.current>Anim::IdleLoopLast)
      setAnim(Anim::Idle); else
      setAnim(animation.current);
    }*/
  implAiTick(dt);
  }

void Npc::nextAiAction(uint64_t dt) {
  if(aiActions.size()==0)
    return;
  auto act = std::move(aiActions.front());
  aiActions.pop_front();

  switch(act.act) {
    case AI_None: break;
    case AI_LookAt:{
      currentLookAt=act.target;
      break;
      }
    case AI_TurnToNpc: {
      if(act.target!=nullptr && implLookAt(*act.target,dt)){
        aiActions.push_front(std::move(act));
        }
      break;
      }
    case AI_GoToNpc:
      setInteraction(nullptr);
      currentGoTo     = nullptr;
      currentGoToNpc  = act.target;
      currentFp       = nullptr;
      currentFpLock   = FpLock();
      currentGoToFlag = GoToHint::GT_Default;
      wayPath.clear();
      break;
    case AI_GoToNextFp: {
      setInteraction(nullptr);
      auto fp = owner.findNextFreePoint(*this,act.s0.c_str());
      if(fp!=nullptr) {
        currentGoToNpc  = nullptr;
        currentFp       = nullptr;
        currentFpLock   = FpLock(*fp);
        currentGoTo     = fp;
        currentGoToFlag = GoToHint::GT_NextFp;
        wayPath.clear();
        }
      break;
      }
    case AI_GoToPoint: {
      setInteraction(nullptr);
      if(wayPath.last()!=act.point)
        wayPath = owner.wayTo(*this,*act.point);
      currentGoTo     = wayPath.pop();
      currentGoToNpc  = nullptr;
      currentGoToFlag = GoToHint::GT_Default;
      attachToPoint(currentGoTo);
      break;
      }
    case AI_StopLookAt:
      currentLookAt=nullptr;
      break;
    case AI_RemoveWeapon:
      if(closeWeapon(false)) {
        setAnim(Anim::Idle);
        } else {
        aiActions.push_front(std::move(act));
        }
      break;
    case AI_StartState:
      if(startState(act.func,act.s0,aiState.eTime,act.i0==0))
        setOther(act.target);
      break;
    case AI_PlayAnim:{
      if(!playAnimByName(act.s0,BS_NONE))
        aiActions.push_front(std::move(act));
      break;
      }
    case AI_PlayAnimBs:{
      if(!playAnimByName(act.s0,BodyState(act.i0)))
        aiActions.push_front(std::move(act));
      }
      break;
    case AI_PlayAnimById:{
      auto tag = Anim(act.i0);
      if(!setAnim(tag)) {
        aiActions.push_front(std::move(act));
        } else {
        implAiWait(visual.pose().animationTotalTime());
        }
      break;
      }
    case AI_Wait:
      implAiWait(uint64_t(act.i0));
      break;
    case AI_StandUp:
      setInteraction(nullptr);
      setAnim(Anim::Idle);
      break;
    case AI_StandUpQuick:
      setInteraction(nullptr);
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
      if(act.i0<0){
        setInteraction(nullptr);
        break;
        }
      if(owner.script().isTalk(*this)) {
        aiActions.push_front(std::move(act));
        break;
        }
      auto inter = owner.aviableMob(*this,act.s0);
      if(inter==nullptr) {
        aiActions.push_front(std::move(act));
        break;
        }

      if(qDistTo(*inter)>MAX_AI_USE_DISTANCE*MAX_AI_USE_DISTANCE) { // too far
        //break; //TODO
        }
      if(!setInteraction(inter))
        aiActions.push_front(std::move(act));
      break;
      }
    case AI_UseItem:
      if(act.i0!=0)
        useItem(uint32_t(act.i0));
      break;
    case AI_Teleport: {
      setPosition(act.point->x,act.point->y,act.point->z);
      }
      break;
    case AI_DrawWeaponMele:
      fghAlgo.onClearTarget();
      if(!drawWeaponMele())
        aiActions.push_front(std::move(act));
      break;
    case AI_DrawWeaponRange:
      fghAlgo.onClearTarget();
      if(!drawWeaponBow())
        aiActions.push_front(std::move(act));
      break;
    case AI_DrawSpell: {
      const int32_t spell = act.i0;
      fghAlgo.onClearTarget();
      // invent.switchActiveSpell(spell,*this);
      if(!drawSpell(spell))
        aiActions.push_front(std::move(act));
      break;
      }
    case AI_Atack:
      //atackMode=true;
      if(currentTarget!=nullptr){
        if(!fghAlgo.fetchInstructions(*this,*currentTarget,owner.script()))
          aiActions.push_front(std::move(act));
        }
      break;
    case AI_Flee:
      //atackMode=false;
      break;
    case AI_Dodge:
      setAnim(Anim::MoveBack);
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
        if(act.act!=AI_OutputSvmOverlay)
          visual.setAnimDialog(*this);
        } else {
        aiActions.push_front(std::move(act));
        }
      break;
      }
    case AI_ProcessInfo:
      if(act.target==nullptr)
        break;
      if(auto p = owner.script().openDlgOuput(*this,*act.target)) {
        outputPipe = p;
        setOther(act.target);
        act.target->setOther(this);
        act.target->outputPipe = p;
        } else {
        aiActions.push_front(std::move(act));
        }
      break;
    case AI_StopProcessInfo:
      if(outputPipe->close()) {
        outputPipe = owner.script().openAiOuput();
        if(currentOther!=nullptr)
          currentOther->outputPipe = owner.script().openAiOuput();
        } else {
        aiActions.push_front(std::move(act));
        }
      break;
    case AI_ContinueRoutine:{
      auto& r = currentRoutine();
      auto  t = endTime(r);
      startState(r.callback,r.point ? r.point->name : "",t,false);
      break;
      }
    case AI_AlignToWp:
    case AI_AlignToFp:{
      if(auto fp = currentFp){
        if(fp->dirX!=0.f || fp->dirZ!=0.f){
          if(implLookAt(fp->dirX,fp->dirZ,0,dt))
            aiActions.push_front(std::move(act));
          }
        }
      break;
      }
    case AI_SetNpcsToState:{
      const int32_t r = act.i0*act.i0;
      owner.detectNpc(position(),hnpc.senses_range,[&act,this,r](Npc& other){
        if(&other!=this && qDistTo(other)<r)
          other.aiStartState(uint32_t(act.func),1,other.currentOther,other.hnpc.wp);
        });
      break;
      }
    case AI_SetWalkMode:{
      setWalkMode(WalkBit(act.i0));
      break;
      }
    case AI_FinishingMove:{
      if(!act.target->isUnconscious())
        break;

      if(!fghAlgo.isInAtackRange(*this,*act.target,owner.script())){
        aiActions.push_front(std::move(act));

        float dx = act.target->x-x;
        float dz = act.target->z-z;
        if(implLookAt(dx,dz,180,dt))
          break;
        setAnim(Anim::Move);
        if(mvAlgo.aiGoTo(currentGoToNpc,fghAlgo.prefferedAtackDistance(*this,*act.target,owner.script())))
          break;
        }
      else if(canFinish(*act.target)){
        setTarget(act.target);
        if(!finishingMove())
          aiActions.push_front(std::move(act));
        }
      break;
      }
    }
  }

bool Npc::startState(size_t id,const std::string &wp) {
  return startState(id,wp,gtime::endOfTime(),false);
  }

bool Npc::startState(size_t id,const std::string &wp, gtime endTime,bool noFinalize) {
  if(id==0)
    return false;
  if(aiState.funcIni==id)
    return false;

  clearState(noFinalize);
  if(!wp.empty())
    hnpc.wp = wp;

  if(aiState.funcIni!=0)
    aiPrevState = aiState.funcIni;

  auto& st = owner.script().getAiState(id);
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
  if(aiState.funcIni!=0 && aiState.started){
    if(owner.script().isTalk(*this)) {
      // avoid ZS_Talk bug
      owner.script().invokeState(this,currentOther,nullptr,aiState.funcLoop);
      }
    if(!noFinalize)
      owner.script().invokeState(this,currentOther,nullptr,aiState.funcEnd);  // cleanup
    }
  aiState = AiState();
  aiState.funcIni = 0;
  }

void Npc::tickRoutine() {
  if(aiState.funcIni==0 && !isPlayer()) {
    auto r = currentRoutine();
    if(r.callback!=0) {
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
      if(aiState.funcLoop!=size_t(-1)) {
        loop = owner.script().invokeState(this,currentOther,nullptr,aiState.funcLoop);
        } else {
        // ZS_DEATH   have no looping, in G1, G2 classic
        // ZS_GETMEAT have no looping, at all
        loop = aiState.funcIni==owner.script().getSymbolIndex("ZS_GETMEAT") ? 1 : 0;
        }

      if(aiState.eTime<=owner.time()) {
        if(!isTalk()) {
          loop=1; // have to hack ZS_Talk bugs
          }
        }
      if(loop!=0) {
        owner.script().invokeState(this,currentOther,nullptr,aiState.funcEnd);
        currentOther = nullptr;
        aiPrevState  = aiState.funcIni;
        aiState      = AiState();
        }
      }
    } else {
    aiState.started=true;
    owner.script().invokeState(this,currentOther,nullptr,aiState.funcIni);
    }
  }

void Npc::setTarget(Npc *t) {
  currentTarget=t;
  mvAlgo.aiGoTo(nullptr);
  }

Npc *Npc::target() {
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

bool Npc::haveOutput() const {
  return aiOutputOrderId()!=std::numeric_limits<int>::max();
  }

void Npc::setAiOutputBarrier(uint64_t dt) {
  aiOutputBarrier = owner.tickCount()+dt;
  }

bool Npc::doAttack(Anim anim) {
  auto weaponSt=invent.weaponState();
  auto weapon  =invent.activeWeapon();

  if(weaponSt==WeaponState::NoWeapon)
    return false;

  if(weaponSt==WeaponState::Mage && weapon!=nullptr)
    anim=Anim(owner.script().spellCastAnim(*this,*weapon));

  //if(animation.current==anim)
  //  return false;
  return setAnim(anim);
  }

void Npc::emitDlgSound(const char *sound) {
  uint64_t dt=0;
  owner.emitDlgSound(sound,x,y+180,z,WorldSound::talkRange,dt);
  setAiOutputBarrier(dt);
  }

void Npc::emitSoundEffect(const char *sound, float range, bool freeSlot) {
  owner.emitSoundEffect(sound,x,y+translateY(),z,range,freeSlot ? nullptr : &soundSlot);
  }

void Npc::emitSoundGround(const char* sound, float range, bool freeSlot) {
  char    buf[256]={};
  uint8_t mat = mvAlgo.groundMaterial();
  std::snprintf(buf,sizeof(buf),"%s_%s",sound,ZenLoad::zCMaterial::getMatGroupString(ZenLoad::MaterialGroup(mat)));
  owner.emitSoundEffect(buf,x,y,z,range,freeSlot ? nullptr : &soundSlot);
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
  if(mvAlgo.isSwim())
    s |= BS_SWIM;
  if(auto i = interactive())
    s = i->stateMask(s);
  if(s!=0)
    return BodyState(s);

  if(isStanding()) {
    s |= BS_STAND;
    } else {
    if(wlkMode==WalkBit::WM_Run)
      s = BS_RUN;
    else if(wlkMode==WalkBit::WM_Walk)
      s = BS_WALK;
    else if(wlkMode==WalkBit::WM_Sneak)
      s = BS_SNEAK;
    }

  return BodyState(s);
  }

void Npc::setToFightMode(const uint32_t item) {
  if(invent.itemCount(item)==0)
    addItem(item,1);

  auto weaponSt=invent.weaponState();

  invent.equip(item,*this,true);
  invent.switchActiveWeapon(*this,1);

  if(weaponSt==WeaponState::W1H || weaponSt==WeaponState::W2H)
    return;

  weaponSt=invent.weaponState();
  if(visual.setToFightMode(weaponSt))
    updateWeaponSkeleton();

  auto& weapon = *currentMeleWeapon();
  auto  st     = weapon.is2H() ? WeaponState::W2H : WeaponState::W1H;
  hnpc.weapon  = (st==WeaponState::W1H ? 3:4);
  }

void Npc::setToFistMode() {
  auto weaponSt=invent.weaponState();
  if(weaponSt==WeaponState::Fist)
    return;
  invent.switchActiveWeaponFist();
  if(visual.setToFightMode(WeaponState::Fist))
    updateWeaponSkeleton();
  hnpc.weapon  = 1;
  }

Item* Npc::addItem(const uint32_t item, uint32_t count) {
  return invent.addItem(item,count,owner);
  }

Item* Npc::addItem(std::unique_ptr<Item>&& i) {
  return invent.addItem(std::move(i));
  }

void Npc::addItem(uint32_t id, Interactive &chest) {
  Inventory::trasfer(invent,chest.inventory(),nullptr,id,1,owner);
  }

void Npc::addItem(uint32_t id, Npc &from) {
  Inventory::trasfer(invent,from.invent,&from,id,1,owner);
  }

void Npc::moveItem(uint32_t id, Interactive &to) {
  Inventory::trasfer(to.inventory(),invent,this,id,1,owner);
  }

void Npc::sellItem(uint32_t id, Npc &to) {
  int32_t price = invent.sellPriceOf(id);
  Inventory::trasfer(to.invent,invent,this,id,1,owner);
  invent.addItem(owner.script().goldId(),uint32_t(price),owner);
  }

void Npc::buyItem(uint32_t id, Npc &from) {
  if(id==owner.script().goldId())
    return;
  int32_t price = from.invent.priceOf(id);
  if(price>int32_t(invent.goldCount())) {
    owner.script().printCannotBuyError(*this);
    return;
    }
  Inventory::trasfer(invent,from.invent,nullptr,id,1,owner);
  invent.delItem(owner.script().goldId(),uint32_t(price),*this);
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

bool Npc::lookAt(float dx, float dz, bool anim, uint64_t dt) {
  return implLookAt(dx,dz,anim ? 0 : 180,dt);
  }

bool Npc::checkGoToNpcdistance(const Npc &other) {
  return fghAlgo.isInAtackRange(*this,other,owner.script());
  }

size_t Npc::hasItem(uint32_t id) const {
  return invent.itemCount(id);
  }

Item *Npc::getItem(uint32_t id) {
  return invent.getItem(id);
  }

void Npc::delItem(uint32_t item, uint32_t amount) {
  invent.delItem(item,amount,*this);
  }

void Npc::useItem(uint32_t item,bool force) {
  invent.use(item,*this,force);
  }

void Npc::unequipItem(uint32_t item) {
  invent.unequip(item,*this);
  }

bool Npc::closeWeapon(bool noAnim) {
  auto weaponSt=invent.weaponState();
  if(weaponSt==WeaponState::NoWeapon)
    return true;
  if(!noAnim && !visual.setAnim(*this,WeaponState::NoWeapon))
    return false;
  invent.switchActiveWeapon(*this,Item::NSLOT);
  hnpc.weapon = 0;
  updateWeaponSkeleton();

  if(weaponSt!=WeaponState::W1H && weaponSt!=WeaponState::W2H)
    return true;

  if(auto w = invent.currentMeleWeapon()){
    if(w->handle()->material==ItemMaterial::MAT_METAL)
      owner.emitSoundRaw("UNDRAWSOUND_ME.WAV",x,y+translateY(),z,500,nullptr); else
      owner.emitSoundRaw("UNDRAWSOUND_WO.WAV",x,y+translateY(),z,500,nullptr);
    }
  return true;
  }

bool Npc::drawWeaponFist() {
  auto weaponSt=invent.weaponState();
  if(weaponSt==WeaponState::Fist)
    return true;
  if(weaponSt!=WeaponState::NoWeapon) {
    closeWeapon(false);
    return false;
    }
  if(!visual.setAnim(*this,WeaponState::Fist))
    return false;
  invent.switchActiveWeaponFist();
  updateWeaponSkeleton();
  hnpc.weapon = 1;
  return true;
  }

bool Npc::drawWeaponMele() {
  if(isFaling() || mvAlgo.isSwim())
    return false;
  auto weaponSt=invent.weaponState();
  if(weaponSt==WeaponState::Fist || weaponSt==WeaponState::W1H || weaponSt==WeaponState::W2H)
    return true;
  if(invent.currentMeleWeapon()==nullptr)
    return drawWeaponFist();
  if(weaponSt!=WeaponState::NoWeapon) {
    closeWeapon(false);
    return false;
    }

  auto& weapon = *invent.currentMeleWeapon();
  auto  st     = weapon.is2H() ? WeaponState::W2H : WeaponState::W1H;
  if(!visual.setAnim(*this,st))
    return false;

  invent.switchActiveWeapon(*this,1);
  hnpc.weapon = (st==WeaponState::W1H ? 3:4);

  updateWeaponSkeleton();

  if(invent.currentMeleWeapon()->handle()->material==ItemMaterial::MAT_METAL)
    owner.emitSoundRaw("DRAWSOUND_ME.WAV",x,y+translateY(),z,500,nullptr); else
    owner.emitSoundRaw("DRAWSOUND_WO.WAV",x,y+translateY(),z,500,nullptr);
  return true;
  }

bool Npc::drawWeaponBow() {
  if(isFaling() || mvAlgo.isSwim())
    return false;
  auto weaponSt=invent.weaponState();
  if(weaponSt==WeaponState::Bow || weaponSt==WeaponState::CBow || invent.currentRangeWeapon()==nullptr)
    return true;
  if(weaponSt!=WeaponState::NoWeapon) {
    closeWeapon(false);
    return false;
    }

  auto& weapon = *invent.currentRangeWeapon();
  auto  st     = weapon.isCrossbow() ? WeaponState::CBow : WeaponState::Bow;
  if(!visual.setAnim(*this,st))
    return false;
  invent.switchActiveWeapon(*this,2);
  hnpc.weapon = (st==WeaponState::W1H ? 5:6);

  updateWeaponSkeleton();

  emitSoundEffect("DRAWSOUND_BOW",25,true);
  return true;
  }

bool Npc::drawMage(uint8_t slot) {
  if(isFaling() || mvAlgo.isSwim())
    return false;
  Item* it = invent.currentSpell(slot-3);
  if(it==nullptr) {
    closeWeapon(false);
    return true;
    }
  return drawSpell(it->spellId());
  }

bool Npc::drawSpell(int32_t spell) {
  if(isFaling() || mvAlgo.isSwim())
    return false;
  auto weaponSt=invent.weaponState();
  if(weaponSt!=WeaponState::NoWeapon && weaponSt!=WeaponState::Mage) {
    closeWeapon(false);
    return false;
    }
  if(!visual.setAnim(*this,WeaponState::Mage))
    return false;

  invent.switchActiveSpell(spell,*this);
  hnpc.weapon = 7;

  updateWeaponSkeleton();
  return true;
  }

bool Npc::canFinish(Npc& oth) {
  auto ws = weaponState();
  if(ws!=WeaponState::W1H && ws!=WeaponState::W2H)
    return false;
  if(!oth.isUnconscious() || !fghAlgo.isInAtackRange(*this,oth,owner.script()))
    return false;
  return true;
  }

void Npc::fistShoot() {
  doAttack(Anim::Atack);
  }

void Npc::blockFist() {
  auto weaponSt=invent.weaponState();
  if(weaponSt!=WeaponState::Fist)
    return;
  setAnim(Anim::AtackBlock);
  }

bool Npc::finishingMove() {
  if(currentTarget==nullptr || !canFinish(*currentTarget))
    return false;

  if(doAttack(Anim::AtackFinish)) {
    currentTarget->hnpc.attribute[Npc::Attribute::ATR_HITPOINTS] = 0;
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

void Npc::swingSwordL() {
  auto active=invent.activeWeapon();
  if(active==nullptr)
    return;
  doAttack(Anim::AtackL);
  }

void Npc::swingSwordR() {
  auto active=invent.activeWeapon();
  if(active==nullptr)
    return;
  doAttack(Anim::AtackR);
  }

void Npc::blockSword() {
  auto active=invent.activeWeapon();
  if(active==nullptr)
    return;
  setAnim(AnimationSolver::AtackBlock);
  }

bool Npc::castSpell() {
  auto active=invent.activeWeapon();
  /*
  if(active==nullptr ||
     (Anim::MagFirst<=animation.current && animation.current<=Anim::MagLast) ||
     (Anim::MagFirst<=animation.prevAni && animation.prevAni<=Anim::MagLast) || !isStanding())
    return false;*/
  if(active==nullptr)
    return false;

  const int32_t   splId = active->spellId();
  const SpellCode code  = SpellCode(owner.script().invokeMana(*this,currentTarget,*active));
  switch(code) {
    case SpellCode::SPL_SENDSTOP:
      setAnim(Anim::MagNoMana);
      break;
    case SpellCode::SPL_NEXTLEVEL:{
      auto ani = Npc::Anim(owner.script().spellCastAnim(*this,*active));
      setAnim(ani);
      }
      break;
    case SpellCode::SPL_SENDCAST: {
      auto ani = Npc::Anim(owner.script().spellCastAnim(*this,*active));
      AiAction a;
      a.act  = AI_PlayAnimById;
      a.i0   = ani;
      aiActions.push_back(a);
      owner.script().invokeSpell(*this,currentTarget,*active);
      if(currentTarget!=nullptr){
        currentTarget->lastHitSpell = splId;
        currentTarget->perceptionProcess(*this,nullptr,0,PERC_ASSESSMAGIC);
        }
      if(active->isSpell()) {
        invent.delItem(active->clsId(),1,*this);
        }
      break;
      }
    default:
      Log::d("unexpected Spell_ProcessMana result: '",int(code),"' for spell '",splId,"'");
      return false;
    }
  return true;
  }

bool Npc::aimBow() {
  auto active=invent.activeWeapon();
  if(active==nullptr)
    return false;
  return setAnim(Anim::AimBow);
  }

bool Npc::shootBow() {
  auto active=invent.activeWeapon();
  if(active==nullptr)
    return false;

  const int32_t munition = active->handle()->munition;
  if(!hasAmunition())
    return false;

  if(!setAnim(Anim::Atack))
    return false;

  float dx=1.f,dy=0.f,dz=0.f;
  if(currentTarget!=nullptr) {
    float y1 = currentTarget->physic.centerY();
    float y0 = y+translateY();

    dx = currentTarget->x-x;
    dy = y1-y0;
    dz = currentTarget->z-z;

    float lxz   = std::sqrt(dx*dx+0*0+dz*dz);
    float speed = DynamicWorld::bulletSpeed;
    float t     = lxz/speed;

    dy = (y1-y0)/t + 0.5f*DynamicWorld::gravity*t;
    dx/=t;
    dz/=t;

    dx = dx/speed;
    dy = dy/speed;
    dz = dz/speed;
    } else {
    float a = rotationRad()-float(M_PI/2);
    float c = std::cos(a), s = std::sin(a);
    dx = c;
    dz = s;
    }

  invent.delItem(size_t(munition),1,*this);
  auto& b = owner.shootBullet(size_t(munition), x,y+translateY(),z,dx,dy,dz);
  b.setOwner(this);
  b.setDamage(rangeDamageValue());

  auto rgn = currentRangeWeapon();
  if(rgn!=nullptr && rgn->isCrossbow())
    b.setHitChance(hnpc.hitChance[TALENT_CROSSBOW]/100.f); else
    b.setHitChance(hnpc.hitChance[TALENT_BOW]/100.f);

  return true;
  }

bool Npc::hasAmunition() const {
  auto active=invent.activeWeapon();
  if(active==nullptr)
    return false;
  const int32_t munition = active->handle()->munition;
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

bool Npc::isTalk() const {
  return owner.script().isTalk(*this);
  }

bool Npc::isPrehit() const {
  return visual.pose().isPrehit();
  }

bool Npc::isImmortal() const {
  return hnpc.flags & Daedalus::GEngineClasses::C_Npc::ENPCFlag::EFLAG_IMMORTAL;
  }

void Npc::setPerceptionTime(uint64_t time) {
  perceptionTime = time;
  }

void Npc::setPerceptionEnable(Npc::PercType t, size_t fn) {
  if(t>0 && t<PERC_Count)
    perception[t].func = fn;
  }

void Npc::setPerceptionDisable(Npc::PercType t) {
  if(t>0 && t<PERC_Count)
    perception[t].func = size_t(-1);
  }

void Npc::startDialog(Npc& pl) {
  if(pl.isDown())
    return;
  if(perceptionProcess(pl,nullptr,0,PERC_ASSESSTALK))
    setOther(&pl);
  }

bool Npc::perceptionProcess(Npc &pl,float quadDist) {
  static bool disable=false;
  if(disable)
    return false;

  if(isPlayer())
    return true;

  bool ret=false;
  if(hasPerc(PERC_ASSESSPLAYER) && canSenseNpc(pl,true)!=SensesBit::SENSE_NONE){
    if(perceptionProcess(pl,nullptr,quadDist,PERC_ASSESSPLAYER)) {
      //currentOther = &pl;
      ret          = true;
      }
    }
  Npc* enem=hasPerc(PERC_ASSESSENEMY) ? updateNearestEnemy() : nullptr;
  if(enem!=nullptr){
    float dist=qDistTo(*enem);
    if(perceptionProcess(*enem,nullptr,dist,PERC_ASSESSENEMY)){
      /*
      if(isTalk())
        Log::e("unxepected perc acton"); else
        setOther(nearestEnemy);*/
      ret          = true;
      } else {
      nearestEnemy = nullptr;
      }
    }
  return ret;
  }

bool Npc::perceptionProcess(Npc &pl, Npc* victum, float quadDist, Npc::PercType perc) {
  float r = hnpc.senses_range;
  r = r*r;
  if(quadDist>r || isPlayer())
    return false;
  if(hasPerc(perc)){
    owner.script().invokeState(this,&pl,victum,perception[perc].func);
    //currentOther=&pl;
    perceptionNextTime=owner.tickCount()+perceptionTime;
    return true;
    }
  perceptionNextTime=owner.tickCount()+perceptionTime;
  return false;
  }

bool Npc::hasPerc(Npc::PercType perc) const {
  return perception[perc].func!=size_t(-1);
  }

uint64_t Npc::percNextTime() const {
  return perceptionNextTime;
  }

bool Npc::setInteraction(Interactive *id) {
  if(currentInteract==id)
    return false;
  setAnim(Npc::Anim::InteractOut);
  if(currentInteract)
    currentInteract->dettach(*this);
  currentInteract=nullptr;

  if(id && id->attach(*this)) {
    owner.sendPassivePerc(*this,*this,*this,PERC_ASSESSUSEMOB);
    currentInteract=id;
    currentInteract->emitTriggerEvent();
    return true;
    }

  return false;
  }

bool Npc::isState(uint32_t stateFn) const {
  return aiState.funcIni==stateFn;
  }

bool Npc::wasInState(uint32_t stateFn) const {
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

void Npc::excRoutine(uint32_t callback) {
  //aiState.funcEnd=0; // no cleanup

  routines.clear();
  owner.script().invokeState(this,currentOther,nullptr,callback);
  aiState.eTime = gtime();
  //setInteraction(nullptr);
  //aiContinueRoutine();
  }

void Npc::multSpeed(float s) {
  mvAlgo.multSpeed(s);
  }

Npc::MoveCode Npc::testMove(const std::array<float,3> &pos,
                           std::array<float,3> &fallback,
                           float speed) {
  if(physic.testMove(pos,fallback,speed))
    return MV_OK;
  std::array<float,3> tmp;
  if(physic.testMove(fallback,tmp,0))
    return MV_CORRECT;
  return MV_FAILED;
  }

bool Npc::tryMove(const std::array<float,3> &dp) {
  if(dp[0]==0.f && dp[1]==0.f && dp[2]==0.f)
    return true;

  std::array<float,3> pos  = {x+dp[0],y+dp[1],z+dp[2]};
  std::array<float,3> norm = {};

  if(physic.tryMoveN(pos,norm)){
    return setViewPosition(pos);
    }

  const float speed = std::sqrt(dp[0]*dp[0]+dp[1]*dp[1]+dp[2]*dp[2]);
  if(speed>=physic.radius() || speed==0.f)
    return false;

  float scale=speed*0.25f;
  for(int i=1;i<4+3;++i){
    std::array<float,3> p=pos;
    p[0]+=norm[0]*scale*i;
    p[2]+=norm[2]*scale*i;

    std::array<float,3> nn={};
    if(physic.tryMoveN(p,nn)) {
      return setViewPosition(p);
      }
    }
  return false;
  }

Npc::JumpCode Npc::tryJump(const std::array<float,3> &p0) {
  float len = 40.f;
  float rot = rotationRad();
  float s   = std::sin(rot), c = std::cos(rot);
  float dx  = len*s, dz = -len*c;
  float trY = -translateY();

  auto pos = p0;
  pos[0]+=dx;
  pos[2]+=dz;

  if(physic.testMove(pos))
    return JumpCode::JM_OK;

  pos[1] = p0[1]+clampHeight(Anim::JumpUpLow)+trY;
  if(physic.testMove(pos))
    return JumpCode::JM_UpLow;

  pos[1] = p0[1]+clampHeight(Anim::JumpUpMid)+trY;
  if(physic.testMove(pos))
    return JumpCode::JM_UpMid;

  return JumpCode::JM_Up;
  }

float Npc::clampHeight(Npc::Anim a) const {
  auto& g = owner.script().guildVal();
  auto  i = guild();

  switch(a) {
    case AnimationSolver::JumpUpLow:
      return g.jumplow_height[i];
    case AnimationSolver::JumpUpMid:
      return g.jumpmid_height[i];
    default:
      return 0;
    }
  }

std::vector<GameScript::DlgChoise> Npc::dialogChoises(Npc& player,const std::vector<uint32_t> &except,bool includeImp) {
  return owner.script().dialogChoises(&player.hnpc,&this->hnpc,except,includeImp);
  }

void Npc::aiLookAt(Npc *other) {
  if(aiActions.size()>0 && aiActions.back().act==AI_LookAt)
    return;
  AiAction a;
  a.act    = AI_LookAt;
  a.target = other;
  aiActions.push_back(a);
  }

void Npc::aiStopLookAt() {
  AiAction a;
  a.act = AI_StopLookAt;
  aiActions.push_back(a);
  }

void Npc::aiRemoveWeapon() {
  AiAction a;
  a.act = AI_RemoveWeapon;
  aiActions.push_back(a);
  }

void Npc::aiTurnToNpc(Npc *other) {
  AiAction a;
  a.act    = AI_TurnToNpc;
  a.target = other;
  aiActions.push_back(a);
  }

void Npc::aiGoToNpc(Npc *other) {
  AiAction a;
  a.act    = AI_GoToNpc;
  a.target = other;
  aiActions.push_back(a);
  }

void Npc::aiGoToNextFp(std::string fp) {
  AiAction a;
  a.act = AI_GoToNextFp;
  a.s0  = fp;
  aiActions.push_back(a);
  }

void Npc::aiStartState(uint32_t stateFn, int behavior, Npc* other, std::string wp) {
  auto& st = owner.script().getAiState(stateFn);(void)st;

  AiAction a;
  a.act    = AI_StartState;
  a.func   = stateFn;
  a.i0     = behavior;
  a.s0     = std::move(wp);
  a.target = other;
  aiActions.push_back(a);
  }

void Npc::aiPlayAnim(const std::string& ani) {
  AiAction a;
  a.act  = AI_PlayAnim;
  a.s0   = ani; //TODO: COW-Strings
  aiActions.push_back(a);
  }

void Npc::aiPlayAnimBs(const std::string &ani,BodyState bs) {
  AiAction a;
  a.act  = AI_PlayAnimBs;
  a.s0   = ani;
  a.i0   = int(bs);
  aiActions.push_back(a);
  }

void Npc::aiWait(uint64_t dt) {
  AiAction a;
  a.act  = AI_Wait;
  a.i0   = int(dt);
  aiActions.push_back(a);
  }

void Npc::aiStandup() {
  AiAction a;
  a.act = AI_StandUp;
  aiActions.push_back(a);
  }

void Npc::aiStandupQuick() {
  AiAction a;
  a.act = AI_StandUpQuick;
  aiActions.push_back(a);
  }

void Npc::aiGoToPoint(const WayPoint& to) {
  AiAction a;
  a.act   = AI_GoToPoint;
  a.point = &to;
  aiActions.push_back(a);
  }

void Npc::aiEquipArmor(int32_t id) {
  AiAction a;
  a.act = AI_EquipArmor;
  a.i0  = id;
  aiActions.push_back(a);
  }

void Npc::aiEquipBestArmor() {
  AiAction a;
  a.act = AI_EquipBestArmor;
  aiActions.push_back(a);
  }

void Npc::aiEquipBestMeleWeapon() {
  AiAction a;
  a.act = AI_EquipMelee;
  aiActions.push_back(a);
  }

void Npc::aiEquipBestRangeWeapon() {
  AiAction a;
  a.act = AI_EquipRange;
  aiActions.push_back(a);
  }

void Npc::aiUseMob(const std::string &name, int st) {
  AiAction a;
  a.act = AI_UseMob;
  a.s0  = name;
  a.i0  = st;
  aiActions.push_back(a);
  }

void Npc::aiUseItem(int32_t id) {
  AiAction a;
  a.act = AI_UseItem;
  a.i0  = id;
  aiActions.push_back(a);
  }

void Npc::aiTeleport(const WayPoint &to) {
  AiAction a;
  a.act   = AI_Teleport;
  a.point = &to;
  aiActions.push_back(a);
  }

void Npc::aiReadyMeleWeapon() {
  AiAction a;
  a.act = AI_DrawWeaponMele;
  aiActions.push_back(a);
  }

void Npc::aiReadyRangeWeapon() {
  AiAction a;
  a.act = AI_DrawWeaponRange;
  aiActions.push_back(a);
  }

void Npc::aiReadySpell(int32_t spell,int32_t /*mana*/) {
  AiAction a;
  a.act = AI_DrawSpell;
  a.i0  = spell;
  aiActions.push_back(a);
  }

void Npc::aiAtack() {
  AiAction a;
  a.act = AI_Atack;
  aiActions.push_back(a);
  }

void Npc::aiFlee() {
  AiAction a;
  a.act = AI_Flee;
  aiActions.push_back(a);
  }

void Npc::aiDodge() {
  AiAction a;
  a.act = AI_Dodge;
  aiActions.push_back(a);
  }

void Npc::aiUnEquipWeapons() {
  AiAction a;
  a.act = AI_UnEquipWeapons;
  aiActions.push_back(a);
  }

void Npc::aiUnEquipArmor() {
  AiAction a;
  a.act = AI_UnEquipArmor;
  aiActions.push_back(a);
  }

void Npc::aiProcessInfo(Npc &other) {
  AiAction a;
  a.act    = AI_ProcessInfo;
  a.target = &other;
  aiActions.push_back(a);
  }

void Npc::aiOutput(Npc& to, std::string text, int order) {
  AiAction a;
  a.act    = AI_Output;
  a.s0     = std::move(text);
  a.target = &to;
  a.i0     = order;
  aiActions.push_back(a);
  }

void Npc::aiOutputSvm(Npc &to, std::string text, int order) {
  AiAction a;
  a.act    = AI_OutputSvm;
  a.s0     = std::move(text);
  a.target = &to;
  a.i0     = order;
  aiActions.push_back(a);
  }

void Npc::aiOutputSvmOverlay(Npc &to, std::string text, int order) {
  AiAction a;
  a.act    = AI_OutputSvmOverlay;
  a.s0     = std::move(text);
  a.target = &to;
  a.i0     = order;
  aiActions.push_back(a);
  }

void Npc::aiStopProcessInfo() {
  AiAction a;
  a.act = AI_StopProcessInfo;
  aiActions.push_back(a);
  }

void Npc::aiContinueRoutine() {
  AiAction a;
  a.act = AI_ContinueRoutine;
  aiActions.push_back(a);
  }

void Npc::aiAlignToFp() {
  AiAction a;
  a.act = AI_AlignToFp;
  aiActions.push_back(a);
  }

void Npc::aiAlignToWp() {
  AiAction a;
  a.act = AI_AlignToWp;
  aiActions.push_back(a);
  }

void Npc::aiSetNpcsToState(size_t func, int32_t radius) {
  AiAction a;
  a.act  = AI_SetNpcsToState;
  a.func = func;
  a.i0   = radius;
  aiActions.push_back(a);
  }

void Npc::aiSetWalkMode(WalkBit w) {
  AiAction a;
  a.act  = AI_SetWalkMode;
  a.i0   = int(w);
  aiActions.push_back(a);
  }

void Npc::aiFinishingMove(Npc &other) {
  AiAction a;
  a.act    = AI_FinishingMove;
  a.target = &other;
  aiActions.push_back(a);
  }

bool Npc::isAiQueueEmpty() const {
  return aiActions.size()==0 &&
         waitTime<owner.tickCount() &&
         currentGoTo==nullptr &&
         currentGoToNpc==nullptr;
  }

void Npc::clearAiQueue() {
  aiActions.clear();
  waitTime        = 0;
  faiWaitTime     = 0;
  currentGoTo     = nullptr;
  currentGoToNpc  = nullptr;
  currentGoToFlag = GoToHint::GT_Default;
  wayPath.clear();
  fghAlgo.onClearTarget();
  }

void Npc::attachToPoint(const WayPoint *p) {
  currentFp     = p;
  currentFpLock = FpLock(currentFp);
  }

void Npc::clearGoTo() {
  currentGoTo     = nullptr;
  currentGoToNpc  = nullptr;
  currentGoToFlag = GoToHint::GT_Default;
  wayPath.clear();
  mvAlgo.aiGoTo(nullptr);
  }

bool Npc::canSeeNpc(const Npc &oth, bool freeLos) const {
  return canSeeNpc(oth.x,oth.y+180,oth.z,freeLos);
  }

bool Npc::canSeeNpc(float tx, float ty, float tz, bool freeLos) const {
  SensesBit s = canSenseNpc(tx,ty,tz,freeLos);
  return int32_t(s&SensesBit::SENSE_SEE)!=0;
  }

SensesBit Npc::canSenseNpc(const Npc &oth, bool freeLos) const {
  return canSenseNpc(oth.x,oth.y+180,oth.z,freeLos);
  }

SensesBit Npc::canSenseNpc(float tx, float ty, float tz, bool freeLos) const {
  DynamicWorld* w = owner.physic();
  static const double ref = std::cos(100*M_PI/180.0); // spec requires +-100 view angle range

  if(qDistTo(tx,ty,tz)>hnpc.senses_range*hnpc.senses_range)
    return SensesBit::SENSE_NONE;

  SensesBit ret=SensesBit::SENSE_NONE;
  if(owner.roomAt({tx,ty,tz})==owner.roomAt({x,y,z})) {
    ret = ret | SensesBit::SENSE_SMELL;
    ret = ret | SensesBit::SENSE_HEAR; // TODO:sneaking
    }

  if(!freeLos){
    float dx  = x-tx, dz=z-tz;
    float dir = angleDir(dx,dz);
    float da  = float(M_PI)*(angle-dir)/180.f;
    if(double(std::cos(da))<=ref)
      if(!w->ray(x,y+180,z, tx,ty,tz).hasCol)
        ret = ret | SensesBit::SENSE_SEE;
    } else {
    // TODO: npc eyesight height
    if(!w->ray(x,y+180,z, tx,ty,tz).hasCol)
      ret = ret | SensesBit::SENSE_SEE;
    }
  return ret & SensesBit(hnpc.senses);
  }

void Npc::updatePos() {
  if(durtyTranform==TR_Pos){
    visual.setPos(x,y,z);
    } else {
    Matrix4x4 mt;
    mt.identity();
    mt.translate(x,y,z);
    mt.rotateOY(180-angle);
    mt.scale(sz[0],sz[1],sz[2]);
    visual.setPos(mt);
    }
  }


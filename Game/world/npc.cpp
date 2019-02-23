#include "npc.h"

#include <Tempest/Matrix4x4>

#include "interactive.h"
#include "graphics/skeleton.h"
#include "graphics/posepool.h"
#include "worldscript.h"
#include "trigger.h"
#include "world.h"
#include "resources.h"

using namespace Tempest;

Npc::Npc(WorldScript &owner, Daedalus::GameState::NpcHandle hnpc)
  :owner(owner),hnpc(hnpc),mvAlgo(*this,owner.world()){
  }

Npc::~Npc(){
  if(currentInteract)
    currentInteract->dettach(*this);
  }

void Npc::setPosition(float ix, float iy, float iz) {
  x = ix;
  y = iy;
  z = iz;
  updatePos();
  }

void Npc::setPosition(const std::array<float,3> &pos) {
  if(x==pos[0] && y==pos[1] && z==pos[2])
    return;
  x = pos[0];
  y = pos[1];
  z = pos[2];
  updatePos();
  }

void Npc::setDirection(float x, float /*y*/, float z) {
  float a=angleDir(x,z);
  setDirection(a);
  }

void Npc::setDirection(const std::array<float,3> &pos) {
  setDirection(pos[0],pos[1],pos[2]);
  }

void Npc::setDirection(float rotation) {
  if(angle==rotation)
    return;
  angle = rotation;
  updatePos();
  }

float Npc::angleDir(float x, float z) {
  float a=-90;
  if(x!=0.f || z!=0.f)
    a = 90+180.f*std::atan2(z,x)/float(M_PI);
  return a;
  }

void Npc::clearSpeed() {
  mvAlgo.clearSpeed();
  }

void Npc::setAiType(Npc::AiType t) {
  aiType=t;
  }

bool Npc::isPlayer() const {
  return aiType==Npc::AiType::Player;
  }

bool Npc::startClimb(Anim ani) {
  if(mvAlgo.startClimb()){
    setAnim(ani);
    return true;
    }
  return false;
  }

std::array<float,3> Npc::position() const {
  return {{x,y,z}};
  }

std::array<float,3> Npc::cameraBone() const {
  if(skInst==nullptr)
    return {{}};
  auto bone=skInst->cameraBone();
  std::array<float,3> r={{}};
  bone.project(r[0],r[1],r[2]);
  pos.project (r[0],r[1],r[2]);
  return r;
  }

float Npc::rotation() const {
  return angle;
  }

float Npc::rotationRad() const {
  return angle*float(M_PI)/180.f;
  }

float Npc::translateY() const {
  return skInst ? skInst->translateY() : 0;
  }

Npc *Npc::lookAtTarget() const {
  return currentLookAt;
  }

void Npc::updateAnimation() {
  for(size_t i=0;i<overlay.size();){
    auto& ov = overlay[i];
    if(ov.time!=0 && ov.time<owner.tickCount())
      overlay.erase(overlay.begin()+int(i)); else
      ++i;
    }

  if(skInst!=nullptr){
    uint64_t dt = owner.tickCount() - sAnim;
    skInst->update(dt);

    head  .setSkeleton(*skInst,pos);
    if(armour.isEmpty())
      view  .setSkeleton(*skInst,pos); else
      armour.setSkeleton(*skInst,pos);
    }
  }

const char *Npc::displayName() const {
  return owner.vmNpc(hnpc).name[0].c_str();
  }

void Npc::setName(const std::string &n) {
  name = n;
  }

void Npc::addOverlay(const Skeleton* sk,uint64_t time) {
  if(sk==nullptr)
    return;
  if(time!=0)
    time+=owner.tickCount();

  Overlay ov = {sk,time};
  overlay.push_back(ov);
  if(animSq) {
    auto ani=animSequence(animSq->name.c_str());
    invalidateAnim(ani,skeleton);
    } else {
    setAnim(Anim::Idle);
    }
  }

void Npc::delOverlay(const char *sk) {
  if(overlay.size()==0)
    return;
  auto skelet = Resources::loadSkeleton(sk);
  delOverlay(skelet);
  }

void Npc::delOverlay(const Skeleton *sk) {
  for(size_t i=0;i<overlay.size();++i)
    if(overlay[i].sk==sk){
      overlay.erase(overlay.begin()+int(i));
      return;
      }
  }

void Npc::setVisual(const Skeleton* v) {
  skeleton = v;
  //skInst.bind(skeleton);

  current=NoAnim;
  setAnim(Anim::Idle);

  head  .setSkeleton(skeleton);
  view  .setSkeleton(skeleton);
  armour.setSkeleton(skeleton);
  invalidateAnim(animSq,skeleton);
  setPos(pos); // update obj matrix
  }

void Npc::addOverlay(const char *sk, uint64_t time) {
  auto skelet = Resources::loadSkeleton(sk);
  addOverlay(skelet,time);
  }

void Npc::setVisualBody(StaticObjects::Mesh&& h, StaticObjects::Mesh &&body, int32_t bodyVer, int32_t bodyColor) {
  head    = std::move(h);
  view    = std::move(body);

  vColor  = bodyVer;
  bdColor = bodyColor;

  head.setSkeleton(skeleton,"BIP01 HEAD");
  view.setSkeleton(skeleton);

  invent.updateArmourView(owner,*this);
  updatePos(); // update obj matrix
  }

void Npc::setArmour(StaticObjects::Mesh &&a) {
  armour = std::move(a);
  armour.setSkeleton(skeleton);
  setPos(pos);
  }

void Npc::setPhysic(DynamicWorld::Item &&item) {
  physic = std::move(item);
  physic.setPosition(x,y,z);
  }

void Npc::setFatness(float) {
  }

void Npc::setScale(float x, float y, float z) {
  sz[0]=x;
  sz[1]=y;
  sz[2]=z;
  updatePos();
  }

void Npc::setAnim(Npc::Anim a) {
  setAnim(a,weaponSt);
  }

void Npc::setAnim(Npc::Anim a,WeaponState nextSt) {
  if(animSq!=nullptr){
    if(current==a && nextSt==weaponSt && animSq->animCls==Animation::Loop)
      return;
    if((animSq->animCls==Animation::Transition &&
        current!=RotL && current!=RotR && current!=MoveL && current!=MoveR && // no idea why this animations maked as Transition
        !(current==Move && a==Jump)) && // allow to jump at any point of run animation
       !animSq->isFinished(owner.tickCount()-sAnim))
      return;
    }
  auto ani = solveAnim(a,weaponSt,current,nextSt);
  current =a;
  weaponSt=nextSt;
  if(ani==animSq) {
    if(animSq!=nullptr && animSq->animCls==Animation::Transition){
      invalidateAnim(ani,skeleton); // restart anim
      }
    return;
    }
  invalidateAnim(ani,skeleton);
  }

ZMath::float3 Npc::animMoveSpeed(uint64_t dt) const {
  if(animSq!=nullptr){
    return animSq->speed(owner.tickCount()-sAnim,dt);
    }
  return ZMath::float3();
  }

ZMath::float3 Npc::animMoveSpeed(Anim a,uint64_t dt) const {
  auto ani = solveAnim(a);
  if(ani!=nullptr && ani->isMove()){
    if(ani==animSq || (current==a && animSq!=nullptr)){
      return animSq->speed(owner.tickCount()-sAnim,dt);
      }
    return ani->speed(0,dt);
    }
  return ZMath::float3();
  }

bool Npc::isFlyAnim() const {
  if(animSq==nullptr)
    return false;
  return animSq->isFly() && !animSq->isFinished(owner.tickCount()-sAnim) &&
         current!=Fall && current!=FallDeep;
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
  if(t<TALENT_MAX) {
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
  if(t<TALENT_MAX)
    return talentsSk[t];
  return 0;
  }

void Npc::setTalentValue(Npc::Talent t, int32_t lvl) {
  if(t<TALENT_MAX)
    talentsVl[t] = lvl;
  }

int32_t Npc::talentValue(Npc::Talent t) const {
  if(t<TALENT_MAX)
    return talentsVl[t];
  return 0;
  }

int32_t Npc::hitChanse(Npc::Talent t) const {
  if(t<Daedalus::GEngineClasses::MAX_HITCHANCE)
    return owner.vmNpc(hnpc).hitChance[t];
  return 0;
  }

bool Npc::isRefuseTalk() const {
  return refuseTalkMilis<owner.tickCount();
  }

void Npc::setRefuseTalk(uint64_t milis) {
  refuseTalkMilis = owner.tickCount()+milis;
  }

int32_t Npc::attribute(Npc::Attribute a) const {
  if(a<ATR_MAX)
    return owner.vmNpc(hnpc).attribute[a];
  return 0;
  }

void Npc::changeAttribute(Npc::Attribute a, int32_t val) {
  if(a>=ATR_MAX)
    return;
  auto& v = owner.vmNpc(hnpc);
  v.attribute[a]+=val;
  if(v.attribute[a]<0)
    v.attribute[a]=0;
  if(a==ATR_HITPOINTS && v.attribute[a]>v.attribute[ATR_HITPOINTSMAX])
    v.attribute[a] = v.attribute[ATR_HITPOINTSMAX];
  if(a==ATR_MANA && v.attribute[a]>v.attribute[ATR_MANAMAX])
    v.attribute[a] = v.attribute[ATR_MANAMAX];

  if(val<0)
    invent.invalidateCond(*this);

  if(a==ATR_HITPOINTS && v.attribute[a]<=0){
    //TODO: death;
    }
  }

int32_t Npc::protection(Npc::Protection p) const {
  if(p<PROT_MAX)
    return owner.vmNpc(hnpc).protection[p];
  return 0;
  }

void Npc::changeProtection(Npc::Protection p, int32_t val) {
  if(p<PROT_MAX)
    owner.vmNpc(hnpc).protection[p]=val;
  }

uint32_t Npc::instanceSymbol() const {
  return uint32_t(owner.vmNpc(hnpc).instanceSymbol);
  }

uint32_t Npc::guild() const {
  return uint32_t(owner.vmNpc(hnpc).guild);
  }

int32_t Npc::magicCyrcle() const {
  return talentSkill(TALENT_RUNES);
  }

int32_t Npc::level() const {
  return owner.vmNpc(hnpc).level;
  }

int32_t Npc::experience() const {
  return owner.vmNpc(hnpc).exp;
  }

int32_t Npc::experienceNext() const {
  return owner.vmNpc(hnpc).exp_next;
  }

int32_t Npc::learningPoints() const {
  return owner.vmNpc(hnpc).lp;
  }

bool Npc::isDead() const {
  return (bodySt & BS_DEAD)!=0;
  }

bool Npc::implLookAt(uint64_t dt) {
  if(currentTurnTo!=nullptr){
    auto dx = currentTurnTo->x-x;
    auto dz = currentTurnTo->z-z;
    if(implLookAt(dx,dz,dt))
      return true;
    currentTurnTo=nullptr;
    return false;
    }
  if(currentLookAt!=nullptr) {
    auto dx = currentLookAt->x-x;
    auto dz = currentLookAt->z-z;
    if(implLookAt(dx,dz,dt))
      return true;
    currentTurnTo=nullptr;
    return false;
    }
  return false;
  }

bool Npc::implLookAt(float dx, float dz, uint64_t dt) {
  float       a    = angleDir(dx,dz);
  float       da   = int(a-angle)%360;
  float       step = 120*dt/1000.f;

  if(std::abs(da)<step){
    setDirection(a);
    if(animSq!=nullptr && !animSq->isFinished(owner.tickCount()-sAnim))
      return true;
    setAnim(Anim::Idle);
    return false;
    }

  const auto sgn = std::sin(double(da)*M_PI/180.0);
  if(sgn<0) {
    setDirection(angle-step);
    setAnim(Anim::RotR);
    } else {
    setDirection(angle+step);
    setAnim(Anim::RotL);
    }
  return true;
  }

void Npc::invalidateAnim(const Animation::Sequence *ani,const Skeleton* sk) {
  animSq = ani;
  sAnim  = owner.tickCount();
  skInst = PosePool::get(sk,ani,sAnim);
  }

void Npc::tick(uint64_t dt) {
  if(aiType==AiType::Player) {
    mvAlgo.tick(dt);
    return;
    }

  setAnim(current);
  mvAlgo.tick(dt);

  if(waitTime>=owner.tickCount())
    return;

  if(implLookAt(dt))
    return;

  if(aiActions.size()==0) {
    tickRoutine();
    if(invent.isChanged())
      invent.autoEquip(owner,*this);
    return;
    }

  auto act = std::move(aiActions.front());
  aiActions.pop();

  switch(act.act) {
    case AI_None: break;
    case AI_LookAt:
      currentLookAt=act.target;
      break;
    case AI_TurnToNpc:
      currentTurnTo=act.target;
      break;
    case AI_StopLookAt:
      currentLookAt=nullptr;
      break;
    case AI_RemoveWeapon:
      break;
    case AI_StartState:
      startState(act.func,act.i0==0,act.s0);
      break;
    case AI_PlayAnim:{
      auto a = animSequence(act.s0.c_str());
      if(a!=nullptr){
        auto tag=animByName(a->name);
        if(tag!=Anim::NoAnim){
          setAnim(tag);
          } else {
          Log::d("AI_PlayAnim: unrecognized anim: \"",a->name,"\"");
          if(animSq!=a)
            invalidateAnim(a,skeleton);
          }
        }
      break;
      }
    case AI_Wait:
      waitTime = owner.tickCount()+uint64_t(act.i0);
      break;
    case AI_StandUp:
      // TODO: not implemented
      break;
    }

  if(invent.isChanged())
    invent.autoEquip(owner,*this);
  }

void Npc::startDialog(Npc* other) {
  auto sym     = owner.getSymbolIndex("ZS_Talk");
  currentOther = other;
  startState(sym,true,"");
  }

void Npc::startState(size_t id,bool loop,const std::string &wp) {
  if(id==0)
    return;
  if(aiState.funcIni!=0 && aiState.started)
    owner.invokeState(this,currentOther,aiState.funcEnd); // cleanup

  if(!wp.empty()){
    auto& v=owner.vmNpc(hnpc);
    v.wp = wp;
    }

  auto& fn = owner.getSymbol(id);
  aiState.started  = false;
  aiState.funcIni  = id;
  aiState.funcLoop = loop ? owner.getSymbolIndex(fn.name+"_Loop") : 0;
  aiState.funcEnd  = owner.getSymbolIndex(fn.name+"_End");
  aiState.sTime    = owner.tickCount();
  }

void Npc::tickRoutine() {
  if(aiState.funcIni==0){
    auto& v=owner.vmNpc(hnpc);
    if(v.start_aistate!=0)
      startState(v.start_aistate,true,"");
    }

  if(aiState.started) {
    int loop = owner.invokeState(this,currentOther,aiState.funcLoop);
    if(loop!=0){
      owner.invokeState(this,currentOther,aiState.funcEnd);
      aiState = AiState();
      }
    } else {
    owner.invokeState(this,currentOther,aiState.funcIni);
    aiState.started=true;
    }

  // auto r = currentRoutine();
  // owner.invokeState(hnpc,r.callback);
  }

Npc::BodyState Npc::bodyState() const {
  return bodySt;
  }

void Npc::setToFistMode() {
  }

void Npc::setToFightMode(const uint32_t item) {
  if(invent.itemCount(item)==0)
    addItem(item,1);

  useItem(item);
  drawWeapon1H();
  }

void Npc::addItem(const uint32_t item, size_t count) {
  invent.addItem(item,count,owner);
  }

void Npc::addItem(std::unique_ptr<Item>&& i) {
  invent.addItem(std::move(i),owner);
  }

void Npc::addItem(uint32_t id, Interactive &chest) {
  Inventory::trasfer(invent,chest.inventory(),nullptr,id,1,owner);
  }

void Npc::moveItem(uint32_t id, Interactive &to) {
  Inventory::trasfer(to.inventory(),invent,this,id,1,owner);
  }

size_t Npc::hasItem(uint32_t id) const {
  return invent.itemCount(id);
  }

void Npc::delItem(uint32_t item, uint32_t amount) {
  invent.delItem(item,amount,owner,*this);
  }

void Npc::useItem(uint32_t item) {
  invent.use(item,owner,*this);
  }

void Npc::unequipItem(uint32_t item) {
  invent.unequip(item,owner,*this);
  }

void Npc::closeWeapon() {
  setAnim(current,WeaponState::NoWeapon);
  }

void Npc::drawWeaponFist() {
  if(weaponSt==Fist)
    closeWeapon(); else
    setAnim(current,WeaponState::Fist);
  }

void Npc::drawWeapon1H() {
  if(weaponSt==W1H)
    closeWeapon(); else
    setAnim(current,WeaponState::W1H);
  }

void Npc::drawWeapon2H() {
  if(weaponSt==W2H)
    closeWeapon(); else
    setAnim(current,WeaponState::W2H);
  }

void Npc::drawWeaponBow() {
  if(weaponSt==Bow)
    closeWeapon(); else
    setAnim(current,WeaponState::Bow);
  }

void Npc::drawWeaponCBow() {
  if(weaponSt==CBow)
    closeWeapon(); else
    setAnim(current,WeaponState::CBow);
  }

void Npc::setPerceptionTime(uint64_t time) {
  perceptionTime = time;
  }

void Npc::setPerceptionEnable(Npc::PercType t, size_t fn) {
  if(t>0 && t<PERC_Count)
    perception[t].func = fn;
  }

bool Npc::setInteraction(Interactive *id) {
  if(currentInteract==id)
    return false;
  if(currentInteract)
    currentInteract->dettach(*this);
  currentInteract=nullptr;

  if(id && id->attach(*this)){
    currentInteract=id;
    auto st = currentInteract->stateFunc();
    if(!st.empty()) {
      owner.useInteractive(hnpc,st);
      }
    if(auto tr = currentInteract->triggerTarget()){
      Log::d("TODO: trigger[",tr->name(),"]");
      }
    return true;
    }

  return false;
  }

bool Npc::isState(uint32_t stateFn) const {
  return aiState.funcIni==stateFn;
  }

uint64_t Npc::stateTime() const {
  return owner.tickCount()-aiState.sTime;
  }

void Npc::addRoutine(gtime s, gtime e, uint32_t callback) {
  Routine r;
  r.start    = s;
  r.end      = e;
  r.callback = callback;
  routines.push_back(r);
  }

void Npc::multSpeed(float s) {
  mvAlgo.multSpeed(s);
  }

Npc::MoveCode Npc::tryMove(const std::array<float,3> &pos,
                           std::array<float,3> &fallback,
                           float speed) {
  if(physic.tryMove(pos,fallback,speed))
    return MV_OK;
  std::array<float,3> tmp;
  if(physic.tryMove(fallback,tmp,0))
    return MV_CORRECT;
  return MV_FAILED;
  }

Npc::MoveCode Npc::tryMoveVr(const std::array<float,3> &pos, std::array<float,3> &fallback, float speed) {
  if(physic.tryMove(pos,fallback,speed))
    return MV_OK;

  std::array<float,3> p=fallback;
  for(int i=1;i<5;++i){
    if(physic.tryMove(p,fallback,speed))
      return MV_CORRECT;
    p=fallback;
    }
  return MV_FAILED;
  }

Npc::JumpCode Npc::tryJump(const std::array<float,3> &p0) {
  float len = 20.f;
  float rot = rotationRad();
  float s   = std::sin(rot), c = std::cos(rot);
  float dx  = len*s, dz = -len*c;

  auto pos = p0;
  pos[0]+=dx;
  pos[2]+=dz;

  if(physic.tryMove(pos))
    return JM_OK;

  pos[1] = p0[1]+clampHeight(Anim::JumpUpLow);
  if(physic.tryMove(pos))
    return JM_UpLow;

  pos[1] = p0[1]+clampHeight(Anim::JumpUpMid);
  if(physic.tryMove(pos))
    return JM_UpMid;

  return JM_Up;
  }

float Npc::clampHeight(Npc::Anim a) const {
  switch(a) {
    case Npc::JumpUpLow:
      return 60;
    case Npc::JumpUpMid:
      return 155;
    default:
      return 0;
    }
  }

std::vector<WorldScript::DlgChoise> Npc::dialogChoises(Npc& player,const std::vector<uint32_t> &except) {
  return owner.dialogChoises(player.hnpc,this->hnpc,except);
  }

void Npc::aiLookAt(Npc *other) {
  AiAction a;
  a.act    = AI_LookAt;
  a.target = other;
  aiActions.push(a);
  }

void Npc::aiStopLookAt() {
  AiAction a;
  a.act = AI_StopLookAt;
  aiActions.push(a);
  }

void Npc::aiRemoveWeapon() {
  AiAction a;
  a.act = AI_RemoveWeapon;
  aiActions.push(a);
  }

void Npc::aiTurnToNpc(Npc *other) {
  AiAction a;
  a.act    = AI_TurnToNpc;
  a.target = other;
  aiActions.push(a);
  }

void Npc::aiStartState(uint32_t stateFn, int behavior, std::string wp) {
  AiAction a;
  a.act  = AI_StartState;
  a.func = stateFn;
  a.i0   = behavior;
  a.s0   = std::move(wp);
  aiActions.push(a);
  }

void Npc::aiPlayAnim(std::string ani) {
  AiAction a;
  a.act  = AI_PlayAnim;
  a.s0   = std::move(ani);
  aiActions.push(a);
  }

void Npc::aiWait(uint64_t dt) {
  AiAction a;
  a.act  = AI_Wait;
  a.i0   = int(dt);
  aiActions.push(a);
  }

void Npc::aiStandup() {
  AiAction a;
  a.act = AI_StandUp;
  aiActions.push(a);
  }

void Npc::aiClearQueue() {
  aiActions=std::queue<AiAction>();
  }

void Npc::updatePos() {
  Matrix4x4 mt;
  mt.identity();
  mt.translate(x,y,z);
  mt.rotateOY(180-angle);
  mt.scale(sz[0],sz[1],sz[2]);

  setPos(mt);
  }

void Npc::setPos(const Matrix4x4 &m) {
  pos = m;
  head  .setObjMatrix(pos);
  if(armour.isEmpty()) {
    view  .setObjMatrix(pos);
    } else {
    armour.setObjMatrix(pos);
    view  .setObjMatrix(Matrix4x4());
    }
  physic.setPosition(x,y,z);
  }

const Animation::Sequence *Npc::solveAnim(Npc::Anim a) const {
  return solveAnim(a,weaponSt,a,weaponSt);
  }

const Animation::Sequence *Npc::solveAnim(Npc::Anim a, WeaponState st0, Npc::Anim cur, WeaponState st) const {
  if(skeleton==nullptr)
    return nullptr;

  if(st0==WeaponState::NoWeapon){
    if(a==Anim::Idle && cur==a && st==WeaponState::W1H)
      return animSequence("T_1H_2_1HRUN");
    if(a==Anim::Move && cur==a && st==WeaponState::W1H)
      return animSequence("T_MOVE_2_1HMOVE");
    if(a==Anim::Idle && cur==a && st==WeaponState::W2H)
      return animSequence("T_RUN_2_2H");
    if(a==Anim::Move && cur==a && st==WeaponState::W2H)
      return animSequence("T_MOVE_2_2HMOVE");
    }
  if(st0==WeaponState::W1H && st==WeaponState::NoWeapon){
    if(a==Anim::Idle && cur==a)
      return animSequence("T_1HMOVE_2_MOVE");
    if(a==Anim::Move && cur==a)
      return animSequence("T_1HMOVE_2_MOVE");
    }
  if(st0==WeaponState::W2H && st==WeaponState::NoWeapon){
    if(a==Anim::Idle && cur==a)
      return animSequence("T_RUN_2_2H");
    if(a==Anim::Move && cur==a)
      return animSequence("T_2HMOVE_2_MOVE");
    }

  if(true) {
    if(st0==WeaponState::Fist && st==WeaponState::NoWeapon)
      return animSequence("T_FISTMOVE_2_MOVE");
    if(st0==WeaponState::NoWeapon && st==WeaponState::Fist)
      return solveAnim("S_%sRUN",st);
    }

  if(currentInteract!=nullptr) {
    if(cur!=Interact && a==Interact)
      return animSequence(currentInteract->anim(Interactive::In));
    if(cur==Interact && a==Interact)
      return animSequence(currentInteract->anim(Interactive::Active));
    if(cur==Interact && a!=Interact)
      return animSequence(currentInteract->anim(Interactive::Out));
    }

  if(cur==Anim::NoAnim && a==Idle)
    return solveAnim("S_%sRUN",st);
  if(cur==Anim::Idle && a==cur)
    return solveAnim("S_%sRUN",st);
  if(cur==Anim::Idle && a==Move)
    return animSequence("T_RUN_2_RUNL");
  if(cur==Anim::Fall && a==Move)
    return animSequence("T_RUN_2_RUNL");

  if(a==Anim::RotL)
    return solveAnim("T_%sRUNTURNL",st);
  if(a==Anim::RotR)
    return solveAnim("T_%sRUNTURNR",st);
  if(a==Anim::MoveL)
    return solveAnim("T_%sRUNSTRAFEL",st);
  if(a==Anim::MoveR)
    return solveAnim("T_%sRUNSTRAFER",st);
  if(a==Anim::MoveBack)
    return solveAnim("T_%sJUMPB",st);

  if(cur==Anim::Move && a==cur)
    return solveAnim("S_%sRUNL",st);
  if(cur==Anim::Move && a==Idle)
    return animSequence("T_RUNL_2_RUN");
  if(cur==Anim::Move && a==Jump)
    return animSequence("T_RUNL_2_JUMP");

  /*
  if(a==Anim::Chest)
    return animSequence("T_CHESTSMALL_STAND_2_S0");
  if(a==Anim::Sit)
    return animSequence("S_BENCH_S1");
  if(a==Anim::Lab)
    return animSequence("S_LAB_S0");
  */

  if(cur==Anim::Idle && a==Anim::Jump)
    return animSequence("T_STAND_2_JUMP");
  if(cur==Anim::Jump && a==Anim::Idle)
    return animSequence("T_JUMP_2_STAND");
  if(a==Anim::Jump)
    return animSequence("S_JUMP");

  if(cur==Anim::Idle && a==Anim::JumpUpLow)
    return animSequence("T_STAND_2_JUMPUPLOW");
  if(cur==Anim::JumpUpLow && a==Anim::Idle)
    return animSequence("T_JUMPUPLOW_2_STAND");
  if(a==Anim::JumpUpLow)
    return animSequence("S_JUMPUPLOW");

  if(cur==Anim::Idle && a==Anim::JumpUpMid)
    return animSequence("T_STAND_2_JUMPUPMID");
  if(cur==Anim::JumpUpMid && a==Anim::Idle)
    return animSequence("T_JUMPUPMID_2_STAND");
  if(a==Anim::JumpUpMid)
    return animSequence("S_JUMPUPMID");

  if(cur==Anim::Idle && a==Anim::JumpUp)
    return animSequence("T_STAND_2_JUMPUP");
  if(a==Anim::JumpUp)
    return animSequence("S_JUMPUP");

  if(cur==Anim::Idle && a==Anim::GuardL)
    return animSequence("T_STAND_2_LGUARD");
  if(a==Anim::GuardL)
    return animSequence("S_LGUARD");

  if(cur==Anim::Idle && a==Anim::GuardH)
    return animSequence("T_STAND_2_HGUARD");
  if(a==Anim::GuardH)
    return animSequence("S_HGUARD");

  if(cur==Anim::Idle && a==Anim::Talk)
    return animSequence("T_STAND_2_TALK");
  if(a==Anim::Talk)
    return animSequence("S_TALK");

  if(cur==Anim::Idle && a==Anim::Eat)
    return animSequence("T_STAND_2_EAT");
  if(cur==Anim::Eat && a==Anim::Idle)
    return animSequence("T_EAT_2_STAND");
  if(a==Anim::Eat)
    return animSequence("S_EAT");

  if(a==Anim::Perception)
    return animSequence("T_PERCEPTION");
  if(a==Anim::Lookaround)
    return animSequence("T_HGUARD_LOOKAROUND");

  if(a==Anim::Fall)
    return animSequence("S_FALLDN");
  //if(cur==Fall && a==Anim::FallDeep)
  //  return animSequence("T_FALL_2_FALLEN");
  if(a==Anim::FallDeep)
    return animSequence("S_FALL");
  if(a==Anim::SlideA)
    return animSequence("S_SLIDE");
  if(a==Anim::SlideB)
    return animSequence("S_SLIDEB");

  // FALLBACK
  if(a==Anim::Move)
    return solveAnim("S_%sRUNL",st);
  if(a==Anim::Idle)
    return solveAnim("S_%sRUN",st);
  return nullptr;
  }

const Animation::Sequence* Npc::solveAnim(const char *format, Npc::WeaponState st) const {
  static const char* weapon[] = {
    "",
    "FIST",
    "1H",
    "2H",
    "BOW",
    "CBOW"
    };
  char name[128]={};
  std::snprintf(name,sizeof(name),format,weapon[st]);
  if(auto ret=animSequence(name))
    return ret;
  std::snprintf(name,sizeof(name),format,"");
  if(auto ret=animSequence(name))
    return ret;
  std::snprintf(name,sizeof(name),format,"FIST");
  return animSequence(name);
  }

const Animation::Sequence *Npc::animSequence(const char *name) const {
  for(size_t i=overlay.size();i>0;){
    --i;
    if(auto s = overlay[i].sk->sequence(name))
      return s;
    }
  return skeleton ? skeleton->sequence(name) : nullptr;
  }

Npc::Anim Npc::animByName(const std::string &name) const {
  if(name=="T_STAND_2_LGUARD" || name=="S_LGUARD")
    return Anim::GuardL;
  if(name=="T_STAND_2_HGUARD")
    return Anim::GuardH;
  if(name=="T_LGUARD_2_STAND" || name=="T_HGUARD_2_STAND")
    return Anim::Idle;
  if(name=="T_STAND_2_TALK" || name=="S_TALK")
    return Anim::Talk;
  if(name=="T_PERCEPTION")
    return Anim::Perception;
  if(name=="T_HGUARD_LOOKAROUND")
    return Anim::Lookaround;
  if(name=="T_STAND_2_EAT" || name=="T_EAT_2_STAND" || name=="S_EAT")
    return Anim::Eat;
  return Anim::NoAnim;
  }

const Npc::Routine& Npc::currentRoutine() const {
  auto time = owner.world().time();
  for(auto& i:routines){
    if(i.start<=time && time<i.end)
      return i;
    }

  static Routine r;
  return r;
  }

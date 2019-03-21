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

bool Npc::setPosition(const std::array<float,3> &pos) {
  if(x==pos[0] && y==pos[1] && z==pos[2])
    return false;
  x = pos[0];
  y = pos[1];
  z = pos[2];

  Matrix4x4& mt=animation.pos;
  mt.set(3,0,x);
  mt.set(3,1,y);
  mt.set(3,2,z);
  setPos(mt);
  return true;
  }

void Npc::setDirection(float x, float /*y*/, float z) {
  float a=angleDir(x,z);
  setDirection(a);
  }

void Npc::setDirection(const std::array<float,3> &pos) {
  setDirection(pos[0],pos[1],pos[2]);
  }

void Npc::setDirection(float rotation) {
  if(std::fabs(angle-rotation)<1.f)
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

void Npc::resetPositionToTA() {
  if(routines.size()==0)
    return;

  attachToPoint(nullptr);
  auto& rot = currentRoutine();
  auto  at  = rot.point;

  if(at==nullptr) {
    auto    time = owner.world().time();
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
      return;
    }

  if(at->isLocked()){
    auto p = owner.world().findNextPoint(*at);
    if(p!=nullptr)
      at=p;
    }
  setPosition (at->x, at->y, at->z);
  setDirection(at->dirX,at->dirY,at->dirZ);
  attachToPoint(at);
  }

void Npc::clearSpeed() {
  mvAlgo.clearSpeed();
  }

void Npc::setAiType(Npc::AiType t) {
  aiType=t;
  }

void Npc::setWalkMode(WalkBit m) {
  wlkMode = m;
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

bool Npc::checkHealth(bool onChange) {
  if(onChange && lastHit!=nullptr) {
    float da = rotationRad()-lastHit->rotationRad();
    if(std::cos(da)>=0)
      lastHitType='A'; else
      lastHitType='B';
    }

  if(owner.isDead(*this)) {
    setAnim(lastHitType=='A' ? Anim::DeadA : Anim::DeadB);
    return false;
    }
  if(owner.isUnconscious(*this)) {
    setAnim(lastHitType=='A' ? Anim::UnconsciousA : Anim::UnconsciousB);
    return false;
    }

  auto& v = owner.vmNpc(hnpc);
  if(v.attribute[ATR_HITPOINTS]<=1) {
    if(v.attribute[ATR_HITPOINTSMAX]<=1){
      size_t fdead=owner.getSymbolIndex("ZS_Dead");
      startState(fdead,"");
      return false;
      }

    if(onChange) {
      if(owner.guildAttitude(*this,*currentOther)==WorldScript::ATT_HOSTILE){
        size_t fdead=owner.getSymbolIndex("ZS_Dead");
        startState(fdead,"");
        } else {
        size_t fdead=owner.getSymbolIndex("ZS_Unconscious");
        startState(fdead,"");
        }
      return false;
      }
    }
  return true;
  }

std::array<float,3> Npc::position() const {
  return {{x,y,z}};
  }

std::array<float,3> Npc::cameraBone() const {
  if(animation.skInst==nullptr)
    return {{}};
  auto bone=animation.skInst->cameraBone();
  std::array<float,3> r={{}};
  bone.project(r[0],r[1],r[2]);
  animation.pos.project (r[0],r[1],r[2]);
  return r;
  }

float Npc::rotation() const {
  return angle;
  }

float Npc::rotationRad() const {
  return angle*float(M_PI)/180.f;
  }

float Npc::translateY() const {
  return animation.skInst ? animation.skInst->translateY() : 0;
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
  animation.updateAnimation(owner.tickCount());
  }

const char *Npc::displayName() const {
  return owner.vmNpc(hnpc).name[0].c_str();
  }

std::array<float,3> Npc::displayPosition() const {
  float h = std::max(animation.view.bboxHeight(),animation.armour.bboxHeight());
  return {{x,y+h,z}};
  }

void Npc::setName(const std::string &n) {
  name = n;
  }

void Npc::addOverlay(const char *sk, uint64_t time) {
  auto skelet = Resources::loadSkeleton(sk);
  addOverlay(skelet,time);
  }

void Npc::addOverlay(const Skeleton* sk,uint64_t time) {
  animation.addOverlay(sk,time,owner.tickCount(),wlkMode,currentInteract,owner.world());
  }

void Npc::delOverlay(const char *sk) {
  animation.delOverlay(sk);
  }

void Npc::delOverlay(const Skeleton *sk) {
  animation.delOverlay(sk);
  }

ZMath::float3 Npc::animMoveSpeed(uint64_t dt) const {
  if(animation.animSq!=nullptr){
    if(animation.animSq.l0)
      return animation.animSq.l0->speed(owner.tickCount()-animation.sAnim,dt);
    return animation.animSq.l1->speed(owner.tickCount()-animation.sAnim,dt);
    }
  return ZMath::float3(0,0,0);
  }

void Npc::setVisual(const Skeleton* v) {
  animation.setVisual(v,owner.tickCount(),invent.weaponState(),wlkMode,currentInteract,owner.world());
  }

void Npc::setVisualBody(StaticObjects::Mesh&& h, StaticObjects::Mesh &&body, int32_t bodyVer, int32_t bodyColor) {
  animation.setVisualBody(std::move(h),std::move(body));

  vColor  = bodyVer;
  bdColor = bodyColor;

  invent.updateArmourView(owner,*this);
  updatePos(); // update obj matrix
  }

void Npc::setArmour(StaticObjects::Mesh &&a) {
  animation.armour = std::move(a);
  animation.armour.setSkeleton(animation.skeleton);
  setPos(animation.pos);
  }

void Npc::setSword(StaticObjects::Mesh &&s) {
  auto st=weaponState();
  animation.sword = std::move(s);
  updateWeaponSkeleton();
  setAnim(animation.current,weaponState(),st);
  setPos(animation.pos);
  }

void Npc::setRangeWeapon(StaticObjects::Mesh &&b) {
  auto st=weaponState();
  animation.bow = std::move(b);
  updateWeaponSkeleton();
  setAnim(animation.current,weaponState(),st);
  setPos(animation.pos);
  }

void Npc::updateWeaponSkeleton() {
  auto st = weaponState();
  if(st==WeaponState::W1H || st==WeaponState::W2H){
    animation.sword.setSkeleton(animation.skeleton,"ZS_RIGHTHAND");
    } else {
    auto weapon   = invent.currentMeleWeapon();
    bool twoHands = weapon!=nullptr && weapon->is2H();
    animation.sword.setSkeleton(animation.skeleton,twoHands ? "ZS_LONGSWORD" : "ZS_SWORD");
    }

  if(st==WeaponState::Bow || st==WeaponState::CBow){
    animation.bow.setSkeleton(animation.skeleton,"ZS_LEFTHAND");
    } else {
    auto range = invent.currentRangeWeapon();
    bool cbow  = range!=nullptr && range->isCrossbow();
    animation.bow.setSkeleton(animation.skeleton,cbow ? "ZS_CROSSBOW" : "ZS_BOW");
    }
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

bool Npc::setAnim(Npc::Anim a) {
  auto weaponSt=invent.weaponState();
  return setAnim(a,weaponSt,weaponSt);
  }

void Npc::stopAnim(const std::string &ani) {
  if(animation.stopAnim(ani))
    setAnim(animation.lastIdle);
  }

bool Npc::isStanding() const {
  return animation.current<Anim::IdleLast || animation.current==Anim::Interact;
  }

bool Npc::isFlyAnim() const {
  return animation.isFlyAnim(owner.tickCount());
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
  return refuseTalkMilis>=owner.tickCount();
  }

int32_t Npc::mageCycle() const {
  return talentValue(TALENT_RUNES);
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
  if(a>=ATR_MAX || val==0)
    return;

  auto& v    = owner.vmNpc(hnpc);
  v.attribute[a]+=val;
  if(v.attribute[a]<0)
    v.attribute[a]=0;
  if(a==ATR_HITPOINTS && v.attribute[a]>v.attribute[ATR_HITPOINTSMAX])
    v.attribute[a] = v.attribute[ATR_HITPOINTSMAX];
  if(a==ATR_MANA && v.attribute[a]>v.attribute[ATR_MANAMAX])
    v.attribute[a] = v.attribute[ATR_MANAMAX];

  if(val<0)
    invent.invalidateCond(*this);

  if(a==ATR_HITPOINTS){
    checkHealth(true);
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
  uint32_t ret = uint32_t(owner.vmNpc(hnpc).guild);
  return ret;
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
    currentLookAt=nullptr;
    return false;
    }
  return false;
  }

bool Npc::implLookAt(float dx, float dz, uint64_t dt) {
  float       a    = angleDir(dx,dz);
  float       da   = int(a-angle)%360;
  auto        gl   = std::min<uint32_t>(guild(),GIL_MAX);
  float       step = owner.guildVal().turn_speed[gl]*(dt/1000.f);

  if(std::abs(da)<step){
    setDirection(a);
    if(animation.current==AnimationSolver::RotL || animation.current==AnimationSolver::RotR) {
      if(currentGoTo==nullptr && animation.animSq!=nullptr && !animation.animSq.isFinished(owner.tickCount()-animation.sAnim)){
        // finish animation
        return setAnim(animation.lastIdle);
        }
      if(currentGoTo==nullptr)
        setAnim(animation.lastIdle);
      return false;
      }
    return false;
    }

  const auto sgn = std::sin(double(da)*M_PI/180.0);
  if(sgn<0) {
    if(setAnim(Anim::RotR))
      setDirection(angle-step);
    } else {
    if(setAnim(Anim::RotL))
      setDirection(angle+step);
    }
  return true;
  }

bool Npc::implGoTo(uint64_t dt) {
  if((!currentGoTo && !currentGoToNpc) || currentTarget!=nullptr)
    return false;

  if(currentGoTo){
    float dx = currentGoTo->x-x;
    //float dy = y-currentGoTo->position.y;
    float dz = currentGoTo->z-z;

    if(!MoveAlgo::isClose(x,y,z,*currentGoTo)){
      if(implLookAt(dx,dz,dt))
        return true;
      }
    if(!mvAlgo.aiGoTo(currentGoTo)) {
      attachToPoint(currentGoTo);
      currentGoTo   = wayPath.pop();
      currentFpLock = FpLock(currentGoTo);
      if(isStanding())
        setAnim(AnimationSolver::Idle);
      }
    return currentGoTo || mvAlgo.hasGoTo();
    } else {
    float dx = currentGoToNpc->x-x;
    //float dy = y-currentGoTo->position.y;
    float dz = currentGoToNpc->z-z;

    if(implLookAt(dx,dz,dt))
      return true;
    if(!mvAlgo.aiGoTo(currentGoToNpc,400)) {
      currentGoToNpc=nullptr;
      if(isStanding())
        setAnim(AnimationSolver::Idle);
      }
    return mvAlgo.hasGoTo();
    }
  }

bool Npc::implAtack(uint64_t dt) {
  if(currentTarget==nullptr || isPlayer())
    return false;
  float dx = currentTarget->x-x;
  float dz = currentTarget->z-z;

  if(implLookAt(dx,dz,dt))
    return true;

  if(owner.isDead(*currentTarget)) {
    currentTarget=nullptr;
    return false;
    }

  FightAlgo::Action act=FightAlgo::MV_MOVE;
  if(currentTarget!=nullptr)
    act=fghAlgo.tick(*this,*currentTarget,owner,dt);

  if(act==FightAlgo::MV_ATACK) {
    doAttack(Anim::Atack);
    return true;
    }

  if(!mvAlgo.aiGoTo(currentTarget,fghAlgo.prefferedAtackDistance(*this,*currentTarget,owner))) {
    setAnim(AnimationSolver::Idle);
    } else {
    setAnim(AnimationSolver::Move);
    }
  return true;
  }

void Npc::tick(uint64_t dt) {
  if(!checkHealth(false)){
    mvAlgo.aiGoTo(nullptr);
    currentOther = lastHit;
    mvAlgo.tick(dt);
    aiActions.clear();
    tickRoutine(); // tick for ZS_Death
    return;
    }

  mvAlgo.tick(dt);

  if(!implAtack(dt)) {
    if(implLookAt(dt))
      return;

    if(implGoTo(dt))
      return;
    }

  if(interactive()!=nullptr)
    setAnim(AnimationSolver::Interact); else
  if(currentGoTo==nullptr && currentGoToNpc==nullptr && currentTarget==nullptr && aiType!=AiType::Player) {
    if(weaponState()==WeaponState::NoWeapon)
      setAnim(animation.lastIdle); else
    if(animation.current>Anim::IdleLoopLast)
      setAnim(Anim::Idle);
    }

  if(waitTime>=owner.tickCount())
    return;

  if(aiActions.size()==0) {
    tickRoutine();
    return;
    }

  nextAiAction(dt);
  }

void Npc::nextAiAction(uint64_t dt) {
  if(aiActions.size()==0)
    return;
  auto act = std::move(aiActions.front());
  aiActions.pop_front();

  switch(act.act) {
    case AI_None: break;
    case AI_LookAt:
      currentLookAt=act.target;
      break;
    case AI_TurnToNpc:
      currentTurnTo =act.target;
      break;
    case AI_GoToNpc:
      currentGoTo     = nullptr;
      currentGoToNpc  = act.target;
      currentFp       = nullptr;
      currentFpLock   = FpLock();
      currentGoToFlag = GoToHint::GT_Default;
      wayPath.clear();
      break;
    case AI_GoToNextFp: {
      auto fp = owner.world().findNextFreePoint(*this,act.s0.c_str());
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
      // TODO: check distance
      wayPath         = owner.world().wayTo(*this,*act.point);
      currentGoTo     = wayPath.pop();
      currentFpLock   = FpLock(currentGoTo);
      currentGoToNpc  = nullptr;
      currentFp       = nullptr;
      currentGoToFlag = GoToHint::GT_Default;
      break;
      }
    case AI_StopLookAt:
      currentLookAt=nullptr;
      break;
    case AI_RemoveWeapon:
      if(!closeWeapon())
        aiActions.push_front(std::move(act));
      break;
    case AI_StartState:
      startState(act.func,act.s0,gtime(),act.i0==0);
      break;
    case AI_PlayAnim:{
      auto tag = animation.animByName(act.s0);
      if(tag!=Anim::NoAnim){
        if(!setAnim(tag))
          aiActions.push_front(std::move(act));
        } else {
        auto a = animation.animSequence(act.s0.c_str());
        if(a!=nullptr) {
          Log::d("AI_PlayAnim: unrecognized anim: \"",act.s0,"\"");
          if(animation.animSq!=a)
            animation.invalidateAnim(a,animation.skeleton,owner.world(),owner.tickCount());
          }
        }
      break;
      }
    case AI_PlayAnimById:{
      auto tag = Anim(act.i0);
      if(!setAnim(tag)) {
        aiActions.push_front(std::move(act));
        } else {
        if(animation.animSq)
          waitTime = owner.tickCount()+uint64_t(animation.animSq.totalTime());
        }
      break;
      }
    case AI_Wait:
      waitTime = owner.tickCount()+uint64_t(act.i0);
      break;
    case AI_StandUp:
      setInteraction(nullptr);
      if(animation.current==Anim::Sit)
        setAnim(Anim::Idle);
      break;
    case AI_StandUpQuick:
      setInteraction(nullptr);
      break;
    case AI_EquipArmor:
      invent.equipArmour(act.i0,owner,*this);
      break;
    case AI_EquipMelee:
      invent.equipBestMeleWeapon(owner,*this);
      break;
    case AI_EquipRange:
      invent.equipBestRangeWeapon(owner,*this);
      break;
    case AI_UseMob:
      if(!owner.isTalk(*this)){
        if(act.i0<0){
          setInteraction(nullptr);
          } else {
          owner.aiUseMob(*this,act.s0);
          }
        }
      break;
    case AI_UseItem:
      if(act.i0!=0)
        useItem(size_t(act.i0));
      break;
    case AI_Teleport: {
      setPosition(act.point->x,act.point->y,act.point->z);
      }
      break;
    case AI_DrawWeaponMele:
      if(!drawWeaponMele())
        aiActions.push_front(std::move(act));
      break;
    case AI_DrawWeaponRange:
      drawWeaponBow();
      break;
    case AI_DrawSpell:
      drawSpell(act.i0);
      break;
    case AI_Atack:
      atackMode=true;
      break;
    case AI_Flee:
      atackMode=false;
      break;
    case AI_Dodge:
      setAnim(Anim::MoveBack);
      break;
    case AI_UnEquipWeapons:
      invent.unequipWeapons(owner,*this);
      break;
    case AI_Output:
      if(!owner.aiOutput(*this,*act.target,act.s0))
        aiActions.push_front(std::move(act));
      break;
    case AI_OutputSvm:
      Log::d("TODO: ai_outputsvm: ",act.s0);
      break;
    case AI_OutputSvmOverlay:
      Log::d("TODO: ai_outputsvm_overlay: ",act.s0);
      break;
    case AI_StopProcessInfo:
      if(!owner.aiClose()) {
        aiActions.push_front(std::move(act));
        }
      break;
    case AI_ContinueRoutine:{
      auto& r = currentRoutine();
      auto  t = endTime(r);
      startState(r.callback,"",t,false);
      break;
      }
    case AI_AlignToWp:
    case AI_AlignToFp:{
      if(auto fp = currentFp){
        if(fp->dirX!=0 || fp->dirZ!=0){
          if(implLookAt(fp->dirX,fp->dirZ,dt))
            aiActions.push_front(std::move(act));
          }
        }
      break;
      }
    }
  }

bool Npc::startState(size_t id,const std::string &wp) {
  return startState(id,wp,gtime(),false);
  }

bool Npc::startState(size_t id,const std::string &wp, gtime endTime,bool noFinalize) {
  if(id==0)
    return false;
  if(aiState.funcIni==id)
    return false;

  if(aiState.funcIni!=0 && aiState.started){
    owner.invokeState(this,currentOther,nullptr,aiState.funcLoop); // avoid ZS_Talk bug
    if(!noFinalize)
      owner.invokeState(this,currentOther,nullptr,aiState.funcEnd);  // cleanup
    }

  if(!wp.empty()){
    auto& v=owner.vmNpc(hnpc);
    v.wp = wp;
    }

  if(aiState.funcIni!=0)
    prevAiState = aiState.funcIni;

  auto& st = owner.getAiState(id);
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

void Npc::clearState() {
  if(aiState.funcIni!=0 && aiState.started){
    owner.invokeState(this,currentOther,nullptr,aiState.funcLoop); // avoid ZS_Talk bug
    owner.invokeState(this,currentOther,nullptr,aiState.funcEnd);  // cleanup
    }
  aiState = AiState();
  aiState.funcIni = 0;
  }

void Npc::tickRoutine() {
  if(aiState.funcIni==0) {
    auto& v = owner.vmNpc(hnpc);
    auto  r = currentRoutine();
    if(r.callback!=0) {
      if(r.point!=nullptr)
        v.wp = r.point->name;
      auto t = endTime(r);
      startState(r.callback,"",t,false);
      }
    else if(v.start_aistate!=0)
      startState(v.start_aistate,"");
    }

  if(aiState.funcIni==0)
    return;

  if(aiState.started) {
    if(aiState.loopNextTime<=owner.tickCount()){
      aiState.loopNextTime+=1000; // one tick per second?
      int loop = owner.invokeState(this,currentOther,nullptr,aiState.funcLoop);
      if(aiState.eTime!=gtime() && aiState.eTime<=owner.world().time())
        loop=1;
      if(loop!=0){
        owner.invokeState(this,currentOther,nullptr,aiState.funcEnd);
        if(atackMode) {
          atackMode=false;
          setTarget(nullptr);
          }
        prevAiState = aiState.funcIni;
        aiState     = AiState();
        }
      }
    } else {
    aiState.started=true;
    owner.invokeState(this,currentOther,nullptr,aiState.funcIni);
    }
  }

void Npc::setTarget(Npc *t) {
  currentTarget=t;
  mvAlgo.aiGoTo(nullptr);
  }

Npc *Npc::target() {
  return currentTarget;
  }

bool Npc::haveOutput() const {
  for(auto& i:aiActions)
    if(i.act==AI_Output)
      return true;
  return false;
  }

void Npc::doAttack(Anim anim) {
  auto weaponSt=invent.weaponState();
  auto weapon  =invent.activeWeapon();

  if(weaponSt==WeaponState::NoWeapon)
    return;

  if(weaponSt==WeaponState::Mage && weapon!=nullptr)
    anim=Anim(owner.spellCastAnim(*this,*weapon));

  if(!currentTarget || !fghAlgo.isInAtackRange(*this,*currentTarget,owner)){
    setAnim(anim,weaponSt,weaponSt);
    return;
    }

  if(animation.current==anim){
    setAnim(Anim::Idle,weaponSt,weaponSt);
    }
  else if(setAnim(anim,weaponSt,weaponSt)){
    currentTarget->currentOther=this;
    currentTarget->perceptionProcess(*this,currentTarget,0,PERC_ASSESSDAMAGE);

    auto ani=currentTarget->anim();
    if(ani!=Anim::MoveBack && ani!=Anim::AtackBlock) {
      currentTarget->lastHit = this;
      if(!currentTarget->isPlayer())
        currentTarget->currentOther=currentTarget->lastHit;
      currentTarget->changeAttribute(ATR_HITPOINTS,currentTarget->isPlayer() ? 1 : -100);

      if(ani==Anim::Move  || ani==Anim::MoveL  || ani==Anim::MoveR ||
         ani==Anim::Atack || ani==Anim::AtackL || ani==Anim::AtackR ||
         ani<Anim::IdleLast || (Anim::MagFirst<=ani && ani<=Anim::MagLast )) {
        currentTarget->animation.resetAni();
        }
      if(currentTarget->lastHitType=='A')
        currentTarget->setAnim(Anim::StumbleA); else
        currentTarget->setAnim(Anim::StumbleB);
      }
    }
  }

const Npc::Routine& Npc::currentRoutine() const {
  auto time = owner.world().time();
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
  auto wtime = owner.world().time();
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

Npc::BodyState Npc::bodyState() const {
  uint32_t s = bodySt;
  if(owner.isDead(*this))
    s = BS_DEAD;
  else if(owner.isUnconscious(*this))
    s = BS_UNCONSCIOUS;
  else if(anim()==Anim::Move || anim()==Anim::MoveL || anim()==Anim::MoveR || anim()==Anim::MoveBack)
    s = BS_RUN;
  else if(anim()==Anim::Fall || anim()==Anim::FallDeep)
    s = BS_FALL;
  else if(anim()==Anim::Sleep)
    s = BS_LIE;
  else if(anim()==Anim::Sit || anim()==Anim::GuardSleep)
    s = BS_SIT;
  if(auto i = interactive())
    s = i->stateMask(s);
  return BodyState(s);
  }

void Npc::setToFightMode(const uint32_t item) {
  if(invent.itemCount(item)==0)
    addItem(item,1);

  invent.equip(item,owner,*this,true);
  drawWeaponMele();
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

void Npc::addItem(uint32_t id, Npc &from) {
  Inventory::trasfer(invent,from.invent,&from,id,1,owner);
  }

void Npc::moveItem(uint32_t id, Interactive &to) {
  Inventory::trasfer(to.inventory(),invent,this,id,1,owner);
  }

void Npc::sellItem(uint32_t id, Npc &to) {
  int32_t price = invent.sellPriceOf(id);
  Inventory::trasfer(to.invent,invent,this,id,1,owner);
  invent.addItem(owner.goldId(),uint32_t(price),owner);
  }

void Npc::buyItem(uint32_t id, Npc &from) {
  if(id==owner.goldId())
    return;
  int32_t price = from.invent.priceOf(id);
  if(price>int32_t(invent.goldCount())) {
    owner.printCannotBuyError(*this);
    return;
    }
  Inventory::trasfer(invent,from.invent,nullptr,id,1,owner);
  invent.delItem(owner.goldId(),uint32_t(price),owner,*this);
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

bool Npc::lookAt(float dx, float dz, uint64_t dt) {
  return implLookAt(dx,dz,dt);
  }

bool Npc::checkGoToNpcdistance(const Npc &other) {
  if(atackMode)
    return fghAlgo.isInAtackRange(*this,other,owner);
  return qDistTo(other)<=200*200;
  }

size_t Npc::hasItem(uint32_t id) const {
  return invent.itemCount(id);
  }

void Npc::delItem(uint32_t item, uint32_t amount) {
  invent.delItem(item,amount,owner,*this);
  }

void Npc::useItem(uint32_t item,bool force) {
  invent.use(item,owner,*this,force);
  }

void Npc::unequipItem(uint32_t item) {
  invent.unequip(item,owner,*this);
  }

bool Npc::closeWeapon() {
  auto weaponSt=invent.weaponState();
  if(weaponSt==WeaponState::NoWeapon)
    return true;
  if(!setAnim(animation.current,WeaponState::NoWeapon,weaponSt))
    return false;
  invent.switchActiveWeapon(Item::NSLOT);
  updateWeaponSkeleton();
  owner.vmNpc(hnpc).weapon = 0;
  return true;
  }

bool Npc::drawWeaponFist() {
  auto weaponSt=invent.weaponState();
  if(weaponSt==WeaponState::Fist)
    return true;
  if(weaponSt!=WeaponState::NoWeapon) {
    closeWeapon();
    return false;
    }
  Anim ani = animation.current==Anim::Idle ? Anim::Idle : Anim::Move;
  if(!setAnim(ani,WeaponState::Fist,weaponSt))
    return false;
  invent.switchActiveWeaponFist();
  updateWeaponSkeleton();
  owner.vmNpc(hnpc).weapon = 1;
  return true;
  }

bool Npc::drawWeaponMele() {
  auto weaponSt=invent.weaponState();
  if(weaponSt==WeaponState::Fist || weaponSt==WeaponState::W1H || weaponSt==WeaponState::W2H)
    return true;
  if(invent.currentMeleWeapon()==nullptr)
    return drawWeaponFist();
  if(weaponSt!=WeaponState::NoWeapon) {
    closeWeapon();
    return false;
    }
  auto st  = invent.currentMeleWeapon()->is2H() ? WeaponState::W2H : WeaponState::W1H;
  Anim ani = animation.current==isStanding()    ? Anim::Idle       : Anim::Move;
  if(!setAnim(ani,st,weaponSt))
    return false;
  invent.switchActiveWeapon(1);
  updateWeaponSkeleton();
  owner.vmNpc(hnpc).weapon = (st==WeaponState::W1H ? 3:4);
  return true;
  }

bool Npc::drawWeaponBow() {
  auto weaponSt=invent.weaponState();
  if(weaponSt==WeaponState::Bow || weaponSt==WeaponState::CBow)
    return true;
  if(weaponSt!=WeaponState::NoWeapon) {
    closeWeapon();
    return false;
    }
  auto st  = invent.currentRangeWeapon()->isCrossbow() ? WeaponState::CBow : WeaponState::Bow;
  Anim ani = animation.current==isStanding()           ? Anim::Idle        : Anim::Move;
  if(!setAnim(ani,st,weaponSt))
    return false;
  invent.switchActiveWeapon(2);
  updateWeaponSkeleton();
  owner.vmNpc(hnpc).weapon = (st==WeaponState::W1H ? 5:6);
  return true;
  }

bool Npc::drawMage(uint8_t slot) {
  auto weaponSt=invent.weaponState();
  if(weaponSt!=WeaponState::NoWeapon && weaponSt!=WeaponState::Mage) {
    closeWeapon();
    return false;
    }
  Anim ani = animation.current==isStanding() ? Anim::Idle : Anim::Move;
  if(!setAnim(ani,WeaponState::Mage,weaponSt))
    return false;
  invent.switchActiveWeapon(slot);
  updateWeaponSkeleton();
  owner.vmNpc(hnpc).weapon = 7;
  return true;
  }

void Npc::drawSpell(int32_t spell) {
  auto weaponSt=invent.weaponState();
  invent.switchActiveSpell(spell,owner,*this);
  setAnim(animation.current,invent.weaponState(),weaponSt);
  updateWeaponSkeleton();
  }

void Npc::fistShoot() {
  doAttack(Anim::Atack);
  }

void Npc::blockFist() {
  auto weaponSt=invent.weaponState();
  if(weaponSt!=WeaponState::Fist)
    return;
  setAnim(Anim::AtackBlock,weaponSt,weaponSt);
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
  auto weaponSt=invent.weaponState();
  setAnim(AnimationSolver::AtackBlock,weaponSt,weaponSt);
  }

bool Npc::castSpell() {
  auto active=invent.activeWeapon();
  if(active==nullptr ||
     (Anim::MagFirst<=animation.current && animation.current<=Anim::MagLast) ||
     (Anim::MagFirst<=animation.prevAni && animation.prevAni<=Anim::MagLast) || !isStanding())
    return false;

  const SpellCode code = SpellCode(owner.invokeMana(*this,*active));
  switch(code) {
    case SpellCode::SPL_SENDSTOP:
      setAnim(Anim::MagNoMana,WeaponState::Mage,invent.weaponState());
      break;
    case SpellCode::SPL_NEXTLEVEL:{
      auto ani = Npc::Anim(owner.spellCastAnim(*this,*active));
      setAnim(ani,WeaponState::Mage,WeaponState::Mage);
      }
      break;
    case SpellCode::SPL_SENDCAST: {
      auto ani = Npc::Anim(owner.spellCastAnim(*this,*active));
      //setAnim(ani,WeaponState::Mage,WeaponState::Mage);
      AiAction a;
      a.act  = AI_PlayAnimById;
      a.i0   = ani;
      aiActions.push_back(a);
      owner.invokeSpell(*this,*active);
      break;
      }
    default:
      //TODO
      break;
    }
  return true;
  }

bool Npc::isEnemy(const Npc &other) const {
  return owner.guildAttitude(*this,other)==WorldScript::ATT_HOSTILE;
  }

bool Npc::isDead() const {
  return owner.isDead(*this);
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
    perception[t].func = 0;
  }

void Npc::startDialog(Npc& pl) {
  perceptionProcess(pl,nullptr,0,PERC_ASSESSTALK);
  }

bool Npc::perceptionProcess(Npc &pl,float quadDist) {
  static bool disable=false;
  if(disable)
    return false;

  float r = owner.vmNpc(hnpc).senses_range;
  r = r*r;
  if(quadDist>r)
    return false;

  bool ret=false;
  if(perceptionProcess(pl,nullptr,quadDist,PERC_ASSESSPLAYER))
    ret = true;
  if(isEnemy(pl) && perceptionProcess(pl,nullptr,quadDist,PERC_ASSESSENEMY))
    ret = true;
  return ret;
  }

bool Npc::perceptionProcess(Npc &pl, Npc* victum, float quadDist, Npc::PercType perc) {
  float r = owner.vmNpc(hnpc).senses_range;
  r = r*r;
  if(quadDist<r && perception[perc].func){
    currentOther=&pl;
    owner.invokeState(this,currentOther,victum,perception[perc].func);
    return true;
    }
  perceptionNextTime=owner.tickCount()+perceptionTime;
  return false;
  }

uint64_t Npc::percNextTime() const {
  return perceptionNextTime;
  }

bool Npc::setInteraction(Interactive *id) {
  if(currentInteract==id)
    return false;
  if(currentInteract)
    currentInteract->dettach(*this);
  currentInteract=nullptr;

  if(id && id->attach(*this)) {
    currentInteract=id;
    auto st = currentInteract->stateFunc();
    if(!st.empty()) {
      owner.useInteractive(hnpc,st);
      }
    if(auto tr = currentInteract->triggerTarget()){
      tr->onTrigger();
      }
    return true;
    }

  return false;
  }

bool Npc::isState(uint32_t stateFn) const {
  return aiState.funcIni==stateFn;
  }

bool Npc::wasInState(uint32_t stateFn) const {
  return prevAiState==stateFn;
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
  routines.clear();
  owner.invokeState(this,currentOther,nullptr,callback);
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

Npc::MoveCode Npc::testMoveVr(const std::array<float,3> &pos, std::array<float,3> &fallback, float speed) {
  if(physic.testMove(pos,fallback,speed))
    return MV_OK;

  std::array<float,3> p=fallback;
  for(int i=1;i<5;++i){
    if(physic.testMove(p,fallback,speed))
      return MV_CORRECT;
    p=fallback;
    }
  return MV_FAILED;
  }

bool Npc::tryMove(const std::array<float,3> &pos, std::array<float,3> &fallback, float speed) {
  if(pos==position())
    return true;
  if(physic.tryMove(pos,fallback,speed)) {
    return setPosition(fallback);
    }

  std::array<float,3> p=fallback;
  for(int i=1;i<5;++i){
    if(physic.tryMove(p,fallback,speed)) {
      return setPosition(fallback);
      }
    p=fallback;
    }
  return false;
  }

bool Npc::tryMove(const std::array<float,3> &pos, float speed) {
  std::array<float,3> fallback=pos;
  return tryMove(pos,fallback,speed);
  }

Npc::JumpCode Npc::tryJump(const std::array<float,3> &p0) {
  float len = 20.f;
  float rot = rotationRad();
  float s   = std::sin(rot), c = std::cos(rot);
  float dx  = len*s, dz = -len*c;

  auto pos = p0;
  pos[0]+=dx;
  pos[2]+=dz;

  if(physic.testMove(pos))
    return JM_OK;

  pos[1] = p0[1]+clampHeight(Anim::JumpUpLow);
  if(physic.testMove(pos))
    return JM_UpLow;

  pos[1] = p0[1]+clampHeight(Anim::JumpUpMid);
  if(physic.testMove(pos))
    return JM_UpMid;

  return JM_Up;
  }

float Npc::clampHeight(Npc::Anim a) const {
  switch(a) {
    case AnimationSolver::JumpUpLow:
      return 60;
    case AnimationSolver::JumpUpMid:
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

void Npc::aiStartState(uint32_t stateFn, int behavior, std::string wp) {
  auto& st = owner.getAiState(stateFn);(void)st;

  AiAction a;
  a.act  = AI_StartState;
  a.func = stateFn;
  a.i0   = behavior;
  a.s0   = std::move(wp);
  aiActions.push_back(a);
  }

void Npc::aiPlayAnim(std::string ani) {
  AiAction a;
  a.act  = AI_PlayAnim;
  a.s0   = std::move(ani);
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

void Npc::aiOutput(Npc& to,std::string text) {
  AiAction a;
  a.act    = AI_Output;
  a.s0     = std::move(text);
  a.target = &to;
  aiActions.push_back(a);
  }

void Npc::aiOutputSvm(Npc &to, std::string text) {
  AiAction a;
  a.act    = AI_OutputSvm;
  a.s0     = std::move(text);
  a.target = &to;
  aiActions.push_back(a);
  }

void Npc::aiOutputSvmOverlay(Npc &to, std::string text) {
  AiAction a;
  a.act    = AI_OutputSvmOverlay;
  a.s0     = std::move(text);
  a.target = &to;
  aiActions.push_back(a);
  }

void Npc::aiStopProcessInfo() {
  AiAction a;
  a.act = AI_StopProcessInfo;
  aiActions.push_back(a);
  }

void Npc::aiClearQueue() {
  aiActions.clear();
  currentGoTo     = nullptr;
  currentFpLock   = FpLock();
  currentGoToFlag = GoToHint::GT_Default;
  mvAlgo.aiGoTo(nullptr);
  //setTarget(nullptr);
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

void Npc::attachToPoint(const WayPoint *p) {
  currentFp     = p;
  currentFpLock = FpLock(currentFp);
  if(p!=nullptr)
    owner.vmNpc(hnpc).wp = p->name;
  }

void Npc::clearGoTo() {
  currentGoTo     = nullptr;
  currentGoToNpc  = nullptr;
  currentFpLock   = FpLock();
  currentGoToFlag = GoToHint::GT_Default;
  wayPath.clear();
  mvAlgo.aiGoTo(nullptr);
  }

bool Npc::canSeeNpc(const Npc &oth, bool freeLos) const {
  return canSeeNpc(oth.x,oth.y+180,oth.z,freeLos);
  }

bool Npc::canSeeNpc(float tx, float ty, float tz, bool freeLos) const {
  DynamicWorld* w = owner.world().physic();

  if(!freeLos){
    float dx  = x-tx, dz=z-tz;
    float dir = angleDir(dx,dz);
    float da  = float(M_PI)*(angle-dir)/180.f;
    if(double(std::cos(da))<std::cos(M_PI/3))
      return false;
    }
  bool ret=true;
  // TODO: npc eyesight height
  w->ray(x,y+180,z, tx,ty,tz, ret);
  return !ret;
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
  animation.setPos(m);
  physic.setPosition(x,y,z);
  }

bool Npc::setAnim(Npc::Anim a, WeaponState st0, WeaponState st) {
  return animation.setAnim(a,owner.tickCount(),st0,st,wlkMode,currentInteract,owner.world());
  }

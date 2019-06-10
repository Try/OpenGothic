#include "npc.h"

#include <Tempest/Matrix4x4>
#include <zenload/zCMaterial.h>

#include "interactive.h"
#include "graphics/skeleton.h"
#include "graphics/posepool.h"
#include "game/serialize.h"
#include "worldscript.h"
#include "trigger.h"
#include "world.h"
#include "resources.h"

using namespace Tempest;

Npc::Npc(WorldScript &owner, size_t instance, const char* waypoint)
  :owner(owner),hnpc(nullptr),mvAlgo(*this){
  hnpc                 = new Daedalus::GEngineClasses::C_Npc();
  hnpc->wp             = waypoint;
  hnpc->instanceSymbol = instance;
  hnpc->userPtr        = this;
  owner.initializeInstance(*hnpc,instance);
  name = hnpc->name[0];
  }

Npc::Npc(WorldScript &owner, Serialize &fin)
  :owner(owner),hnpc(nullptr),mvAlgo(*this){
  hnpc          = new Daedalus::GEngineClasses::C_Npc();
  hnpc->userPtr = this;
  int32_t     flags=0;
  uint8_t     policy=0;
  uint32_t    size=0;
  std::string str;

  Daedalus::GEngineClasses::C_Npc& h = *hnpc;
  fin.read(h.instanceSymbol);
  fin.read(h.id,h.name,h.slot,h.effect,h.npcType);
  fin.read(flags); h.flags=Daedalus::GEngineClasses::C_Npc::ENPCFlag(flags);
  fin.read(h.attribute,h.hitChance,h.protection,h.damage);
  fin.read(h.damagetype,h.guild,h.level);
  fin.read(h.mission);
  fin.read(h.fight_tactic,h.weapon,h.voice,h.voicePitch,h.bodymass);
  fin.read(h.daily_routine,h.start_aistate);
  fin.read(h.spawnPoint,h.spawnDelay,h.senses,h.senses_range);
  fin.read(h.aivar);
  fin.read(h.wp,h.exp,h.exp_next,h.lp,h.bodyStateInterruptableOverride,h.noFocus);

  name = h.name[0];
  fin.read(policy); aiPolicy=ProcessPolicy(policy);
  fin.read(x,y,z,angle,sz);

  fin.read(str);
  setVisual(str.c_str());
  fin.read(body,head,vHead,vTeeth,bdColor,vColor);
  setVisualBody(vHead,vTeeth,vColor,bdColor,body,head);
  durtyTranform|=(TR_Pos|TR_Rot|TR_Scale);

  fin.read(wlkMode);
  fin.read(trGuild,talentsSk,talentsVl,refuseTalkMilis);
  invent.load(owner,*this,fin);
  fin.read(perceptionTime,perceptionNextTime);
  for(auto& i:perception)
    fin.read(i.func);
  fin.read(aiState.funcIni,aiState.funcLoop,aiState.funcEnd,aiState.sTime,aiState.eTime,aiState.started,aiState.loopNextTime);

  fin.read(size);
  routines.resize(size);
  for(auto& i:routines){
    fin.read(i.start,i.end,i.callback);
    fin.read(str);
    i.point = owner.world().findPoint(str);
    }

  animation.load(fin);
  }

Npc::~Npc(){
  if(currentInteract)
    currentInteract->dettach(*this);
  delete hnpc;
  }

void Npc::save(Serialize &fout) {
  Daedalus::GEngineClasses::C_Npc& h = *hnpc;
  fout.write(h.instanceSymbol);
  fout.write(h.id,h.name,h.slot,h.effect,h.npcType);
  fout.write(int32_t(h.flags));
  fout.write(h.attribute,h.hitChance,h.protection,h.damage);
  fout.write(h.damagetype,h.guild,h.level);
  fout.write(h.mission);
  fout.write(h.fight_tactic,h.weapon,h.voice,h.voicePitch,h.bodymass);
  fout.write(h.daily_routine,h.start_aistate);
  fout.write(h.spawnPoint,h.spawnDelay,h.senses,h.senses_range);
  fout.write(h.aivar);
  fout.write(h.wp,h.exp,h.exp_next,h.lp,h.bodyStateInterruptableOverride,h.noFocus);

  fout.write(uint8_t(aiPolicy));
  fout.write(x,y,z,angle,sz);

  if(animation.skeleton)
    fout.write(animation.skeleton->name()); else
    fout.write(std::string(""));
  fout.write(body,head,vHead,vTeeth,bdColor,vColor);
  fout.write(wlkMode);
  fout.write(trGuild,talentsSk,talentsVl,refuseTalkMilis);
  invent.save(fout);
  fout.write(perceptionTime,perceptionNextTime);
  for(auto& i:perception)
    fout.write(i.func);

  fout.write(aiState.funcIni,aiState.funcLoop,aiState.funcEnd,aiState.sTime,aiState.eTime,aiState.started,aiState.loopNextTime);

  fout.write(uint32_t(routines.size()));
  for(auto& i:routines){
    fout.write(i.start,i.end,i.callback);
    if(i.point)
      fout.write(i.point->name); else
      fout.write(std::string(""));
    }

  animation.save(fout);
  }

void Npc::setPosition(float ix, float iy, float iz) {
  if(x==ix && y==iy && z==iz)
    return;
  x = ix;
  y = iy;
  z = iz;
  durtyTranform |= TR_Pos;
  physic.setPosition(x,y,z);
  }

bool Npc::setPosition(const std::array<float,3> &pos) {
  if(x==pos[0] && y==pos[1] && z==pos[2])
    return false;
  x = pos[0];
  y = pos[1];
  z = pos[2];
  durtyTranform |= TR_Pos;
  physic.setPosition(x,y,z);
  return true;
  }

bool Npc::setViewPosition(const std::array<float,3> &pos) {
  x = pos[0];
  y = pos[1];
  z = pos[2];
  durtyTranform |= TR_Pos;
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
  durtyTranform |= TR_Rot;
  }

float Npc::angleDir(float x, float z) {
  float a=-90;
  if(x!=0.f || z!=0.f)
    a = 90+180.f*std::atan2(z,x)/float(M_PI);
  return a;
  }

void Npc::resetPositionToTA() {
  if(routines.size()==0 || isDead())
    return;

  attachToPoint(nullptr);
  setInteraction(nullptr);
  aiClearQueue();
  clearState(true);

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

void Npc::startDlgAnim() {
  if(weaponState()!=WeaponState::NoWeapon)
    return;
  auto ani = std::rand()%10+Anim::Dialog1;
  setAnim(Anim(ani));
  }

void Npc::stopDlgAnim() {
  auto a = anim();
  if(Anim::Dialog1<=a && a<=Anim::Dialog10){
    animation.resetAni();
    setAnim(animation.lastIdle);
    }
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

bool Npc::startClimb(Anim ani) {
  if(mvAlgo.startClimb()){
    setAnim(ani);
    return true;
    }
  return false;
  }

bool Npc::checkHealth(bool onChange) {
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
    setAnim(lastHitType=='A' ? Anim::DeadA : Anim::DeadB);
    return false;
    }
  if(isUnconscious()) {
    setAnim(lastHitType=='A' ? Anim::UnconsciousA : Anim::UnconsciousB);
    closeWeapon(true);
    return false;
    }

  auto& v = *hnpc;
  if(v.attribute[ATR_HITPOINTS]<=1) {
    if(v.attribute[ATR_HITPOINTSMAX]<=1){
      size_t fdead=owner.getSymbolIndex("ZS_Dead");
      startState(fdead,"");
      return false;
      }

    if(onChange && currentOther!=nullptr) {
      // currentOther must be externaly initialized
      if(owner.personAttitude(*this,*currentOther)==ATT_HOSTILE || guild()>GIL_SEPERATOR_HUM){
        if(v.attribute[ATR_HITPOINTS]<=0){
          size_t fdead=owner.getSymbolIndex("ZS_Dead");
          animation.resetAni();
          startState(fdead,"");

          if(hnpc->voice>0){
            char name[32]={};
            std::snprintf(name,sizeof(name),"SVM_%d_DEAD",int(hnpc->voice));
            emitSoundEffect(name,25,true);
            }
          }
        } else {
        size_t fdead=owner.getSymbolIndex("ZS_Unconscious");
        animation.resetAni();
        startState(fdead,"");

        if(hnpc->voice>0){
          char name[32]={};
          std::snprintf(name,sizeof(name),"SVM_%d_AARGH",int(hnpc->voice));
          emitSoundEffect(name,25,true);
          }
        }
      return false;
      }
    }
  physic.setEnable(true);
  return true;
  }

World& Npc::world() {
  return owner.world();
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
  if(owner.world().isInListenerRange({x,y,z}))
    animation.emitSfx(*this,owner.tickCount());
  animation.updateAnimation(owner.tickCount());
  }

void Npc::updateTransform() {
  if(durtyTranform){
    updatePos();
    durtyTranform=0;
    }
  }

const char *Npc::displayName() const {
  return hnpc->name[0].c_str();
  }

std::array<float,3> Npc::displayPosition() const {
  float h = 0;
  if(animation.skeleton)
    h = animation.skeleton->colisionHeight()*1.5f;
  return {{x,y+h,z}};
  }

void Npc::setName(const std::string &n) {
  name = n;
  }

void Npc::setVisual(const char* visual) {
  auto skelet = Resources::loadSkeleton(visual);
  setVisual(skelet);
  setPhysic(owner.world().getPhysic(visual));
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

static std::string addExt(const std::string& s,const char* ext){
  if(s.size()>0 && s.back()=='.')
    return s+&ext[1];
  return s+ext;
  }

void Npc::setVisualBody(int32_t headTexNr, int32_t teethTexNr, int32_t bodyTexNr, int32_t bodyTexColor,
                        const std::string &body, const std::string &head) {
  auto& w = owner.world();

  auto  vname = addExt(body,".MDM");
  auto  vhead = head.empty() ? StaticObjects::Mesh() : w.getView(addExt(head,".MMB"),headTexNr,teethTexNr,bodyTexColor);
  auto  vbody = body.empty() ? StaticObjects::Mesh() : w.getView(vname,bodyTexNr,0,bodyTexColor);

  animation.setVisualBody(std::move(vhead),std::move(vbody));
  this->body = body;
  this->head = head;
  vHead      = headTexNr;
  vTeeth     = teethTexNr;
  vColor     = bodyTexNr;
  bdColor    = bodyTexColor;

  invent.updateArmourView(owner,*this);
  durtyTranform|=TR_Pos; // update obj matrix
  //setVisualBody(std::move(vhead),std::move(vbody),bodyTexNr,bodyTexColor,body,head);
  }

/*
void Npc::setVisualBody(StaticObjects::Mesh&& h, StaticObjects::Mesh &&b, int32_t bodyVer, int32_t bodyColor,
                        const std::string &bodyS, const std::string &headS) {
  animation.setVisualBody(std::move(h),std::move(b));

  body    = bodyS;
  head    = headS;
  vColor  = bodyVer;
  bdColor = bodyColor;

  invent.updateArmourView(owner,*this);
  durtyTranform|=TR_Pos; // update obj matrix
  }*/

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

void Npc::resetView(bool clear) {
  if(clear){
    setPhysic(DynamicWorld::Item());
    animation.setVisualBody(StaticObjects::Mesh(),StaticObjects::Mesh());
    animation.sword  = StaticObjects::Mesh();
    animation.bow    = StaticObjects::Mesh();
    animation.armour = StaticObjects::Mesh();
    return;
    }

  if(animation.skeleton!=nullptr) {
    auto name = animation.skeleton->name();
    setVisual(name.c_str());
    setVisualBody(vHead,vTeeth,vColor,bdColor,body,head);
    setPhysic(owner.world().getPhysic(name));
    invent.updateBowView(owner,*this);
    invent.updateSwordView(owner,*this);
    }
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

bool Npc::setAnim(Npc::Anim a) {
  auto weaponSt=invent.weaponState();
  return setAnim(a,weaponSt,weaponSt);
  }

void Npc::stopAnim(const std::string &ani) {
  if(animation.stopAnim(ani))
    setAnim(animation.lastIdle);
  }

bool Npc::isStanding() const {
  return animation.current<Anim::IdleLast;
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
    return hnpc->hitChance[t];
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
    return hnpc->attribute[a];
  return 0;
  }

void Npc::changeAttribute(Npc::Attribute a, int32_t val) {
  if(a>=ATR_MAX || val==0)
    return;

  auto& v = *hnpc;
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
    return hnpc->protection[p];
  return 0;
  }

void Npc::changeProtection(Npc::Protection p, int32_t val) {
  if(p<PROT_MAX)
    hnpc->protection[p]=val;
  }

uint32_t Npc::instanceSymbol() const {
  return uint32_t(hnpc->instanceSymbol);
  }

uint32_t Npc::guild() const {
  uint32_t ret = uint32_t(hnpc->guild);
  return ret;
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

void Npc::setAttitude(Attitude att) {
  permAttitude = att;
  }

void Npc::setTempAttitude(Attitude att) {
  tmpAttitude = att;
  }

bool Npc::implLookAt(uint64_t dt) {
  if(currentLookAt!=nullptr) {
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
  if(implLookAt(dx,dz,true,dt))
    return true;
  currentLookAt=nullptr;
  return false;
  }

bool Npc::implLookAt(float dx, float dz, bool anim, uint64_t dt) {
  auto  gl   = std::min<uint32_t>(guild(),GIL_MAX);
  //float step = owner.guildVal().turn_speed[gl]*(dt/1000.f);
  float step = owner.guildVal().turn_speed[gl]*(dt/1000.f)*60.f/100.f;

  float a    = angleDir(dx,dz);
  float da   = a-angle;

  if(std::abs(int(da)%180)<=step && std::cos(double(da)*M_PI/180.0)>0){
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
  if(anim) {
    if(sgn<0) {
      if(setAnim(Anim::RotR))
        setDirection(angle-step);
      } else {
      if(setAnim(Anim::RotL))
        setDirection(angle+step);
      }
    } else {
    if(sgn<0)
      setDirection(angle-step); else
      setDirection(angle+step);
    }
  return true;
  }

bool Npc::implGoTo(uint64_t dt) {
  if(!currentGoTo && !currentGoToNpc)
    return false;

  if(currentGoTo){
    float dx = currentGoTo->x-x;
    //float dy = y-currentGoTo->position.y;
    float dz = currentGoTo->z-z;

    if(implLookAt(dx,dz,walkMode()!=WalkBit::WM_Run,dt)){ // TODO: force rotation, if angle > 45deg
      mvAlgo.aiGoTo(nullptr);
      return true;
      }

    if(!mvAlgo.aiGoTo(currentGoTo)) {
      attachToPoint(currentGoTo);
      currentGoTo = wayPath.pop();
      if(currentGoTo!=nullptr) {
        currentFpLock = FpLock(*currentGoTo);
        } else {
        if(isStanding())
          setAnim(AnimationSolver::Idle);
        }
      }
    return currentGoTo || mvAlgo.hasGoTo();
    } else {
    float dx = currentGoToNpc->x-x;
    //float dy = y-currentGoTo->position.y;
    float dz = currentGoToNpc->z-z;

    if(implLookAt(dx,dz,true,dt))
      return true;
    if(!mvAlgo.aiGoTo(currentGoToNpc,400)) {
      currentGoToNpc=nullptr;
      if(isStanding())
        setAnim(AnimationSolver::Idle);
      }
    return mvAlgo.hasGoTo();
    }
  }

bool Npc::isAtackMode() const {
  if(interactive()!=nullptr)
    return false;
  if(currentTarget==nullptr || isPlayer())
    return false;
  if(currentTarget->isDead())
    return false;
  return true;
  }

bool Npc::implAtack(uint64_t dt) {
  if(currentTarget==nullptr || isPlayer() || isTalk())
    return false;

  if(currentTarget->isDead()){
    return true;
    }

  if(weaponState()==WeaponState::NoWeapon)
    return false;

  float dx = currentTarget->x-x;
  float dz = currentTarget->z-z;

  auto ani = anim();
  if(ani!=Anim::Atack && ani!=Anim::AtackBlock){
    if(implLookAt(dx,dz,false,dt))
      return true;
    }

  FightAlgo::Action act = fghAlgo.tick(*this,*currentTarget,owner,dt);

  if(act==FightAlgo::MV_BLOCK) {
    if(setAnim(Anim::AtackBlock))
      fghAlgo.consumeAction();
    return true;
    }

  if(act==FightAlgo::MV_ATACK || act==FightAlgo::MV_ATACKL || act==FightAlgo::MV_ATACKR) {
    static const Anim ani[3]={Anim::Atack,Anim::AtackL,Anim::AtackR};
    if(act!=FightAlgo::MV_ATACK && !fghAlgo.isInAtackRange(*this,*currentTarget,owner)){
      fghAlgo.consumeAction();
      return true;
      }
    if(doAttack(ani[act-FightAlgo::MV_ATACK]))
      fghAlgo.consumeAction();
    return true;
    }

  if(act==FightAlgo::MV_STRAFEL) {
    if(implLookAt(*currentTarget,dt))
      return true;
    if(setAnim(Npc::Anim::MoveL))
      fghAlgo.consumeAction();
    return true;
    }

  if(act==FightAlgo::MV_STRAFER) {
    if(implLookAt(*currentTarget,dt))
      return true;
    if(setAnim(Npc::Anim::MoveR))
      fghAlgo.consumeAction();
    return true;
    }

  if(act==FightAlgo::MV_JUMPBACK) {
    if(setAnim(Npc::Anim::MoveBack))
      fghAlgo.consumeAction();
    return true;
    }

  if(act==FightAlgo::MV_NULL) {
    fghAlgo.consumeAction();
    return true;
    }

  if(act==FightAlgo::MV_MOVE) {
    fghAlgo.consumeAction();
    if(!mvAlgo.aiGoTo(currentTarget,fghAlgo.prefferedAtackDistance(*this,*currentTarget,owner))) {
      setAnim(AnimationSolver::Idle);
      }
    }

  return true;
  }

void Npc::commitDamage() {
  fghWaitToDamage = uint64_t(-1);
  if(currentTarget==nullptr)
    return;

  auto ani = anim();
  if(ani!=Anim::Atack && ani!=Anim::AtackL && ani!=Anim::AtackR && !(Anim::MagFirst<=ani && ani<=Anim::MagLast))
    return;

  static const float maxAngle = std::cos(float(M_PI/12));

  const float dx    = x-currentTarget->x;
  const float dz    = z-currentTarget->z;
  const float plAng = rotationRad()+float(M_PI/2);

  const float da = plAng-std::atan2(dz,dx);
  const float c  = std::cos(da);

  if(c<maxAngle && dx*dx+dz*dz>20*20)
    return;

  if(!fghAlgo.isInAtackRange(*this,*currentTarget,owner))
    return;
  currentTarget->takeDamage(*this);
  }

void Npc::takeDamage(Npc &other) {
  if(isDead())
    return;

  setOther(&other);
  perceptionProcess(other,this,0,PERC_ASSESSDAMAGE);
  owner.world().sendPassivePerc(*this,other,*this,PERC_ASSESSOTHERSDAMAGE);

  auto ani=anim();
  if(ani!=Anim::MoveBack && ani!=Anim::AtackBlock) {
    lastHit = &other;
    fghAlgo.onTakeHit();
    if(!isPlayer())
      setOther(lastHit);
    int dmg = isImmortal() ? 0 : other.damageValue(*this);
    if(isPlayer() && owner.isRamboMode())
      dmg = std::min(1,dmg);
    changeAttribute(ATR_HITPOINTS,-dmg);

    owner.world().emitWeaponsSound(other,*this);

    if(ani==Anim::Move  || ani==Anim::MoveL  || ani==Anim::MoveR ||
       ani==Anim::Atack || ani==Anim::AtackL || ani==Anim::AtackR ||
       ani==Anim::Warn  ||
       ani<Anim::IdleLast || (Anim::MagFirst<=ani && ani<=Anim::MagLast )) {
      animation.resetAni();
      }
    if(attribute(ATR_HITPOINTS)>0){
      if(lastHitType=='A')
        setAnim(Anim::StumbleA); else
        setAnim(Anim::StumbleB);
      //emitSoundEffect("FIG_DUMMYWOUND",25);
      }
    } else {
    emitSoundEffect("D_PARADE",25,true);
    }
  }

int Npc::damageValue(Npc &other) const {
  const int dtype = hnpc->damagetype;
  uint8_t   hitCh = TALENT_UNKNOWN;
  if(auto w = invent.activeWeapon()){
    if(w->is2H())
      hitCh = TALENT_2H; else
      hitCh = TALENT_1H;
    }
  int s          = attribute(Attribute::ATR_STRENGTH);
  int v          = 0;
  int critChance = int(owner.rand(100));

  for(int i=0;i<Daedalus::GEngineClasses::DAM_INDEX_MAX;++i){
    if((dtype & (1<<i))==0)
      continue;
    int vd = std::max(s + hnpc->damage[i] - other.hnpc->protection[i],0);
    if(hnpc->hitChance[hitCh]<critChance)
      vd = (vd-1)/10;
    if(other.hnpc->protection[i]>=0) // Filter immune
      v += vd;
    }
  return std::max(v,3);
  }

void Npc::tick(uint64_t dt) {
  owner.world().tickSlot(animation.soundSlot);

  if(!checkHealth(false)){
    fghWaitToDamage = uint64_t(-1);
    mvAlgo.aiGoTo(nullptr);
    mvAlgo.tick(dt);
    setOther(lastHit);
    aiActions.clear();
    setPhysic(DynamicWorld::Item());
    tickRoutine(); // tick for ZS_Death
    return;
    }

  if(fghWaitToDamage<owner.tickCount())
    commitDamage();

  // do parallel?
  mvAlgo.tick(dt);

  if(waitTime>=owner.tickCount())
    return;

  const bool isAtk = isAtackMode();
  if(isAtk) {
    implAtack(dt);
    } else {
    if(implLookAt(dt))
      return;

    if(implGoTo(dt))
      return;

    if(interactive()!=nullptr)
      setAnim(AnimationSolver::Interact); else
    if(currentGoTo==nullptr && currentGoToNpc==nullptr && (currentTarget==nullptr || currentTarget->isDown()) &&
       aiPolicy!=ProcessPolicy::Player && anim()!=Anim::Pray && anim()!=Anim::PrayRand) {
      if(weaponState()==WeaponState::NoWeapon)
        setAnim(animation.lastIdle); else
      if(animation.current>Anim::IdleLoopLast)
        setAnim(Anim::Idle);
      }
    }

  if(!aiState.started && aiState.funcIni!=0) {
    tickRoutine();
    }
  else if(aiActions.size()==0) {
    tickRoutine();
    if(aiActions.size()>0)
      nextAiAction(dt);
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
      setInteraction(nullptr);
      if(wayPath.last()!=act.point)
        wayPath = owner.world().wayTo(*this,*act.point);
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
      if(!closeWeapon(false))
        aiActions.push_front(std::move(act));
      break;
    case AI_StartState:
      if(startState(act.func,act.s0,aiState.eTime,act.i0==0))
        setOther(act.target);
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
    case AI_EquipBestArmor:
      invent.equipBestArmour(owner,*this);
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
      fghAlgo.onClearTarget();
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
    case AI_UnEquipArmor:
      invent.unequipArmour(owner,*this);
      break;
    case AI_Output:
      if(!owner.aiOutput(*this,*act.target,act.s0)) {
        aiActions.push_front(std::move(act));
        } else {
        startDlgAnim();
        }
      break;
    case AI_OutputSvm:{
      if(!owner.aiOutputSvm(*this,*act.target,act.s0,hnpc->voice)) {
        aiActions.push_front(std::move(act));
        } else {
        startDlgAnim();
        }
      break;
      }
    case AI_OutputSvmOverlay:
      if(!owner.aiOutputSvm(*this,*act.target,act.s0,hnpc->voice)) {
        aiActions.push_front(std::move(act));
        }
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
        if((fp->dirX!=0 || fp->dirZ!=0) && currentTarget==nullptr){
          if(implLookAt(fp->dirX,fp->dirZ,true,dt))
            aiActions.push_front(std::move(act));
          }
        }
      break;
      }
    case AI_SetNpcsToState:{
      const int32_t r = act.i0*act.i0;
      owner.world().detectNpc(position(),[&act,this,r](Npc& other){
        if(&other!=this && qDistTo(other)<r)
          other.aiStartState(act.func,1,other.currentOther,other.hnpc->wp);
        });
      break;
      }
    case AI_SetWalkMode:{
      setWalkMode(WalkBit(act.i0));
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
  if(!wp.empty()){
    hnpc->wp = wp;
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

void Npc::clearState(bool noFinalize) {
  if(aiState.funcIni!=0 && aiState.started){
    if(owner.isTalk(*this)) {
      // avoid ZS_Talk bug
      owner.invokeState(this,currentOther,nullptr,aiState.funcLoop);
      }
    if(!noFinalize)
      owner.invokeState(this,currentOther,nullptr,aiState.funcEnd);  // cleanup
    }
  aiState = AiState();
  aiState.funcIni = 0;
  }

void Npc::tickRoutine() {
  if(aiState.funcIni==0 && !isPlayer()) {
    auto& v = *hnpc;
    auto  r = currentRoutine();
    if(r.callback!=0) {
      if(r.point!=nullptr)
        v.wp = r.point->name;
      auto t = endTime(r);
      startState(r.callback,"",t,false);
      }
    else if(v.start_aistate!=0) {
      startState(v.start_aistate,"");
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
      int loop = owner.invokeState(this,currentOther,nullptr,aiState.funcLoop);
      if(aiState.eTime<=owner.world().time())
        loop=1;
      if(loop!=0){
        owner.invokeState(this,currentOther,nullptr,aiState.funcEnd);
        if(atackMode) {
          atackMode=false;
          setTarget(nullptr);
          }
        currentOther = nullptr;
        prevAiState  = aiState.funcIni;
        aiState      = AiState();
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

void Npc::clearNearestEnemy() {
  nearestEnemy = nullptr;
  }

void Npc::setNearestEnemy(Npc& n) {
  if(!isEnemy(n))
    return;
  if(nearestEnemy==nullptr || nearestEnemy->isDown()){
    nearestEnemy = &n;
    return;
    }
  float d2 = qDistTo(n);
  float d1 = qDistTo(*nearestEnemy);
  if(d2<d1 && canSeeNpc(n,true))
    nearestEnemy = &n;
  }

void Npc::setOther(Npc *ot) {
  if(isTalk() && ot && !ot->isPlayer())
    Log::e("unxepected perc acton");
  currentOther = ot;
  }

bool Npc::haveOutput() const {
  for(auto& i:aiActions)
    if(i.act==AI_Output)
      return true;

  return false;
  }

bool Npc::doAttack(Anim anim) {
  auto weaponSt=invent.weaponState();
  auto weapon  =invent.activeWeapon();

  if(weaponSt==WeaponState::NoWeapon)
    return false;

  if(weaponSt==WeaponState::Mage && weapon!=nullptr)
    anim=Anim(owner.spellCastAnim(*this,*weapon));

  if(animation.current==anim){
    return setAnim(Anim::Idle,weaponSt,weaponSt);
    }

  if(fghWaitToDamage==uint64_t(-1) && setAnim(anim,weaponSt,weaponSt)){
    fghWaitToDamage = owner.tickCount()+300;
    return true;
    }
  return false;
  }

void Npc::emitDlgSound(const char *sound) {
  owner.world().emitDlgSound(sound,x,y+180,z,WorldSound::talkRange);
  }

void Npc::emitSoundEffect(const char *sound, float range, bool freeSlot) {
  owner.world().emitSoundEffect(sound,x,y+100,z,range,freeSlot ? nullptr : &animation.soundSlot);
  }

void Npc::emitSoundGround(const char* sound, float range, bool freeSlot) {
  char    buf[256]={};
  uint8_t mat = mvAlgo.groundMaterial();
  std::snprintf(buf,sizeof(buf),"%s_%s",sound,ZenLoad::zCMaterial::getMatGroupString(ZenLoad::MaterialGroup(mat)));
  owner.world().emitSoundEffect(buf,x,y,z,range,freeSlot ? nullptr : &animation.soundSlot);
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
  uint32_t s   = bodySt;
  auto     ani = anim();
  if(isDead())
    s = BS_DEAD;
  else if(isUnconscious())
    s = BS_UNCONSCIOUS;
  else if(ani==Anim::Move || ani==Anim::MoveL || ani==Anim::MoveR || ani==Anim::MoveBack)
    s = BS_RUN;
  else if(ani==Anim::Fall || ani==Anim::FallDeep)
    s = BS_FALL;
  else if(ani==Anim::Sleep)
    s = BS_LIE;
  else if(ani==Anim::Sit || ani==Anim::GuardSleep || ani==Anim::Pray || ani==Anim::PrayRand)
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

Item* Npc::addItem(const uint32_t item, size_t count) {
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

void Npc::clearInventory() {
  invent.clear(owner,*this);
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
  return implLookAt(dx,dz,anim,dt);
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

bool Npc::closeWeapon(bool noAnim) {
  auto weaponSt=invent.weaponState();
  if(weaponSt==WeaponState::NoWeapon)
    return true;
  if(!noAnim && !setAnim(animation.current,WeaponState::NoWeapon,weaponSt))
    return false;
  invent.switchActiveWeapon(Item::NSLOT);
  updateWeaponSkeleton();
  hnpc->weapon = 0;
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
  Anim ani = animation.current==Anim::Idle ? Anim::Idle : Anim::Move;
  if(!setAnim(ani,WeaponState::Fist,weaponSt))
    return false;
  invent.switchActiveWeaponFist();
  updateWeaponSkeleton();
  hnpc->weapon = 1;
  return true;
  }

bool Npc::drawWeaponMele() {
  auto weaponSt=invent.weaponState();
  if(weaponSt==WeaponState::Fist || weaponSt==WeaponState::W1H || weaponSt==WeaponState::W2H)
    return true;
  if(invent.currentMeleWeapon()==nullptr)
    return drawWeaponFist();
  if(weaponSt!=WeaponState::NoWeapon) {
    closeWeapon(false);
    return false;
    }
  auto st  = invent.currentMeleWeapon()->is2H() ? WeaponState::W2H : WeaponState::W1H;
  Anim ani = animation.current==isStanding()    ? Anim::Idle       : Anim::Move;
  if(!setAnim(ani,st,weaponSt))
    return false;
  invent.switchActiveWeapon(1);
  updateWeaponSkeleton();
  hnpc->weapon = (st==WeaponState::W1H ? 3:4);
  if(invent.currentMeleWeapon()->handle()->material==ItemMaterial::MAT_METAL)
    emitSoundEffect("DRAWSOUND_ME",50,true); else
    emitSoundEffect("DRAWSOUND_WO",50,true);
  return true;
  }

bool Npc::drawWeaponBow() {
  auto weaponSt=invent.weaponState();
  if(weaponSt==WeaponState::Bow || weaponSt==WeaponState::CBow)
    return true;
  if(weaponSt!=WeaponState::NoWeapon) {
    closeWeapon(false);
    return false;
    }
  auto st  = invent.currentRangeWeapon()->isCrossbow() ? WeaponState::CBow : WeaponState::Bow;
  Anim ani = animation.current==isStanding()           ? Anim::Idle        : Anim::Move;
  if(!setAnim(ani,st,weaponSt))
    return false;
  invent.switchActiveWeapon(2);
  updateWeaponSkeleton();
  hnpc->weapon = (st==WeaponState::W1H ? 5:6);
  emitSoundEffect("DRAWSOUND_BOW",25,true);
  return true;
  }

bool Npc::drawMage(uint8_t slot) {
  auto weaponSt=invent.weaponState();
  if(weaponSt!=WeaponState::NoWeapon && weaponSt!=WeaponState::Mage) {
    closeWeapon(false);
    return false;
    }
  Anim ani = animation.current==isStanding() ? Anim::Idle : Anim::Move;
  if(!setAnim(ani,WeaponState::Mage,weaponSt))
    return false;
  invent.switchActiveWeapon(slot);
  updateWeaponSkeleton();
  hnpc->weapon = 7;
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

  const int32_t   splId = active->spellId();
  const SpellCode code  = SpellCode(owner.invokeMana(*this,currentTarget,*active));
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
      AiAction a;
      a.act  = AI_PlayAnimById;
      a.i0   = ani;
      aiActions.push_back(a);
      owner.invokeSpell(*this,currentTarget,*active);
      if(currentTarget!=nullptr){
        currentTarget->lastHitSpell = splId;
        currentTarget->perceptionProcess(*this,nullptr,0,PERC_ASSESSMAGIC);
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
  auto weaponSt=invent.weaponState();
  return setAnim(Anim::AimBow,weaponSt,weaponSt);
  }

bool Npc::shootBow() {
  auto active=invent.activeWeapon();
  if(active==nullptr)
    return false;
  auto weaponSt=invent.weaponState();
  return setAnim(Anim::Atack,weaponSt,weaponSt);
  //  return setAnim(Anim::AimBow,weaponSt,weaponSt);
  }

bool Npc::isEnemy(const Npc &other) const {
  return owner.personAttitude(*this,other)==ATT_HOSTILE;
  }

bool Npc::isDead() const {
  return owner.isDead(*this);
  }

bool Npc::isUnconscious() const {
  return owner.isUnconscious(*this);
  }

bool Npc::isDown() const {
  return isUnconscious() || isDead();
  }

bool Npc::isTalk() const {
  return owner.isTalk(*this);
  }

bool Npc::isPrehit() const {
  return fghWaitToDamage<owner.tickCount();
  }

bool Npc::isImmortal() const {
  return hnpc->flags & Daedalus::GEngineClasses::C_Npc::ENPCFlag::EFLAG_IMMORTAL;
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
  if(perceptionProcess(pl,nullptr,0,PERC_ASSESSTALK))
    setOther(&pl);
  }

bool Npc::perceptionProcess(Npc &pl,float quadDist) {
  static bool disable=false;
  if(disable)
    return false;

  float r = hnpc->senses_range;
  r = r*r;

  bool ret=false;
  if(quadDist<r && canSeeNpc(pl,true)){
    if(perceptionProcess(pl,nullptr,quadDist,PERC_ASSESSPLAYER)) {
      //currentOther = &pl;
      ret          = true;
      }
    }
  if(nearestEnemy!=nullptr){
    float dist=qDistTo(*nearestEnemy);
    if(perceptionProcess(*nearestEnemy,nullptr,dist,PERC_ASSESSENEMY)){
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
  float r = hnpc->senses_range;
  r = r*r;
  if(quadDist<r && perception[perc].func){
    owner.invokeState(this,&pl,victum,perception[perc].func);
    //currentOther=&pl;
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
  //clearState(true);
  aiState.funcEnd=0; // no cleanup

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

  std::array<float,3> norm={};
  if(physic.tryMoveN(pos,norm)){
    fallback = pos;
    return setViewPosition(pos);
    }

  if(speed>=physic.radius() || speed==0.f)
    return false;

  /*
  if(norm[2]>0)
    Log::d("");
  if(norm[2]<0)
    Log::d("");*/

  float scale=speed*0.25f;
  for(int i=1;i<4+3;++i){
    std::array<float,3> p=pos;
    p[0]+=norm[0]*scale*i;
    p[2]+=norm[2]*scale*i;

    std::array<float,3> nn={};
    if(physic.tryMoveN(p,nn)) {
      fallback = p;
      return setViewPosition(p);
      }
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

void Npc::aiStartState(uint32_t stateFn, int behavior, Npc* other, std::string wp) {
  auto& st = owner.getAiState(stateFn);(void)st;

  AiAction a;
  a.act    = AI_StartState;
  a.func   = stateFn;
  a.i0     = behavior;
  a.s0     = std::move(wp);
  a.target = other;
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
  currentGoToFlag = GoToHint::GT_Default;
  wayPath.clear();
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
  DynamicWorld* w = owner.world().physic();

  if(!freeLos){
    float dx  = x-tx, dz=z-tz;
    float dir = angleDir(dx,dz);
    float da  = float(M_PI)*(angle-dir)/180.f;
    if(double(std::cos(da))>0) // FIXME: spec requires +-100 view angle range
      return false;
    }
  // TODO: npc eyesight height
  const bool ret = w->ray(x,y+180,z, tx,ty,tz).hasCol;
  return !ret;
  }

void Npc::updatePos() {
  if(durtyTranform==TR_Pos){
    Matrix4x4& mt=animation.pos;
    mt.set(3,0,x);
    mt.set(3,1,y);
    mt.set(3,2,z);
    setPos(mt);
    } else {
    Matrix4x4 mt;
    mt.identity();
    mt.translate(x,y,z);
    mt.rotateOY(180-angle);
    mt.scale(sz[0],sz[1],sz[2]);
    setPos(mt);
    }
  }

void Npc::setPos(const Matrix4x4 &m) {
  animation.setPos(m);
  //physic.setPosition(x,y,z);
  }

bool Npc::setAnim(Npc::Anim a, WeaponState st0, WeaponState st) {
  return animation.setAnim(a,owner.tickCount(),st0,st,wlkMode,currentInteract,owner.world());
  }


#include "npc.h"

#include <Tempest/Matrix4x4>

#include "interactive.h"
#include "graphics/skeleton.h"
#include "worldscript.h"
#include "resources.h"

using namespace Tempest;

Npc::Npc(WorldScript &owner, Daedalus::GameState::NpcHandle hnpc)
  :owner(owner),hnpc(hnpc),mvAlgo(*this,owner.world()){
  }

Npc::~Npc(){
  if(currentInteract)
    currentInteract->dettach(*this);
  }

void Npc::setView(StaticObjects::Mesh &&m) {
  view = std::move(m);
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
  float a=-90;
  if(x!=0.f || z!=0.f)
    a = 90+180.f*std::atan2(z,x)/float(M_PI);
  //angle = -180+180.f*std::atan2(z,x)/float(M_PI);
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

void Npc::clearSpeed() {
  mvAlgo.clearSpeed();
  }

void Npc::setAiType(Npc::AiType t) {
  aiType=t;
  }

void Npc::tick(uint64_t dt) {
  if(aiType==AiType::AiNormal)
    ;//setAnim(Move);
  if(aiType==AiType::Player)
    mvAlgo.tick(dt);
  }

std::array<float,3> Npc::position() const {
  return {{x,y,z}};
  }

float Npc::rotation() const {
  return angle;
  }

float Npc::rotationRad() const {
  return angle*float(M_PI)/180.f;
  }

float Npc::translateY() const {
  return skInst.translateY();
  }

void Npc::updateAnimation() {
  if(animSq!=nullptr){
    uint64_t dt = owner.tickCount() - sAnim;
    skInst.update(*animSq,dt);

    head  .setSkeleton(skInst,pos);
    if(armour.isEmpty())
      view  .setSkeleton(skInst,pos); else
      armour.setSkeleton(skInst,pos);
    }
  }

const char *Npc::displayName() const {
  return owner.vmNpc(hnpc).name[0].c_str();
  }

void Npc::setName(const std::string &n) {
  name = n;
  }

void Npc::setOverlay(const Skeleton* sk,float /*time*/) {
  overlay=sk;
  if(animSq)
    animSq=animSequence(animSq->name.c_str());
  }

void Npc::setVisual(const Skeleton* v) {
  skeleton = v;
  skInst.bind(skeleton);

  current=NoAnim;
  setAnim(Anim::Idle);

  head  .setSkeleton(skeleton);
  view  .setSkeleton(skeleton);
  armour.setSkeleton(skeleton);
  setPos(pos); // update obj matrix
  }

void Npc::setVisualBody(StaticObjects::Mesh&& h, StaticObjects::Mesh &&body) {
  head = std::move(h);
  view = std::move(body);

  head.setSkeleton(skeleton,"BIP01 HEAD");
  view.setSkeleton(skeleton);
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
    if(animSq!=nullptr && animSq->animCls==Animation::Transition)
      sAnim = owner.tickCount(); // restart anim
    return;
    }
  animSq = ani;
  sAnim  = owner.tickCount();
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
  return animSq->isFly() && current!=Fall;
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
  if(t<TALENT_MAX)
    talents[t] = lvl;
  }

int32_t Npc::talentSkill(Npc::Talent t) const {
  if(t<TALENT_MAX)
    return talents[t];
  return 0;
  }

int32_t Npc::attribute(Npc::Attribute a) const {
  if(a<ATR_MAX)
    return owner.vmNpc(hnpc).attribute[a];
  return 0;
  }

int32_t Npc::protection(Npc::Protection p) const {
  if(p<PROT_MAX)
    return owner.vmNpc(hnpc).protection[p];
  return 0;
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
  return aiSt==State::DEAD;
  }

void Npc::setToFistMode() {
  }

void Npc::setToFightMode(const uint32_t item) {
  if(getItemCount(item)==0)
    addItem(item);

  equipItem(item);
  drawWeapon1H();
  }

Daedalus::GameState::ItemHandle Npc::addItem(const uint32_t item, size_t count) {
  return owner.getGameState().createInventoryItem(item, hnpc, count);
  }

void Npc::equipItem(const uint32_t /*item*/) {
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

bool Npc::setInteraction(Interactive *id) {
  if(currentInteract==id)
    return false;
  if(currentInteract)
    currentInteract->dettach(*this);
  currentInteract=id;
  if(currentInteract)
    return currentInteract->attach(*this);
  return false;
  }

bool Npc::isState(uint32_t stateFn) const {
  return currentRoutine().callback==stateFn;
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

std::vector<WorldScript::DlgChoise> Npc::dialogChoises(Npc& player) {
  auto ret = owner.dialogChoises(player.hnpc,this->hnpc);
  //if(ret.size()>0)
  //  owner.exec(ret[0],player.hnpc,this->hnpc);
  return ret;
  }

const std::list<Daedalus::GameState::ItemHandle>& Npc::getItems() {
  return owner.getInventoryOf(hnpc);
  }

size_t Npc::getItemCount(const uint32_t item) {
  auto& items = getItems();

  for(Daedalus::GameState::ItemHandle h : items) {
    Daedalus::GEngineClasses::C_Item& data = owner.getGameState().getItem(h);

    if(data.instanceSymbol==item)
      return data.amount;
    }
  return 0;
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

  if(st0==WeaponState::NoWeapon && st==WeaponState::W1H){
    if(a==Anim::Idle && cur==a)
      return animSequence("T_1H_2_1HRUN");
    if(a==Anim::Move && cur==a)
      return animSequence("T_MOVE_2_1HMOVE");
    }
  if(st0==WeaponState::W1H && st==WeaponState::NoWeapon){
    if(a==Anim::Idle && cur==a)
      return animSequence("T_1HMOVE_2_MOVE");
    if(a==Anim::Move && cur==a)
      return animSequence("T_1HMOVE_2_MOVE");
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
  if(a==Anim::Fall)
    return animSequence("S_FALLDN");
  if(a==Anim::Slide)
    return animSequence("S_SLIDE");

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
  if(overlay!=nullptr){
    if(auto s = overlay->sequence(name))
      return s;
    }
  return skeleton ? skeleton->sequence(name) : nullptr;
  }

const Npc::Routine &Npc::currentRoutine() const {
  if(routines.size()){
    //auto time = owner.gameTime();
    return routines[0];
    }

  static Routine r;
  return r;
  }

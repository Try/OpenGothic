#include "interactive.h"
#include "npc.h"

#include <Tempest/Matrix4x4>

#include "graphics/skeleton.h"
#include "worldscript.h"
#include "resources.h"

using namespace Tempest;

Npc::Npc(WorldScript &owner, Daedalus::GameState::NpcHandle hnpc)
  :owner(owner),hnpc(hnpc){
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
  x = pos[0];
  y = pos[1];
  z = pos[2];
  updatePos();
  }

void Npc::setDirection(float x, float /*y*/, float z) {
  if(x==0.f && z==0.f)
    angle = -90; else
    angle = 90+180.f*std::atan2(z,x)/float(M_PI);
  //angle = -180+180.f*std::atan2(z,x)/float(M_PI);
  updatePos();
  }

void Npc::setDirection(float rotation) {
  angle = rotation;
  updatePos();
  }

std::array<float,3> Npc::position() const {
  return {{x,y,z}};
  }

float Npc::rotation() const {
  return angle;
  }

float Npc::translateY() const {
  return skInst.translateY();
  }

void Npc::updateAnimation() {
  if(animSq!=nullptr){
    uint64_t dt = owner.tickCount() - sAnim;
    skInst.update(*animSq,dt);

    head  .setSkeleton(skInst,pos);
    view  .setSkeleton(skInst,pos);
    armour.setSkeleton(skInst,pos);
    }
  }

void Npc::setName(const std::string &n) {
  name = n;
  }

void Npc::setVisual(const Skeleton* v) {
  skeleton = v;
  skInst.bind(skeleton);

  current=NoAnim;
  setAnim(Anim::Idle);

  head.setSkeleton(skeleton);
  view.setSkeleton(skeleton);
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

void Npc::setFatness(float) {
  }

void Npc::setOverlay(const std::string& /*name*/,float /*time*/) {
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
    if((animSq->animCls==Animation::Transition && current!=RotL && current!=RotR && current!=MoveL && current!=MoveR) &&
       !animSq->isFinished(owner.tickCount()-sAnim))
      return;
    }
  auto ani = solveAnim(a,weaponSt,current,nextSt);
  current =a;
  weaponSt=nextSt;
  if(ani==animSq)
    return;
  animSq = ani;
  sAnim  = owner.tickCount();
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
  return animSq->isFly();
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
  //skeleton->debug();
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

void Npc::addRoutine(gtime s, gtime e, int32_t callback) {
  Routine r;
  r.start    = s;
  r.end      = e;
  r.callback = callback;
  routines.push_back(r);
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
  view  .setObjMatrix(pos);
  head  .setObjMatrix(pos);
  armour.setObjMatrix(pos);
  }

const Animation::Sequence *Npc::solveAnim(Npc::Anim a) const {
  return solveAnim(a,weaponSt,a,weaponSt);
  }

const Animation::Sequence *Npc::solveAnim(Npc::Anim a, WeaponState st0, Npc::Anim cur, WeaponState st) const {
  if(skeleton==nullptr)
    return nullptr;

  if(st0==WeaponState::NoWeapon && st==WeaponState::W1H){
    if(a==Anim::Idle && cur==a)
      return skeleton->sequence("T_1H_2_1HRUN");
    if(a==Anim::Move && cur==a)
      return skeleton->sequence("T_MOVE_2_1HMOVE");
    }
  if(st0==WeaponState::W1H && st==WeaponState::NoWeapon){
    if(a==Anim::Idle && cur==a)
      return skeleton->sequence("T_1HMOVE_2_MOVE");
    if(a==Anim::Move && cur==a)
      return skeleton->sequence("T_1HMOVE_2_MOVE");
    }

  if(true) {
    if(st0==WeaponState::Fist && st==WeaponState::NoWeapon)
      return skeleton->sequence("T_FISTMOVE_2_MOVE");
    if(st0==WeaponState::NoWeapon && st==WeaponState::Fist)
      return solveAnim("S_%sRUN",st);
    }

  if(currentInteract!=nullptr) {
    if(cur!=Interact && a==Interact)
      return skeleton->sequence(currentInteract->anim(Interactive::In));
    if(cur==Interact && a==Interact)
      return skeleton->sequence(currentInteract->anim(Interactive::Active));
    if(cur==Interact && a!=Interact)
      return skeleton->sequence(currentInteract->anim(Interactive::Out));
    }

  if(cur==Anim::NoAnim && a==Idle)
    return solveAnim("S_%sRUN",st);
  if(cur==Anim::Idle && a==cur)
    return solveAnim("S_%sRUN",st);
  if(cur==Anim::Idle && a==Move)
    return skeleton->sequence("T_RUN_2_RUNL");

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
    return skeleton->sequence("T_RUNL_2_RUN");
  if(cur==Anim::Move && a==Jump)
    return skeleton->sequence("T_RUNL_2_JUMP");

  /*
  if(a==Anim::Chest)
    return skeleton->sequence("T_CHESTSMALL_STAND_2_S0");
  if(a==Anim::Sit)
    return skeleton->sequence("S_BENCH_S1");
  if(a==Anim::Lab)
    return skeleton->sequence("S_LAB_S0");
  */

  if(cur==Anim::Idle && a==Anim::Jump)
    return skeleton->sequence("T_STAND_2_JUMP");
  if(cur==Anim::Jump && a==Anim::Idle)
    return skeleton->sequence("T_JUMP_2_STAND");
  if(a==Anim::Jump)
    return skeleton->sequence("S_JUMP");

  // FALLBACK
  if(a==Anim::Move)
    return solveAnim("S_%sRUNL",st);
  if(a==Anim::Idle)
    return solveAnim("S_%sRUN",st);
  return nullptr;
  }

const Animation::Sequence *Npc::solveAnim(const char *format, Npc::WeaponState st) const {
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
  if(auto ret=skeleton->sequence(name))
    return ret;
  std::snprintf(name,sizeof(name),format,"");
  if(auto ret=skeleton->sequence(name))
    return ret;
  std::snprintf(name,sizeof(name),format,"FIST");
  return skeleton->sequence(name);
  }

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
  angle = 90+180.f*std::atan2(z,x)/float(M_PI);
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
  drawWeaponMelee();
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

void Npc::drawWeaponMelee() {
  if(weaponSt==W1H)
    closeWeapon(); else
    setAnim(current,WeaponState::W1H);
  }

void Npc::drawWeapon2H() {
  if(weaponSt==W2H)
    closeWeapon(); else
    setAnim(current,WeaponState::W2H);
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

  if(a==Anim::Idle && cur==a && st0==WeaponState::NoWeapon && st==WeaponState::W1H)
    return skeleton->sequence("T_1H_2_1HRUN");
  if(a==Anim::Idle && cur==a && st0==WeaponState::W1H && st==WeaponState::NoWeapon)
    return skeleton->sequence("T_1HMOVE_2_MOVE");
  if(a==Anim::Move && cur==a && st0==WeaponState::NoWeapon && st==WeaponState::W1H)
    return skeleton->sequence("T_MOVE_2_1HMOVE");
  if(a==Anim::Move && cur==a && st0==WeaponState::W1H && st==WeaponState::NoWeapon)
    return skeleton->sequence("T_1HMOVE_2_MOVE");

  if(a==Anim::Idle && cur==a && st0==WeaponState::Fist && st==WeaponState::NoWeapon)
    return skeleton->sequence("T_FISTMOVE_2_MOVE");
  if(a==Anim::Idle && cur==a && st0==WeaponState::NoWeapon && st==WeaponState::Fist)
    return nullptr;//skeleton->sequence("T_MOVE_2_FISTMOVE");

  if(cur==Anim::Idle && a==Anim::Jump)
    return skeleton->sequence("T_STAND_2_JUMP");
  if(cur==Anim::Move && a==Anim::Jump)
    return skeleton->sequence("T_RUNL_2_JUMP");
  if(cur==Anim::Jump && a==Anim::Idle)
    return skeleton->sequence("T_JUMP_2_STAND");
  if(cur==Anim::Jump && a==Anim::Move)
    return skeleton->sequence("S_RUNL");
  if(a==Anim::RotL)
    return skeleton->sequence("T_RUNTURNL");
  if(a==Anim::RotR)
    return skeleton->sequence("T_RUNTURNR");
  if(a==Anim::MoveBack)
    return skeleton->sequence("T_JUMPB");

  char name[128]={};
  auto old  = animName(cur,st0);
  auto next = animName(a,st);

  if(old==nullptr || old==next){
    std::snprintf(name,sizeof(name),"S_%s",next);
    if(auto ret=skeleton->sequence(name))
      return ret;
    } else {
    std::snprintf(name,sizeof(name),"T_%s_2_%s",old,next);
    if(auto ret=skeleton->sequence(name))
      return ret;
    //if(std::strcmp(name,"T_RUN_2_FISTRUN")==0)
    //  return skeleton->sequence("T_MOVE_2_FISTMOVE"); //not exist in original game
    }

  // universal transition
  std::snprintf(name,sizeof(name),"T_%s",next);
  if(auto ret=skeleton->sequence(name))
    return ret;

  std::snprintf(name,sizeof(name),"S_%s",next);
  return skeleton->sequence(name);
  }

const char *Npc::animName(Npc::Anim a,WeaponState st) const {
  static const char* aniStd[] = {
    nullptr,
    "RUN",
    "RUNL",
    "JUMPB",
    "RUNSTRAFEL",
    "RUNSTRAFER",
    "RUNTURNL",
    "RUNTURNR",
    "JUMP",
    "FIST",
    "1H", //_2_1HRUN",
    "2H"  //_2_2HRUN"
    };
  static const char* aniFist[] = {
    nullptr,
    "FISTRUN",
    "FISTRUNL",
    "FISTJUMPB",
    "FISTRUNSTRAFEL",
    "FISTRUNSTRAFER",
    "FISTRUNTURNL",
    "FISTRUNTURNR",
    "JUMP",
    "FIST",
    "1H", //_2_1HRUN",
    "2H"  //_2_2HRUN"
    };
  static const char* ani1H[] = {
    nullptr,
    "1HRUN",
    "1HRUNL",
    "1HJUMPB",
    "1HRUNSTRAFEL",
    "1HRUNSTRAFER",
    "1HRUNTURNL",
    "1HRUNTURNR",
    "JUMP",
    "FIST",
    "1H", //_2_1HRUN",
    "2H"  //_2_2HRUN"
    };
  if(st==Fist)
    return aniFist[a];
  if(st==W1H)
    return ani1H[a];
  return aniStd[a];
  }

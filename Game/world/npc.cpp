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
  if(current==a)
    return;
  current=a;
  auto ani = solveAnim(a);
  if(ani==animSq)
    return;
  animSq = ani;
  sAnim  = owner.tickCount();
  }

ZMath::float3 Npc::animMoveSpeed(Anim a,uint64_t dt) const {
  auto ani = solveAnim(a);
  if(ani!=nullptr && ani->isMove())
    return ani->speed(dt);
  return ZMath::float3();
  }

void Npc::setTalentSkill(Npc::Talent t, int32_t lvl) {
  if(t<NPC_TALENT_MAX)
    talents[t] = lvl;
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

void Npc::drawWeaponMelee() {
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
  static const char* ani[] = {
    "",
    "S_RUN",
    "S_RUNL",
    "T_JUMPB",
    "T_RUNSTRAFEL",
    "T_RUNSTRAFER",
    "T_RUNTURNL",
    "T_RUNTURNR"
    };

  if(skeleton) {
    return &skeleton->sequence(ani[a]);
    }
  return nullptr;
  }

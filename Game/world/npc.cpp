#include "npc.h"

#include <Tempest/Matrix4x4>

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

void Npc::setDirection(float x, float y, float /*z*/) {
  angle = 180.f*std::atan2(y,x)/float(M_PI);
  updatePos();
  }

void Npc::updateAnimation() {
  if(skeleton){

    }
  }

void Npc::setName(const std::string &n) {
  name = n;
  }

void Npc::setVisual(const Skeleton* v) {
  skeleton = v;

  head.setSkeleton(skeleton);
  view.setSkeleton(skeleton);
  setPos(pos); // update obj matrix
  }

void Npc::setVisualBody(StaticObjects::Mesh&& h, StaticObjects::Mesh &&body) {
  head = std::move(h);
  view = std::move(body);

  head.setSkeleton(skeleton,"BIP01 HEAD");
  view.setSkeleton(skeleton);
  setPos(pos); // update obj matrix
  }

void Npc::setFatness(float) {
  }

void Npc::setOverlay(const std::string& name,float time) {
  }

void Npc::setScale(float x, float y, float z) {
  sz[0]=x;
  sz[1]=y;
  sz[2]=z;
  updatePos();
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
  mt.rotateOY(angle);
  mt.scale(sz[0],sz[1],sz[2]);

  setPos(mt);
  }

void Npc::setPos(const Matrix4x4 &m) {
  pos = m;
  view.setObjMatrix(pos);
  head.setObjMatrix(pos);
  }

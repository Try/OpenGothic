#include "npc.h"

#include "worldscript.h"
#include "resources.h"

Npc::Npc(WorldScript &owner, Daedalus::GameState::NpcHandle hnpc)
  :owner(owner),hnpc(hnpc){
  }

Npc::~Npc(){
  }

void Npc::setName(const std::string &n) {
  name = n;
  }

void Npc::setVisual(const std::string&) {
  }

void Npc::setVisualBody(const std::string &head, const std::string &body) {
  Resources::loadStMesh(head+".MMB");
  Resources::loadStMesh(body+".MDM");
  }

void Npc::setFatness(float) {
  }

void Npc::setOverlay(const std::string& name,float time) {

  }

void Npc::setScale(float x, float y, float z) {
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

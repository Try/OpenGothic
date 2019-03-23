#include "item.h"

#include "world.h"
#include "worldscript.h"

Item::Item(WorldScript& owner, Daedalus::GEngineClasses::C_Item *hitem)
  :owner(owner),hitem(hitem){
  }

void Item::setView(StaticObjects::Mesh &&m) {
  view = std::move(m);
  updateMatrix();
  }

void Item::setPosition(float x, float y, float z) {
  pos={{x,y,z}};
  updateMatrix();
  }

void Item::setDirection(float, float, float) {
  }

void Item::setMatrix(const Tempest::Matrix4x4 &m) {
  pos[0] = m.at(3,0);
  pos[1] = m.at(3,1);
  pos[2] = m.at(3,2);
  view.setObjMatrix(m);
  }

const char *Item::displayName() const {
  return owner.vmItem(hitem).name.c_str();
  }

const char *Item::description() const {
  return owner.vmItem(hitem).description.c_str();
  }

std::array<float,3> Item::position() const {
  return pos;
  }

bool Item::isGold() const {
  return owner.vmItem(hitem).instanceSymbol==owner.goldId();
  }

int32_t Item::mainFlag() const {
  return owner.vmItem(hitem).mainflag;
  }

int32_t Item::itemFlag() const {
  return owner.vmItem(hitem).flags;
  }

bool Item::isSpell() const {
  return uint32_t(mainFlag()) & Inventory::ITM_CAT_RUNE;
  }

bool Item::is2H() const {
  auto flg = uint32_t(itemFlag());
  return flg & (Inventory::ITM_2HD_SWD | Inventory::ITM_2HD_AXE);
  }

bool Item::isCrossbow() const {
  auto flg = uint32_t(itemFlag());
  return flg & Inventory::ITM_CROSSBOW;
  }

int32_t Item::spellId() const {
  return owner.vmItem(hitem).spell;
  }

const char *Item::uiText(size_t id) const {
  auto& v = owner.vmItem(hitem);
  return v.text[id].c_str();
  }

int32_t Item::uiValue(size_t id) const {
  auto& v = owner.vmItem(hitem);
  return v.count[id];
  }

size_t Item::count() const {
  auto& v = owner.vmItem(hitem);
  return v.amount;
  }

int32_t Item::cost() const {
  auto& v = owner.vmItem(hitem);
  return v.value;
  }

int32_t Item::sellCost() const {
  return int32_t(std::ceil(owner.tradeValueMultiplier()*cost()));
  }

bool Item::checkCond(const Npc &other) const {
  int32_t a=0,v=0;
  return checkCondUse(other,a,v) && checkCondRune(other,a,v);
  }

bool Item::checkCondUse(const Npc &other, int32_t &a, int32_t &nv) const {
  auto& itData = owner.vmItem(hitem);

  for(size_t i=0;i<Daedalus::GEngineClasses::C_Item::COND_ATR_MAX;++i){
    auto atr = Npc::Attribute(itData.cond_atr[i]);
    if(other.attribute(atr)<itData.cond_value[i] && itData.cond_value[i]!=0) {
      a  = atr;
      nv = itData.cond_value[i];
      return false;
      }
    }
  return true;
  }

bool Item::checkCondRune(const Npc &other, int32_t &cPl, int32_t &cIt) const {
  auto& itData = owner.vmItem(hitem);
  cIt          = itData.mag_circle;
  cPl          = other.mageCycle();
  return (cPl>=cIt);
  }

size_t Item::clsId() const {
  return owner.vmItem(hitem).instanceSymbol;
  }

void Item::updateMatrix() {
  Tempest::Matrix4x4 m;
  m.identity();
  m.translate(pos[0],pos[1],pos[2]);
  view.setObjMatrix(m);
  }

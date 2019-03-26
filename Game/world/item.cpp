#include "item.h"

#include "world.h"
#include "worldscript.h"

Item::Item(WorldScript& owner,Daedalus::GEngineClasses::C_Item *hitem)
  :owner(owner),hitem(hitem){
  }

Item::~Item() {
  delete hitem;
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
  return hitem->name.c_str();
  }

const char *Item::description() const {
  return hitem->description.c_str();
  }

std::array<float,3> Item::position() const {
  return pos;
  }

bool Item::isGold() const {
  return hitem->instanceSymbol==owner.goldId();
  }

int32_t Item::mainFlag() const {
  return hitem->mainflag;
  }

int32_t Item::itemFlag() const {
  return hitem->flags;
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
  return hitem->spell;
  }

int32_t Item::swordLength() const {
  return hitem->range;
  }

const char *Item::uiText(size_t id) const {
  return hitem->text[id].c_str();
  }

int32_t Item::uiValue(size_t id) const {
  return hitem->count[id];
  }

size_t Item::count() const {
  return hitem->amount;
  }

int32_t Item::cost() const {
  return hitem->value;
  }

int32_t Item::sellCost() const {
  return int32_t(std::ceil(owner.tradeValueMultiplier()*cost()));
  }

bool Item::checkCond(const Npc &other) const {
  int32_t a=0,v=0;
  return checkCondUse(other,a,v) && checkCondRune(other,a,v);
  }

bool Item::checkCondUse(const Npc &other, int32_t &a, int32_t &nv) const {
  auto& itData = *hitem;

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
  cIt = hitem->mag_circle;
  cPl = other.mageCycle();
  return (cPl>=cIt);
  }

size_t Item::clsId() const {
  return hitem->instanceSymbol;
  }

void Item::updateMatrix() {
  Tempest::Matrix4x4 m;
  m.identity();
  m.translate(pos[0],pos[1],pos[2]);
  view.setObjMatrix(m);
  }

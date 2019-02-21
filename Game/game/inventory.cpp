#include "inventory.h"

#include "world/worldscript.h"
#include "world/item.h"
#include "world/npc.h"
#include "world/world.h"

using namespace Daedalus::GameState;

Inventory::Inventory() {  
  }

Inventory::~Inventory() {
  }

size_t Inventory::goldCount() const {
  for(auto& i:items)
    if(i->isGold()){
      return i->count();
      }
  return 0;
  }

size_t Inventory::itemCount(const size_t cls) const {
  for(auto& i:items)
    if(i->clsId()==cls){
      return i->count();
      }
  return 0;
  }

const Item &Inventory::at(size_t i) const {
  return *items[i];
  }

void Inventory::addItem(std::unique_ptr<Item> &&p, WorldScript &vm, Npc &owner) {
  using namespace Daedalus::GEngineClasses;
  if(p==nullptr)
    return;

  const auto cls = p->clsId();
  p->setView(StaticObjects::Mesh());
  Item* it=findByClass(cls);
  if(it==nullptr) {
    items.emplace_back(std::move(p));
    } else {
    auto& c = vm.vmItem(p->handle());
    vm.vmItem(it->handle()).amount += c.amount;
    vm.getGameState().removeItem(p->handle());
    }

  autoEquip(cls,vm,owner);
  }

void Inventory::addItem(size_t itemSymbol, uint32_t count, WorldScript &vm, Npc &owner) {
  using namespace Daedalus::GEngineClasses;
  if(count<=0)
    return;

  Item* it=findByClass(itemSymbol);
  if(it==nullptr) {
    auto  h      = vm.getGameState().insertItem(itemSymbol);
    auto& itData = vm.getGameState().getItem(h);

    std::unique_ptr<Item> ptr{new Item(vm,h)};
    itData.userPtr = ptr.get();
    itData.amount  = count;
    it = ptr.get();
    items.emplace_back(std::move(ptr));
    } else {
    vm.vmItem(it->handle()).amount += count;
    }

  autoEquip(itemSymbol,vm,owner);
  }

void Inventory::delItem(size_t itemSymbol, uint32_t count, WorldScript &vm) {
  //TODO: handle unequip
  using namespace Daedalus::GEngineClasses;
  if(count<=0)
    return;
  Item* it=findByClass(itemSymbol);
  if(it==nullptr)
    return;
  auto& itData = vm.getGameState().getItem(it->handle());
  if(itData.amount>count)
    itData.amount-=count; else
    itData.amount = 0;
  vm.getGameState().removeItem(it->handle());
  for(size_t i=0;i<items.size();++i)
    if(items[i]->clsId()==itemSymbol){
      items.erase(items.begin()+int(i));
      break;
      }
  }

bool Inventory::equip(size_t cls, WorldScript &vm, Npc& owner) {
  Item* it=findByClass(cls);
  if(it==nullptr)
    return false;

  auto& itData = vm.getGameState().getItem(it->handle());
  if((itData.mainflag & ITM_CAT_ARMOR)!=0) {
    // TODO: equiping armor
    auto visual = itData.visual_change;
    if(visual.rfind(".asc")==visual.size()-4)
      std::memcpy(&visual[visual.size()-3],"MDM",3);
    auto vbody  = visual.empty() ? StaticObjects::Mesh() : vm.world().getView(visual,owner.bodyVer(),0,owner.bodyColor());
    owner.setArmour(std::move(vbody));
    }

  if((itData.mainflag & (Daedalus::GEngineClasses::C_Item::ITM_CAT_NF | Daedalus::GEngineClasses::C_Item::ITM_CAT_FF))) {
    // TODO
    }
  return false;
  }

Item *Inventory::findByClass(size_t cls) {
  for(auto& i:items)
    if(i->clsId()==cls)
      return i.get();
  return nullptr;
  }

bool Inventory::autoEquip(size_t cls, WorldScript &vm, Npc &owner) {
  if(owner.isPlayer())
    return false;
  return equip(cls,vm,owner);
  }

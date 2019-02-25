#include "inventory.h"

#include <Tempest/Log>

#include "world/worldscript.h"
#include "world/item.h"
#include "world/npc.h"
#include "world/world.h"

using namespace Daedalus::GameState;
using namespace Tempest;

Inventory::Inventory() {  
  }

Inventory::~Inventory() {
  }

size_t Inventory::goldCount() const {
  for(auto& i:items)
    if(i->isGold())
      return i->count();
  return 0;
  }

size_t Inventory::itemCount(const size_t cls) const {
  for(auto& i:items)
    if(i->clsId()==cls)
      return i->count();
  return 0;
  }

const Item &Inventory::at(size_t i) const {
  return *items[i];
  }

void Inventory::addItem(std::unique_ptr<Item> &&p, WorldScript &vm) {
  using namespace Daedalus::GEngineClasses;
  if(p==nullptr)
    return;
  ch=true;

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
  }

void Inventory::addItem(const char *name, uint32_t count, WorldScript &vm) {
  size_t id = vm.getSymbolIndex(name);
  if(id>0)
    addItem(id,count,vm);
  }

void Inventory::addItem(size_t itemSymbol, uint32_t count, WorldScript &vm) {
  using namespace Daedalus::GEngineClasses;
  if(count<=0)
    return;
  ch=true;

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
  }

void Inventory::delItem(size_t itemSymbol, uint32_t count, WorldScript &vm, Npc& owner) {
  using namespace Daedalus::GEngineClasses;
  if(count<=0)
    return;
  Item* it=findByClass(itemSymbol);
  return delItem(it,count,vm,owner);
  }

void Inventory::delItem(Item *it, uint32_t count, WorldScript &vm, Npc& owner) {
  if(it==nullptr)
    return;
  auto  handle = it->handle();
  auto& itData = vm.getGameState().getItem(handle);
  if(itData.amount>count)
    itData.amount-=count; else
    itData.amount = 0;
  if(itData.amount>0)
    return;

  // unequip, if have to
  unequip(it,vm,owner);

  for(size_t i=0;i<items.size();++i)
    if(items[i]->clsId()==itData.instanceSymbol){
      items.erase(items.begin()+int(i));
      break;
      }
  vm.getGameState().removeItem(handle);
  }

void Inventory::trasfer(Inventory &to, Inventory &from, Npc* fromNpc, size_t itemSymbol, uint32_t count, WorldScript &vm) {
  for(size_t i=0;i<from.items.size();++i){
    auto& it = *from.items[i];
    if(it.clsId()!=itemSymbol)
      continue;

    auto  handle = it.handle();
    auto& itData = vm.getGameState().getItem(handle);
    if(count>itData.amount)
      count=itData.amount;

    if(itData.amount==count) {
      if(it.isEquiped()){
        if(fromNpc==nullptr){
          Log::d("Inventory: invalid transfer call");
          return; // error
          }
        from.unequip(&it,vm,*fromNpc);
        }
      to.addItem(std::move(from.items[i]),vm);
      from.items.erase(from.items.begin()+int(i));
      } else {
      itData.amount-=count;
      to.addItem(itemSymbol,count,vm);
      }
    }
  }

bool Inventory::unequip(size_t cls, WorldScript &vm, Npc &owner) {
  Item* it=findByClass(cls);
  if(it==nullptr || !it->isEquiped())
    return false;
  unequip(it,vm,owner);
  return true;
  }

void Inventory::unequip(Item *it, WorldScript &vm, Npc &owner) {
  if(armour==it)
    setSlot(armour,nullptr,vm,owner);
  if(belt==it)
    setSlot(belt,nullptr,vm,owner);
  if(amulet==it)
    setSlot(amulet,nullptr,vm,owner);
  if(ringL==it)
    setSlot(ringL,nullptr,vm,owner);
  if(ringR==it)
    setSlot(ringR,nullptr,vm,owner);
  if(mele==it)
    setSlot(mele,nullptr,vm,owner);
  if(range==it)
    setSlot(range,nullptr,vm,owner);
  for(auto& i:numslot)
    if(i==it)
      setSlot(i,nullptr,vm,owner);
  }

bool Inventory::setSlot(Item *&slot, Item* next, WorldScript &vm, Npc& owner) {
  if(slot!=nullptr){
    auto& itData = vm.getGameState().getItem(slot->handle());
    auto  flag   = Flags(itData.mainflag);
    vm.invokeItem(&owner,itData.on_unequip);
    applyArmour(*slot,vm,owner,-1);
    slot->setAsEquiped(false);
    if(flag & ITM_CAT_ARMOR){
      owner.setArmour(StaticObjects::Mesh());
      }
    slot=nullptr;
    }

  if(next==nullptr || !next->checkCond(owner))
    return false;

  auto& itData = vm.getGameState().getItem(next->handle());
  vm.invokeItem(&owner,itData.on_equip);
  slot=next;
  slot->setAsEquiped(true);
  applyArmour(*slot,vm,owner,1);
  updateArmourView(vm,owner);
  return true;
  }

void Inventory::updateArmourView(WorldScript &vm, Npc& owner) {
  if(armour==nullptr)
    return;

  auto& itData = vm.getGameState().getItem(armour->handle());
  auto  flag   = Flags(itData.mainflag);
  if(flag & ITM_CAT_ARMOR){
    auto visual = itData.visual_change;
    if(visual.rfind(".asc")==visual.size()-4)
      std::memcpy(&visual[visual.size()-3],"MDM",3);
    auto vbody  = visual.empty() ? StaticObjects::Mesh() : vm.world().getView(visual,owner.bodyVer(),0,owner.bodyColor());
    owner.setArmour(std::move(vbody));
    }
  }

void Inventory::equipBestMeleWeapon(WorldScript &vm, Npc &owner) {
  auto a = bestMeleeWeapon(vm,owner);
  setSlot(mele,a,vm,owner);
  }

bool Inventory::equipNumSlot(Item *next, WorldScript &vm, Npc &owner) {
  for(auto& i:numslot){
    if(i==nullptr){
      setSlot(i,next,vm,owner);
      return true;
      }
    }
  return false;
  }

void Inventory::applyArmour(Item &it, WorldScript &vm, Npc &owner, int32_t sgn) {
  auto& itData = vm.getGameState().getItem(it.handle());

  for(size_t i=0;i<Npc::PROT_MAX;++i){
    auto v = owner.protection(Npc::Protection(i));
    owner.changeProtection(Npc::Protection(i),v+itData.protection[i]*sgn);
    }
  }

bool Inventory::use(size_t cls, WorldScript &vm, Npc &owner) {
  Item* it=findByClass(cls);
  if(it==nullptr)
    return false;

  auto& itData   = vm.getGameState().getItem(it->handle());
  auto  mainflag = Flags(itData.mainflag);
  auto  flag     = Flags(itData.flags);

  if(mainflag & ITM_CAT_NF)
    return setSlot(mele,it,vm,owner);

  if(mainflag & ITM_CAT_FF)
    return setSlot(range,it,vm,owner);

  if(mainflag & ITM_CAT_RUNE)
    return equipNumSlot(it,vm,owner);

  if(mainflag & ITM_CAT_ARMOR)
    return setSlot(armour,it,vm,owner);

  if(flag & ITM_BELT)
    return setSlot(belt,it,vm,owner);

  if(flag & ITM_AMULET)
    return setSlot(amulet,it,vm,owner);

  if(flag & ITM_RING) {
    if(ringL==nullptr)
      return setSlot(ringL,it,vm,owner);
    if(ringR==nullptr)
      return setSlot(ringR,it,vm,owner);
    return false;
    }

  if((flag & ITM_MULTI) && itData.on_state[0]!=0){
    // eat item
    vm.invokeItem(&owner,itData.on_state[0]);
    delItem(cls,1,vm,owner);
    return true;
    }
  return true;
  }

void Inventory::invalidateCond(Npc &owner) {
  invalidateCond(armour,owner);
  invalidateCond(belt  ,owner);
  invalidateCond(amulet,owner);
  invalidateCond(ringL ,owner);
  invalidateCond(mele  ,owner);
  invalidateCond(range ,owner);
  for(auto& i:numslot)
    invalidateCond(i,owner);
  }

void Inventory::invalidateCond(Item *&slot, Npc &owner) {
  if(slot && !slot->checkCond(owner))
    slot=nullptr;
  }

void Inventory::autoEquip(WorldScript &vm, Npc &owner) {
  ch=false;
  auto a = bestArmour(vm,owner);
  setSlot(armour,a,vm,owner);
  }

Item *Inventory::findByClass(size_t cls) {
  for(auto& i:items)
    if(i->clsId()==cls)
      return i.get();
  return nullptr;
  }

Item* Inventory::bestItem(WorldScript &vm, Npc &owner, Inventory::Flags f) {
  Item* ret=nullptr;
  int   g  =-1;
  for(auto& i:items) {
    auto& itData = vm.getGameState().getItem(i->handle());
    auto  flag   = Flags(itData.mainflag);
    if((flag & f)==0)
      continue;
    if(!i->checkCond(owner))
      continue;

    if(itData.value>g){
      ret=i.get();
      g = itData.value;
      }
    }
  return ret;
  }

Item *Inventory::bestArmour(WorldScript &vm, Npc &owner) {
  return bestItem(vm,owner,ITM_CAT_ARMOR);
  }

Item *Inventory::bestMeleeWeapon(WorldScript &vm, Npc &owner) {
  return bestItem(vm,owner,ITM_CAT_NF);
  }


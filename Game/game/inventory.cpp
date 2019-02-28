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
  sortItems();
  return *items[i];
  }

void Inventory::addItem(std::unique_ptr<Item> &&p, WorldScript &vm) {
  using namespace Daedalus::GEngineClasses;
  if(p==nullptr)
    return;
  sorted=false;

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
  sorted=false;

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
  sorted=false;
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
  sorted=false;

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

    from.sorted = false;
    to.sorted   = false;

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
    setSlot(armour,nullptr,vm,owner,false);
  if(belt==it)
    setSlot(belt,nullptr,vm,owner,false);
  if(amulet==it)
    setSlot(amulet,nullptr,vm,owner,false);
  if(ringL==it)
    setSlot(ringL,nullptr,vm,owner,false);
  if(ringR==it)
    setSlot(ringR,nullptr,vm,owner,false);
  if(mele==it)
    setSlot(mele,nullptr,vm,owner,false);
  if(range==it)
    setSlot(range,nullptr,vm,owner,false);
  for(auto& i:numslot)
    if(i==it)
      setSlot(i,nullptr,vm,owner,false);
  }

bool Inventory::setSlot(Item *&slot, Item* next, WorldScript &vm, Npc& owner, bool force) {
  if(next!=nullptr) {
    int32_t atr=0,nValue=0,plMag=0,itMag=0;
    if(!force && !next->checkCondUse(owner,atr,nValue)) {
      vm.printCannotUseError(owner,atr,nValue);
      return false;
      }

    if(!force && !next->checkCondRune(owner,plMag,itMag)) {
      vm.printCannotCastError(owner,plMag,itMag);
      return false;
      }
    }

  if(slot!=nullptr){
    auto& itData = vm.getGameState().getItem(slot->handle());
    auto  flag   = Flags(itData.mainflag);
    vm.invokeItem(&owner,itData.on_unequip);
    applyArmour(*slot,vm,owner,-1);
    slot->setAsEquiped(false);
    if(flag & ITM_CAT_ARMOR){
      owner.setArmour(StaticObjects::Mesh());
      }
    else if(flag & ITM_CAT_NF){
      owner.setSword(StaticObjects::Mesh());
      }
    else if(flag & ITM_CAT_FF){
      owner.setRangeWeapon(StaticObjects::Mesh());
      }
    slot=nullptr;
    }

  if(next==nullptr)
    return false;

  auto& itData = vm.getGameState().getItem(next->handle());
  vm.invokeItem(&owner,itData.on_equip);
  slot=next;
  slot->setAsEquiped(true);
  slot->setSlot(slotId(slot));
  applyArmour(*slot,vm,owner,1);

  updateArmourView(vm,owner);
  updateSwordView (vm,owner);
  updateBowView   (vm,owner);
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

void Inventory::updateSwordView(WorldScript &vm, Npc &owner) {
  if(mele==nullptr)
    return;

  auto& itData = vm.getGameState().getItem(mele->handle());
  auto  vbody  = vm.world().getView(itData.visual,itData.material,0,itData.material);
  owner.setSword(std::move(vbody));
  }

void Inventory::updateBowView(WorldScript &vm, Npc &owner) {
  if(range==nullptr)
    return;

  auto flag = Flags(range->mainFlag());
  if(flag & ITM_CAT_FF){
    auto& itData = vm.getGameState().getItem(range->handle());
    auto  vbody  = vm.world().getView(itData.visual,itData.material,0,itData.material);
    owner.setRangeWeapon(std::move(vbody));
    }
  }

void Inventory::equipBestMeleWeapon(WorldScript &vm, Npc &owner) {
  auto a = bestMeleeWeapon(vm,owner);
  setSlot(mele,a,vm,owner,false);
  }

const Item *Inventory::activeWeapon() const {
  if(active!=nullptr)
    return *active;
  return nullptr;
  }

Item *Inventory::activeWeapon() {
  if(active!=nullptr)
    return *active;
  return nullptr;
  }

void Inventory::switchActiveWeaponFist() {
  if(active==&mele)
    active=nullptr; else
    active=&mele;
  }

void Inventory::switchActiveWeapon(uint8_t slot) {
  if(slot==Item::NSLOT){
    active=nullptr;
    return;
    }

  Item** next=nullptr;
  if(slot==1)
    next=&mele;
  if(slot==2)
    next=&range;
  if(3<=slot && slot<=10)
    next=&numslot[slot-3];
  if(next==active)
    active=nullptr; else
  if(next!=nullptr && *next!=nullptr)
    active=next;
  }

Inventory::WeaponState Inventory::weaponState() const {
  if(active==nullptr)
    return WeaponState::NoWeapon;
  if(*active==nullptr){
    if(active==&mele)
      return WeaponState::Fist;
    return WeaponState::NoWeapon;
    }
  if(active==&mele) {
    if(mele->is2H())
      return WeaponState::W2H;
    return WeaponState::W1H;
    }
  if(active==&range) {
    auto itFlag = Flags(range->itemFlag());
    if(itFlag&ITM_CROSSBOW)
      return WeaponState::CBow;
    return WeaponState::Bow;
    }
  for(auto& i:numslot){
    if(active==&i)
      return WeaponState::Mage;
    }
  return WeaponState::NoWeapon;
  }

bool Inventory::equipNumSlot(Item *next, WorldScript &vm, Npc &owner,bool force) {
  for(auto& i:numslot){
    if(i==nullptr){
      setSlot(i,next,vm,owner,force);
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

bool Inventory::use(size_t cls, WorldScript &vm, Npc &owner, bool force) {
  Item* it=findByClass(cls);
  if(it==nullptr)
    return false;

  auto& itData   = vm.getGameState().getItem(it->handle());
  auto  mainflag = Flags(itData.mainflag);
  auto  flag     = Flags(itData.flags);

  if(mainflag & ITM_CAT_NF)
    return setSlot(mele,it,vm,owner,force);

  if(mainflag & ITM_CAT_FF)
    return setSlot(range,it,vm,owner,force);

  if(mainflag & ITM_CAT_RUNE)
    return equipNumSlot(it,vm,owner,force);

  if(mainflag & ITM_CAT_ARMOR)
    return setSlot(armour,it,vm,owner,force);

  if(flag & ITM_BELT)
    return setSlot(belt,it,vm,owner,force);

  if(flag & ITM_AMULET)
    return setSlot(amulet,it,vm,owner,force);

  if(flag & ITM_RING) {
    if(ringL==nullptr)
      return setSlot(ringL,it,vm,owner,force);
    if(ringR==nullptr)
      return setSlot(ringR,it,vm,owner,force);
    return false;
    }

  if(((flag & ITM_MULTI) || (flag & ITM_MISSION)) && itData.on_state[0]!=0){
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
  sortItems();

  auto a = bestArmour     (vm,owner);
  auto m = bestMeleeWeapon(vm,owner);
  auto r = bestRangeWeapon(vm,owner);
  setSlot(armour,a,vm,owner,false);
  setSlot(mele  ,m,vm,owner,false);
  setSlot(range ,r,vm,owner,false);
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

Item *Inventory::bestRangeWeapon(WorldScript &vm, Npc &owner) {
  return bestItem(vm,owner,ITM_CAT_FF);
  }

void Inventory::sortItems() const {
  if(sorted)
    return;
  sorted = true;
  std::sort(items.begin(),items.end(),[](std::unique_ptr<Item>& l,std::unique_ptr<Item>& r){
    return less(*l,*r);
    });
  }

bool Inventory::less(Item &il, Item &ir) {
  auto ordL = orderId(il);
  auto ordR = orderId(ir);

  if(ordL<ordR)
    return true;
  if(ordL>ordR)
    return false;

  if(il.cost()>ir.cost())
    return true;
  if(il.cost()<ir.cost())
    return false;

  return il.clsId()<ir.clsId();
  }

int Inventory::orderId(Item &i) {
  auto flag = Flags(i.mainFlag());

  if(flag&ITM_CAT_NF)
    return 0;
  if(flag&ITM_CAT_FF)
    return 1;
  if(flag&ITM_CAT_MUN)
    return 3;
  if(flag&ITM_CAT_POTION)
    return 4;
  if(flag&ITM_CAT_FOOD)
    return 5;
  if(flag&ITM_CAT_ARMOR)
    return 6;
  if(flag&ITM_CAT_RUNE)
    return 7;
  if(flag&ITM_CAT_DOCS)
    return 8;
  if(flag&ITM_CAT_LIGHT)
    return 9;
  return 100;
  }

uint8_t Inventory::slotId(Item *&slt) const {
  if(&slt==&mele)
    return 1;
  if(&slt==&range)
    return 2;

  uint8_t id=3;
  for(auto& i:numslot){
    if(&i==&slt)
      return id;
    ++id;
    }

  return 255;
  }


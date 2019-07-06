#include "inventory.h"
#include <Tempest/Log>

#include "game/gamescript.h"
#include "world/item.h"
#include "world/npc.h"
#include "world/world.h"
#include "serialize.h"

using namespace Daedalus::GameState;
using namespace Tempest;

Inventory::Inventory() {  
  }

Inventory::~Inventory() {
  }

void Inventory::load(Npc& owner, Serialize &s) {
  uint32_t sz=0;
  items.clear();
  s.read(sz);
  for(size_t i=0;i<sz;++i)
    items.emplace_back(std::make_unique<Item>(owner.world(),s));

  armour = readPtr(s);
  belt   = readPtr(s);
  amulet = readPtr(s);
  ringL  = readPtr(s);
  ringR  = readPtr(s);
  mele   = readPtr(s);
  range  = readPtr(s);
  for(auto& i:numslot)
    i = readPtr(s);

  uint8_t id=255;
  s.read(id);
  if(id==1)
    active=&mele;
  else if(id==2)
    active=&range;
  else if(3<=id && id<10)
    active=&numslot[id-3];

  updateArmourView(owner);
  updateSwordView (owner);
  updateBowView   (owner);
  }

void Inventory::save(Serialize &fout) {
  uint32_t sz=items.size();
  fout.write(sz);
  for(auto& i:items)
    i->save(fout);
  fout.write(indexOf(armour));
  fout.write(indexOf(belt)  );
  fout.write(indexOf(amulet));
  fout.write(indexOf(ringL) );
  fout.write(indexOf(ringR) );
  fout.write(indexOf(mele)  );
  fout.write(indexOf(range) );
  for(auto& i:numslot)
    fout.write(indexOf(i));

  uint8_t id=255;
  if(active==&mele)
    id=1;
  else if(active==&range)
    id=2;
  for(int i=0;i<8;++i)
    if(active==&numslot[i])
      id = uint8_t(3+i);
  fout.write(id);
  }

int32_t Inventory::priceOf(size_t cls) const {
  for(auto& i:items)
    if(i->clsId()==cls)
      return i->cost();
  return 0;
  }

int32_t Inventory::sellPriceOf(size_t cls) const {
  for(auto& i:items)
    if(i->clsId()==cls)
      return i->sellCost();
  return 0;
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

size_t Inventory::tradableCount() const {
  size_t c=0;
  for(auto& i:items)
    if(!i->isEquiped() && isTakable(*i) && !i->isGold())
      c++;
  return c;
  }

size_t Inventory::ransackCount() const {
  size_t c=0;
  for(auto& i:items)
    if(isTakable(*i))
      c++;
  return c;
  }

const Item &Inventory::at(size_t i) const {
  sortItems();
  return *items[i];
  }

const Item &Inventory::atTrade(size_t n) const {
  sortItems();
  size_t c=0;
  for(auto& i:items)
    if(!i->isEquiped() && isTakable(*i) && !i->isGold()) {
      if(c==n)
        return *i;
      c++;
      }
  throw std::logic_error("index out of range");
  }

const Item &Inventory::atRansack(size_t n) const {
  sortItems();
  size_t c=0;
  for(auto& i:items)
    if(isTakable(*i)) {
      if(c==n)
        return *i;
      c++;
      }
  throw std::logic_error("index out of range");
  }

Item* Inventory::addItem(std::unique_ptr<Item> &&p) {
  using namespace Daedalus::GEngineClasses;
  if(p==nullptr)
    return nullptr;
  sorted=false;

  const auto cls = p->clsId();
  p->setView(StaticObjects::Mesh());
  Item* it=findByClass(cls);
  if(it==nullptr) {
    p->clearView();
    items.emplace_back(std::move(p));
    return items.back().get();
    } else {
    auto& c = *p->handle();
    it->handle()->amount += c.amount;
    return p.get();
    }
  }

void Inventory::addItem(const char *name, uint32_t count, World &owner) {
  auto&  vm = owner.script();
  size_t id = vm.getSymbolIndex(name);
  if(id!=size_t(-1))
    addItem(id,count,owner);
  }

Item* Inventory::addItem(size_t itemSymbol, uint32_t count, World &owner) {
  using namespace Daedalus::GEngineClasses;
  if(count<=0)
    return nullptr;
  sorted=false;

  Item* it=findByClass(itemSymbol);
  if(it==nullptr) {
    std::unique_ptr<Item> ptr{new Item(owner,itemSymbol)};
    ptr->clearView();
    ptr->setCount(count);
    items.emplace_back(std::move(ptr));
    return items.back().get();
    } else {
    it->handle()->amount += count;
    return it;
    }
  }

void Inventory::delItem(size_t itemSymbol, uint32_t count, Npc& owner) {
  using namespace Daedalus::GEngineClasses;
  if(count<=0)
    return;
  sorted=false;
  Item* it=findByClass(itemSymbol);
  return delItem(it,count,owner);
  }

void Inventory::delItem(Item *it, uint32_t count, Npc& owner) {
  if(it==nullptr)
    return;
  auto  handle = it->handle();
  auto& itData = *handle;
  if(itData.amount>count)
    itData.amount-=count; else
    itData.amount = 0;
  if(itData.amount>0)
    return;

  // unequip, if have to
  unequip(it,owner);
  sorted=false;

  for(size_t i=0;i<items.size();++i)
    if(items[i]->clsId()==itData.instanceSymbol){
      items.erase(items.begin()+int(i));
      break;
      }
  }

void Inventory::trasfer(Inventory &to, Inventory &from, Npc* fromNpc, size_t itemSymbol, uint32_t count, World &wrld) {
  for(size_t i=0;i<from.items.size();++i){
    auto& it = *from.items[i];
    if(it.clsId()!=itemSymbol)
      continue;

    from.sorted = false;
    to.sorted   = false;

    auto  handle = it.handle();
    auto& itData = *handle;
    if(count>itData.amount)
      count=itData.amount;

    if(itData.amount==count) {
      if(it.isEquiped()){
        if(fromNpc==nullptr){
          Log::e("Inventory: invalid transfer call");
          return; // error
          }
        from.unequip(&it,*fromNpc);
        }
      to.addItem(std::move(from.items[i]));
      from.items.erase(from.items.begin()+int(i));
      } else {
      itData.amount-=count;
      to.addItem(itemSymbol,count,wrld);
      }
    }
  }

bool Inventory::unequip(size_t cls, Npc &owner) {
  Item* it=findByClass(cls);
  if(it==nullptr || !it->isEquiped())
    return false;
  unequip(it,owner);
  return true;
  }

void Inventory::unequip(Item *it, Npc &owner) {
  if(armour==it)
    setSlot(armour,nullptr,owner,false);
  if(belt==it)
    setSlot(belt,nullptr,owner,false);
  if(amulet==it)
    setSlot(amulet,nullptr,owner,false);
  if(ringL==it)
    setSlot(ringL,nullptr,owner,false);
  if(ringR==it)
    setSlot(ringR,nullptr,owner,false);
  if(mele==it)
    setSlot(mele,nullptr,owner,false);
  if(range==it)
    setSlot(range,nullptr,owner,false);
  for(auto& i:numslot)
    if(i==it)
      setSlot(i,nullptr,owner,false);
  }

bool Inventory::setSlot(Item *&slot, Item* next, Npc& owner, bool force) {
  GameScript& vm = owner.world().script();

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

  if(slot!=nullptr) {
    auto& itData = *slot->handle();
    auto  flag   = Flags(itData.mainflag);
    applyArmour(*slot,owner,-1);
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
    if(&slot==active)
      applyWeaponStats(owner,*slot,-1);
    slot=nullptr;
    vm.invokeItem(&owner,itData.on_unequip);
    }

  if(next==nullptr)
    return false;

  auto& itData = *next->handle();
  slot=next;
  slot->setAsEquiped(true);
  slot->setSlot(slotId(slot));
  applyArmour(*slot,owner,1);

  updateArmourView(owner);
  updateSwordView (owner);
  updateBowView   (owner);
  if(&slot==active)
    applyWeaponStats(owner,*slot,1);
  vm.invokeItem(&owner,itData.on_equip);
  return true;
  }

void Inventory::updateArmourView(Npc& owner) {
  if(armour==nullptr)
    return;

  auto& itData = *armour->handle();
  auto  flag   = Flags(itData.mainflag);
  if(flag & ITM_CAT_ARMOR){
    auto visual = itData.visual_change;
    if(visual.rfind(".asc")==visual.size()-4)
      std::memcpy(&visual[visual.size()-3],"MDM",3);
    auto vbody  = visual.empty() ? StaticObjects::Mesh() : owner.world().getView(visual,owner.bodyVer(),0,owner.bodyColor());
    owner.setArmour(std::move(vbody));
    }
  }

void Inventory::updateSwordView(Npc &owner) {
  if(mele==nullptr)
    return;

  auto& itData = *mele->handle();
  auto  vbody  = owner.world().getView(itData.visual,itData.material,0,itData.material);
  owner.setSword(std::move(vbody));
  }

void Inventory::updateBowView(Npc &owner) {
  if(range==nullptr)
    return;

  auto flag = Flags(range->mainFlag());
  if(flag & ITM_CAT_FF){
    auto& itData = *range->handle();
    auto  vbody  = owner.world().getView(itData.visual,itData.material,0,itData.material);
    owner.setRangeWeapon(std::move(vbody));
    }
  }

void Inventory::equipBestMeleWeapon(Npc &owner) {
  auto a = bestMeleeWeapon(owner);
  setSlot(mele,a,owner,false);
  }

void Inventory::equipBestRangeWeapon(Npc &owner) {
  auto a = bestRangeWeapon(owner);
  setSlot(range,a,owner,false);
  }

void Inventory::unequipWeapons(GameScript &vm, Npc &owner) {
  setSlot(mele, nullptr,owner,false);
  setSlot(range,nullptr,owner,false);
  }

void Inventory::unequipArmour(GameScript &vm, Npc &owner) {
  setSlot(armour,nullptr,owner,false);
  }

void Inventory::clear(GameScript&, Npc&) {
  std::vector<std::unique_ptr<Item>> used;
  for(auto& i:items)
    if(i->isEquiped() || i->isMission()){
      used.emplace_back(std::move(i));
      }
  items = std::move(used); // Gothic don't clear items, which are in use
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

void Inventory::switchActiveWeapon(Npc& owner,uint8_t slot) {
  if(slot==Item::NSLOT){
    if(active!=nullptr && *active!=nullptr)
      applyWeaponStats(owner,**active,-1);
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

  if(active!=nullptr && *active!=nullptr)
    applyWeaponStats(owner,**active,1);
  }

void Inventory::switchActiveSpell(int32_t spell, Npc& owner) {
  for(auto& i:items)
    if(i->spellId()==spell){
      setSlot(numslot[0],i.get(),owner,true);
      switchActiveWeapon(owner,3);
      return;
      }
  }

WeaponState Inventory::weaponState() const {
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

uint8_t Inventory::currentSpellSlot() const {
  for(uint8_t i=0;i<8;++i){
    if(active==&numslot[i])
      return i+3;
    }
  return Item::NSLOT;
  }

bool Inventory::equipNumSlot(Item *next, Npc &owner,bool force) {
  for(auto& i:numslot){
    if(i==nullptr){
      setSlot(i,next,owner,force);
      return true;
      }
    }
  return false;
  }

void Inventory::applyArmour(Item &it, Npc &owner, int32_t sgn) {
  auto& itData = *it.handle();

  for(size_t i=0;i<Npc::PROT_MAX;++i){
    auto v = owner.protection(Npc::Protection(i));
    owner.changeProtection(Npc::Protection(i),v+itData.protection[i]*sgn);
    }
  }

bool Inventory::use(size_t cls, Npc &owner, bool force) {
  Item* it=findByClass(cls);
  if(it==nullptr)
    return false;

  auto& itData   = *it->handle();
  auto  mainflag = Flags(itData.mainflag);
  auto  flag     = Flags(itData.flags);

  if(mainflag & ITM_CAT_NF)
    return setSlot(mele,it,owner,force);

  if(mainflag & ITM_CAT_FF)
    return setSlot(range,it,owner,force);

  if(mainflag & ITM_CAT_RUNE)
    return equipNumSlot(it,owner,force);

  if(mainflag & ITM_CAT_ARMOR)
    return setSlot(armour,it,owner,force);

  if(flag & ITM_BELT)
    return setSlot(belt,it,owner,force);

  if(flag & ITM_AMULET)
    return setSlot(amulet,it,owner,force);

  if(flag & ITM_RING) {
    if(ringL==nullptr)
      return setSlot(ringL,it,owner,force);
    if(ringR==nullptr)
      return setSlot(ringR,it,owner,force);
    return false;
    }

  if(((flag & ITM_MULTI) || (flag & ITM_MISSION)) && itData.on_state[0]!=0){
    // eat item
    auto& vm = owner.world().script();
    vm.invokeItem(&owner,itData.on_state[0]);
    delItem(cls,1,owner);
    return true;
    }
  return true;
  }

bool Inventory::equip(size_t cls, Npc &owner, bool force) {
  Item* it=findByClass(cls);
  if(it==nullptr || it->isEquiped())
    return false;
  return use(cls,owner,force);
  }

void Inventory::invalidateCond(Npc &owner) {
  if(!owner.isPlayer())
    return; // gothic doesn't care
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

void Inventory::autoEquip(Npc &owner) {
  sortItems();

  auto a = bestArmour     (owner);
  auto m = bestMeleeWeapon(owner);
  auto r = bestRangeWeapon(owner);
  setSlot(armour,a,owner,false);
  setSlot(mele  ,m,owner,false);
  setSlot(range ,r,owner,false);
  }

void Inventory::equipArmour(int32_t cls, Npc &owner) {
  if(cls<=0)
    return;
  auto it = findByClass(size_t(cls));
  if(it==nullptr)
    return;
  if(uint32_t(it->mainFlag()) & ITM_CAT_ARMOR){
    if(!it->isEquiped())
      use(size_t(cls),owner,true);
    }
  }

void Inventory::equipBestArmour(Npc &owner) {
  auto a = bestArmour(owner);
  setSlot(armour,a,owner,false);
  }

Item *Inventory::findByClass(size_t cls) {
  for(auto& i:items)
    if(i->clsId()==cls)
      return i.get();
  return nullptr;
  }

Item* Inventory::bestItem(Npc &owner, Inventory::Flags f) {
  Item* ret=nullptr;
  int   g  =-1;
  for(auto& i:items) {
    auto& itData = *i->handle();
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

Item *Inventory::bestArmour(Npc &owner) {
  return bestItem(owner,ITM_CAT_ARMOR);
  }

Item *Inventory::bestMeleeWeapon(Npc &owner) {
  return bestItem(owner,ITM_CAT_NF);
  }

Item *Inventory::bestRangeWeapon(Npc &owner) {
  return bestItem(owner,ITM_CAT_FF);
  }

bool Inventory::isTakable(const Item &i) const {
  auto flag = Flags(i.mainFlag());
  if(i.isEquiped()) {
    if(flag & ITM_CAT_ARMOR)
      return false;
    if(flag & ITM_CAT_RUNE)
      return false;
    }
  return true;
  }

void Inventory::applyWeaponStats(Npc& owner, const Item &weapon, int sgn) {
  auto& hnpc = *owner.handle();
  hnpc.damagetype = sgn>0 ? weapon.handle()->damageType : (1 << Daedalus::GEngineClasses::DAM_INDEX_BLUNT);
  for(size_t i=0;i<Daedalus::GEngineClasses::DAM_INDEX_MAX;++i){
    hnpc.damage[i] += sgn*weapon.handle()->damage[i];
    if(weapon.handle()->damageType & (1<<i)) {
      hnpc.damage[i] += sgn*weapon.handle()->damageTotal;
      }
    }
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

uint32_t Inventory::indexOf(const Item *it) const {
  for(size_t i=0;i<items.size();++i)
    if(items[i].get()==it)
      return i;
  return uint32_t(-1);
  }

Item *Inventory::readPtr(Serialize &fin) {
  uint32_t v=uint32_t(-1);
  fin.read(v);
  if(v<items.size())
    return items[v].get();
  return nullptr;
  }


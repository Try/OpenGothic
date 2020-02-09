#include "inventory.h"
#include <Tempest/Log>

#include "game/gamescript.h"
#include "world/item.h"
#include "world/npc.h"
#include "world/world.h"
#include "serialize.h"
#include <daedalus/DaedalusExcept.h>

using namespace Daedalus::GameState;
using namespace Tempest;

Inventory::Inventory() {  
  }

Inventory::~Inventory() {
  }

void Inventory::implLoad(Npc* owner, World& world, Serialize &s) {
  uint32_t sz=0;
  items.clear();
  s.read(sz);
  for(size_t i=0;i<sz;++i)
    items.emplace_back(std::make_unique<Item>(world,s,false));

  s.read(sz);
  mdlSlots.resize(sz);
  for(auto& i:mdlSlots) {
    s.read(i.slot);
    i.item = readPtr(s);
    }
  for(size_t i=0;i<mdlSlots.size();)
    if(mdlSlots[i].item==nullptr) {
      mdlSlots[i] = std::move(mdlSlots.back());
      mdlSlots.pop_back();
      } else {
      ++i;
      }
  if(s.version()>=5) {
    s.read(ammotSlot.slot);
    ammotSlot.item = readPtr(s);
    s.read(stateSlot.slot);
    stateSlot.item = readPtr(s);
    }

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

  if(owner!=nullptr) {
    updateArmourView(*owner);
    updateSwordView (*owner);
    updateBowView   (*owner);

    for(auto& i:mdlSlots) {
      auto& itData = *i.item->handle();
      auto  vbody  = world.getView(itData.visual,itData.material,0,itData.material);
      owner->setSlotItem(std::move(vbody),i.slot.c_str());
      }
    if(ammotSlot.item!=nullptr) {
      auto& itData = *ammotSlot.item->handle();
      auto  vbody  = world.getView(itData.visual,itData.material,0,itData.material);
      owner->setAmmoItem(std::move(vbody),ammotSlot.slot.c_str());
      }
    }
  s.read(curItem,stateItem);
  }

void Inventory::load(Npc &owner, Serialize &s) {
  implLoad(&owner,owner.world(),s);
  }

void Inventory::load(Interactive&, World& w, Serialize &s) {
  implLoad(nullptr,w,s);
  }

void Inventory::save(Serialize &fout) const {
  uint32_t sz=uint32_t(items.size());
  fout.write(sz);
  for(auto& i:items)
    i->save(fout);

  sz=uint32_t(mdlSlots.size());
  fout.write(sz);
  for(auto& i:mdlSlots){
    fout.write(i.slot,indexOf(i.item));
    }
  fout.write(ammotSlot.slot,indexOf(ammotSlot.item));
  fout.write(stateSlot.slot,indexOf(stateSlot.item));

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
  fout.write(curItem,stateItem);
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

Item &Inventory::at(size_t i) {
  sortItems();
  return *items[i];
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
  p->setView(MeshObjects::Mesh());
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

Item* Inventory::addItem(const char *name, uint32_t count, World &owner) {
  auto&  vm = owner.script();
  size_t id = vm.getSymbolIndex(name);
  if(id!=size_t(-1))
    return addItem(id,count,owner);
  return nullptr;
  }

Item* Inventory::addItem(size_t itemSymbol, uint32_t count, World &owner) {
  using namespace Daedalus::GEngineClasses;
  if(count<=0)
    return nullptr;
  sorted=false;

  Item* it=findByClass(itemSymbol);
  if(it==nullptr) {
    try {
      std::unique_ptr<Item> ptr{new Item(owner,itemSymbol)};
      ptr->clearView();
      ptr->setCount(count);
      items.emplace_back(std::move(ptr));
      return items.back().get();
      }
    catch(const Daedalus::InvalidCall& call) {
      Log::e("[invalid call in VM, while initializing item: ",itemSymbol,"]");
      for(auto& i:call.callstack)
        Log::e("-",i);
      Log::e("---end of callstack---");
      return nullptr;
      }
    } else {
    it->handle()->amount += count;
    return it;
    }
  }

void Inventory::delItem(size_t itemSymbol, uint32_t count, Npc& owner) {
  using namespace Daedalus::GEngineClasses;
  if(count<=0)
    return;
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

  // clear slots
  for(size_t i=0;i<mdlSlots.size();)
    if(mdlSlots[i].item==it) {
      mdlSlots[i] = std::move(mdlSlots.back());
      mdlSlots.pop_back();
      } else {
      ++i;
      }
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

Item *Inventory::getItem(size_t instance) {
  return findByClass(instance);
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
  if(it->isEquiped()) {
    // error
    Log::e("[",owner.displayName(),"] inconsistent inventory state");
    setSlot(it,nullptr,owner,false);
    }
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
    if(&slot==active)
      applyWeaponStats(owner,*slot,-1);
    slot=nullptr;

    if(flag & ITM_CAT_ARMOR){
      owner.updateArmour();
      }
    else if(flag & ITM_CAT_NF){
      owner.setSword(MeshObjects::Mesh());
      }
    else if(flag & ITM_CAT_FF){
      owner.setRangeWeapon(MeshObjects::Mesh());
      }
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
  if(&slot==active) {
    updateRuneView  (owner);
    applyWeaponStats(owner,*slot,1);
    }
  vm.invokeItem(&owner,itData.on_equip);
  return true;
  }

void Inventory::updateArmourView(Npc& owner) {
  if(armour==nullptr)
    return;

  auto& itData = *armour->handle();
  auto  flag   = Flags(itData.mainflag);
  if(flag & ITM_CAT_ARMOR)
    owner.updateArmour();
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

void Inventory::updateRuneView(Npc &owner) {
  if(active==nullptr || *active==nullptr)
    return;

  auto* sp = *active;
  if(!sp->isSpellOrRune())
    return;

  const VisualFx*   vfx      = owner.world().script().getSpellVFx(sp->spellId());
  const ParticleFx* pfx      = owner.world().script().getSpellFx(vfx);
  auto              vemitter = owner.world().getView(pfx);
  owner.setMagicWeapon(std::move(vemitter));
  }

void Inventory::equipBestMeleWeapon(Npc &owner) {
  auto a = bestMeleeWeapon(owner);
  setSlot(mele,a,owner,false);
  }

void Inventory::equipBestRangeWeapon(Npc &owner) {
  auto a = bestRangeWeapon(owner);
  setSlot(range,a,owner,false);
  }

void Inventory::unequipWeapons(GameScript &, Npc &owner) {
  setSlot(mele, nullptr,owner,false);
  setSlot(range,nullptr,owner,false);
  }

void Inventory::unequipArmour(GameScript &, Npc &owner) {
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
  if(next!=nullptr && *next!=nullptr)
    active=next;

  if(active!=nullptr && *active!=nullptr)
    applyWeaponStats(owner,**active,1);
  }

void Inventory::switchActiveSpell(int32_t spell, Npc& owner) {
  for(uint8_t i=0;i<8;++i) {
    auto s = numslot[i];
    if(s!=nullptr && s->isSpellOrRune() && s->spellId()==spell){
      switchActiveWeapon(owner,uint8_t(i+3));
      updateRuneView(owner);
      return;
      }
    }

  for(auto& i:items)
    if(i->spellId()==spell){
      setSlot(numslot[0],i.get(),owner,true);
      switchActiveWeapon(owner,3);
      updateRuneView(owner);
      return;
      }
  }

uint8_t Inventory::currentSpellSlot() const {
  for(uint8_t i=0;i<8;++i){
    if(active==&numslot[i])
      return uint8_t(i+3);
    }
  return Item::NSLOT;
  }

void Inventory::putCurrentToSlot(Npc& owner, const char *slot) {
  if(curItem!=0) {
    putToSlot(owner,size_t(curItem),slot);
    curItem = 0;
    return;
    }
  if(stateItem!=0)
    implPutState(owner,size_t(stateItem),slot);
  }

void Inventory::putToSlot(Npc& owner, size_t cls, const char *slot) {
  clearSlot(owner,slot,false);

  Item* it=findByClass(cls);
  if(it==nullptr)
    it = addItem(cls,1,owner.world());

  for(auto& i:mdlSlots)
    if(i.slot==slot) {
      i.item = it;

      auto& itData = *it->handle();
      auto  vitm   = owner.world().getView(itData.visual,itData.material,0,itData.material);
      owner.setSlotItem(std::move(vitm),slot);
      return;
      }
  mdlSlots.emplace_back();
  MdlSlot& sl = mdlSlots.back();
  sl.slot = slot;
  sl.item = it;
  auto& itData = *it->handle();
  auto  vitm   = owner.world().getView(itData.visual,itData.material,0,itData.material);
  owner.setSlotItem(std::move(vitm),slot);
  }

void Inventory::clearSlot(Npc& owner,const char *slot,bool remove) {
  const bool all = (slot==nullptr || slot[0]=='\0');
  for(size_t i=0;i<mdlSlots.size();)
    if(all || mdlSlots[i].slot==slot) {
      owner.clearSlotItem(slot);
      auto last = mdlSlots[i].item;
      mdlSlots[i] = mdlSlots.back();
      mdlSlots.pop_back();
      if(remove)
        delItem(last,1,owner);
      } else {
      ++i;
      }
  }

void Inventory::putAmmunition(Npc& owner, size_t cls, const char* slot) {
  Item* it = (cls==0 ? nullptr : findByClass(cls));
  if(it==nullptr) {
    ammotSlot.slot.clear();
    ammotSlot.item = nullptr;
    owner.setAmmoItem(MeshObjects::Mesh(),nullptr);
    return;
    }

  ammotSlot.slot = slot;
  ammotSlot.item = it;
  auto& itData = *it->handle();
  auto  vitm   = owner.world().getView(itData.visual,itData.material,0,itData.material);
  owner.setAmmoItem(std::move(vitm),slot);
  }

void Inventory::implPutState(Npc& owner, size_t cls, const char* slot) {
  Item* it = (cls==0 ? nullptr : findByClass(cls));
  if(it==nullptr) {
    stateSlot.slot.clear();
    stateSlot.item = nullptr;
    owner.setStateItem(MeshObjects::Mesh(),nullptr);
    return;
    }

  stateSlot.slot = slot;
  stateSlot.item = it;
  auto& itData = *it->handle();
  auto  vitm   = owner.world().getView(itData.visual,itData.material,0,itData.material);
  owner.setStateItem(std::move(vitm),slot);
  }

bool Inventory::putState(Npc& owner, size_t cls, int mode) {
  Item* it = (cls==0 ? nullptr : findByClass(cls));
  if(it==nullptr) {
    implPutState(owner,0,stateSlot.slot.c_str());
    setStateItem(0);
    return true;
    }

  if(mode>0 && !owner.setAnimItem(it->handle()->scemeName.c_str()))
    return false;

  if(mode==0)
    implPutState(owner,cls,"ZS_LEFTHAND");

  setCurrentItem(0);
  setStateItem(cls);
  return true;
  }

void Inventory::setCurrentItem(size_t cls) {
  curItem = int32_t(cls);
  }

void Inventory::setStateItem(size_t cls) {
  stateItem = int32_t(cls);
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
  if(flag & ITM_TORCH) {
    const char* overlay = "HUMANS_TORCH.MDS";
    if(owner.hasOverlay(overlay)) {
      owner.delOverlay(overlay);
      owner.delItem(cls,1);
      } else {
      owner.addOverlay(overlay,0);
      }
    }

  if(!owner.setAnimItem(it->handle()->scemeName.c_str()))
    return false;

  setCurrentItem(it->clsId());
  if(itData.on_state[0]!=0){
    auto& vm = owner.world().script();
    vm.invokeItem(&owner,itData.on_state[0]);
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
  //hnpc.damagetype = sgn>0 ? weapon.handle()->damageType : (1 << Daedalus::GEngineClasses::DAM_INDEX_BLUNT);
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

std::pair<int, int> Inventory::orderId(Item &i) {
  // TODO: invCatOrder = COMBAT,POTION,FOOD,ARMOR,MAGIC,RUNE,DOCS,OTHER,NONE

  // auto flg  = Flags(i.itemFlag());
  auto mflg = Flags(i.mainFlag());

  if(mflg&ITM_CAT_NF)
    return std::make_pair(0,-i.handle()->damageTotal);
  if(mflg&ITM_CAT_FF)
    return std::make_pair(1,-i.handle()->damageTotal);
  if(mflg&ITM_CAT_MUN)
    return std::make_pair(2,i.clsId());
  if(mflg&ITM_CAT_POTION)
    return std::make_pair(3,-i.clsId());
  if(mflg&ITM_CAT_FOOD)
    return std::make_pair(4,-i.cost());
  if(mflg&ITM_CAT_ARMOR)
    return std::make_pair(5,-i.cost());
  if(mflg&ITM_CAT_MAGIC)
    return std::make_pair(6,-i.cost());
  if(mflg&ITM_CAT_RUNE)
    return std::make_pair(i.isSpell() ? 8 : 7,-i.cost());
  if(mflg&ITM_CAT_DOCS)
    return std::make_pair(9,-i.clsId());
  if(mflg&ITM_CAT_LIGHT)
    return std::make_pair(10,-i.cost());
  if(mflg&ITM_CAT_NONE)
    return std::make_pair(12,-i.cost());

  // OTHER
  return std::make_pair(11,-i.cost());
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
  if(it==nullptr)
    return uint32_t(-1);
  for(size_t i=0;i<items.size();++i)
    if(items[i].get()==it)
      return uint32_t(i);
  return uint32_t(-1);
  }

Item *Inventory::readPtr(Serialize &fin) {
  uint32_t v=uint32_t(-1);
  fin.read(v);
  if(v<items.size())
    return items[v].get();
  return nullptr;
  }


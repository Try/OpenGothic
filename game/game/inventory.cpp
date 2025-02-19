#include "inventory.h"

#include <Tempest/Log>

#include "world/objects/item.h"
#include "world/objects/npc.h"
#include "world/world.h"
#include "game/gamescript.h"
#include "serialize.h"
#include "gothic.h"

using namespace Tempest;

const Item& Inventory::Iterator::operator*() const {
  return *owner->items[at];
  }

const Item* Inventory::Iterator::operator ->() const {
  return owner->items[at].get();
  }

bool Inventory::Iterator::isEquipped() const {
  auto& cur = *owner->items[at];
  return subId==0 && cur.isEquipped();
  }

uint8_t Inventory::Iterator::slot() const {
  auto& cur = *owner->items[at];
  return subId==0 ? cur.slot() : Item::NSLOT;
  }

size_t Inventory::Iterator::count() const {
  auto& cur = *owner->items[at];
  if(!cur.isMulti()) {
    if(cur.isEquipped() && subId==0)
      return cur.equipCount();
    if(cur.isEquipped())
      return cur.count()-cur.equipCount();
    return cur.count();
    }
  return cur.count();
  }

Inventory::Iterator& Inventory::Iterator::operator++() {
  auto& it  = owner->items;
  auto& cur = *it[at];
  if(!cur.isMulti()) {
    if(cur.isEquipped() && cur.count()>1 && subId==0) {
      ++subId;
      return *this;
      }
    subId = 0;
    }
  at++;
  skipHidden();
  return *this;
  }

bool Inventory::Iterator::isValid() const {
  return at<owner->items.size();
  }

Inventory::Iterator::Iterator(Inventory::IteratorType t, const Inventory* owner)
  :type(t), owner(owner) {
  owner->sortItems();
  skipHidden();
  }

void Inventory::Iterator::skipHidden() {
  auto& it = owner->items;
  if(type==T_Trade) {
    while(at<it.size() && (it[at]->isEquipped() || it[at]->isGold()))
      ++at;
    }
  if(type==T_Ransack) {
    while(at<it.size() && it[at]->isEquipped()) {
      ++at;
      }
    }
  }


Inventory::Inventory() {
  }

Inventory::~Inventory() {
  }

bool Inventory::isEmpty() const {
  return items.size()==0 && active==nullptr;
  }

void Inventory::implLoad(Npc* owner, World& world, Serialize &s) {
  uint32_t sz=0;
  items.clear();
  s.read(sz);
  for(size_t i=0;i<sz;++i)
    items.emplace_back(std::make_unique<Item>(world,s,Item::T_Inventory));

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

  s.read(ammotSlot.slot);
  ammotSlot.item = readPtr(s);
  s.read(stateSlot.slot);
  stateSlot.item = readPtr(s);

  armor  = readPtr(s);
  belt   = readPtr(s);
  amulet = readPtr(s);
  ringL  = readPtr(s);
  ringR  = readPtr(s);
  melee  = readPtr(s);
  range  = readPtr(s);
  if(s.version()>45)
    shield = readPtr(s);
  for(auto& i:numslot)
    i = readPtr(s);

  uint8_t id=255;
  s.read(id);
  if(id==1)
    active=&melee;
  else if(id==2)
    active=&range;
  else if(3<=id && id<10)
    active=&numslot[id-3];
  s.read(curItem,stateItem);

  if(owner!=nullptr)
    updateView(*owner);
  }

void Inventory::load(Serialize &s, Npc& owner) {
  implLoad(&owner,owner.world(),s);
  }

void Inventory::load(Serialize& s, Interactive&, World& w) {
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

  fout.write(indexOf(armor) );
  fout.write(indexOf(belt)  );
  fout.write(indexOf(amulet));
  fout.write(indexOf(ringL) );
  fout.write(indexOf(ringR) );
  fout.write(indexOf(melee) );
  fout.write(indexOf(range) );
  fout.write(indexOf(shield));
  for(auto& i:numslot)
    fout.write(indexOf(i));

  uint8_t id=255;
  if(active==&melee)
    id=1;
  else if(active==&range)
    id=2;
  for(int i=0;i<8;++i)
    if(active==&numslot[i])
      id = uint8_t(3+i);
  fout.write(id);
  fout.write(curItem,stateItem);
  }

Inventory::Iterator Inventory::iterator(IteratorType t) const {
  return Iterator(t,this);
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

Item* Inventory::addItem(std::unique_ptr<Item> &&p) {
  if(p==nullptr)
    return nullptr;
  sorted=false;

  const auto cls = p->clsId();
  p->clearView();
  Item* it=findByClass(cls);
  if(it==nullptr) {
    p->clearView();
    items.emplace_back(std::move(p));
    return items.back().get();
    } else {
    it->setCount(it->count()+p->count());
    it->handle().owner       = p->handle().owner;
    it->handle().owner_guild = p->handle().owner_guild;
    return p.get();
    }
  }

Item* Inventory::addItem(std::string_view name, size_t count, World &owner) {
  auto&  vm = owner.script();
  size_t id = vm.findSymbolIndex(name);
  if(id!=size_t(-1))
    return addItem(id,count,owner);
  return nullptr;
  }

Item* Inventory::addItem(size_t itemSymbol, size_t count, World &owner) {
  if(count<=0)
    return nullptr;
  sorted=false;

  Item* it=findByClass(itemSymbol);
  if(it==nullptr) {
    try {
      std::unique_ptr<Item> ptr{new Item(owner,itemSymbol,Item::T_Inventory)};
      ptr->setCount(count);
      items.emplace_back(std::move(ptr));
      return items.back().get();
      }
    catch(const std::runtime_error& call) {
      Log::e("[invalid call in VM, while initializing item: ",itemSymbol,"]");
      return nullptr;
      }
    } else {
    it->setCount(it->count()+count);
    return it;
    }
  }

void Inventory::delItem(size_t itemSymbol, size_t count, Npc& owner) {
  if(count<=0)
    return;
  Item* it=findByClass(itemSymbol);
  return delItem(it,count,owner);
  }

void Inventory::delItem(Item *it, size_t count, Npc& owner) {
  if(it==nullptr)
    return;

  if(it->count()>count)
    it->setCount(it->count()-count); else
    it->setCount(0);

  if(it->count()>0)
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
    if(items[i]->clsId()==it->clsId()){
      items.erase(items.begin()+int(i));
      break;
      }
  }

void Inventory::transfer(Inventory &to, Inventory &from, Npc* fromNpc, size_t itemSymbol, size_t count, World &wrld) {
  for(size_t i=0;i<from.items.size();++i){
    auto& it = *from.items[i];
    if(it.clsId()!=itemSymbol)
      continue;

    from.sorted = false;
    to.sorted   = false;

    if(count>it.count())
      count=it.count();

    if(it.count()==count) {
      if(it.isEquipped()) {
        if(fromNpc==nullptr){
          Log::e("Inventory: invalid transfer call");
          return; // error
          }
        from.unequip(&it,*fromNpc);
        }
      to.addItem(std::move(from.items[i]));
      from.items.erase(from.items.begin()+int(i));
      } else {
      it.setCount(it.count()-count);
      to.addItem(itemSymbol,count,wrld);
      }
    }
  }

Item *Inventory::getItem(size_t instance) {
  return findByClass(instance);
  }

bool Inventory::unequip(size_t cls, Npc &owner) {
  Item* it=findByClass(cls);
  if(it==nullptr || !it->isEquipped())
    return false;
  unequip(it,owner);
  return true;
  }

void Inventory::unequip(Item *it, Npc &owner) {
  if(armor==it) {
    setSlot(armor,nullptr,owner,false);
    return;
    }
  if(belt==it) {
    setSlot(belt,nullptr,owner,false);
    return;
    }
  if(amulet==it) {
    setSlot(amulet,nullptr,owner,false);
    return;
    }
  if(ringL==it) {
    setSlot(ringL,nullptr,owner,false);
    return;
    }
  if(ringR==it) {
    setSlot(ringR,nullptr,owner,false);
    return;
    }
  if(melee==it) {
    setSlot(melee,nullptr,owner,false);
    return;
    }
  if(range==it) {
    setSlot(range,nullptr,owner,false);
    return;
    }
  if(shield==it) {
    setSlot(shield,nullptr,owner,false);
    return;
    }

  for(auto& i:numslot)
    if(i==it)
      setSlot(i,nullptr,owner,false);
  if(it->isEquipped()) {
    // error
    Log::e("[",owner.displayName().data(),"] inconsistent inventory state");
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
    auto& itData   = slot->handle();
    auto  mainFlag = ItmFlags(itData.main_flag);
    auto  flag     = ItmFlags(itData.flags);

    applyArmor(*slot,owner,-1);
    if(slot->isEquipped())
      slot->setAsEquipped(false);
    if(&slot==active)
      applyWeaponStats(owner,*slot,-1);
    slot=nullptr;

    if(flag & ITM_SHIELD){
      owner.setShield(MeshObjects::Mesh());
      }
    else if(mainFlag & ITM_CAT_ARMOR){
      owner.updateArmor();
      }
    else if(mainFlag & ITM_CAT_NF){
      owner.setSword(MeshObjects::Mesh());
      }
    else if(mainFlag & ITM_CAT_FF){
      owner.setRangedWeapon(MeshObjects::Mesh());
      }
    vm.invokeItem(&owner,uint32_t(itData.on_unequip));
    }

  if(next==nullptr)
    return false;

  auto& itData = next->handle();
  slot=next;
  slot->setAsEquipped(true);
  slot->setSlot(slotId(slot));
  applyArmor(*slot,owner,1);

  updateArmorView (owner);
  updateSwordView (owner);
  updateBowView   (owner);
  updateShieldView(owner);
  if(&slot==active) {
    updateRuneView  (owner);
    applyWeaponStats(owner,*slot,1);
    }
  vm.invokeItem(&owner,uint32_t(itData.on_equip));
  return true;
  }

void Inventory::updateView(Npc& owner) {
  auto& world = owner.world();

  updateArmorView (owner);
  updateSwordView (owner);
  updateBowView   (owner);
  updateShieldView(owner);
  updateRuneView  (owner);

  for(auto& i:mdlSlots) {
    auto  vbody  = world.addView(i.item->handle());
    owner.setSlotItem(std::move(vbody),i.slot);
    }
  if(ammotSlot.item!=nullptr) {
    auto  vbody  = world.addView(ammotSlot.item->handle());
    owner.setAmmoItem(std::move(vbody),ammotSlot.slot);
    }
  if(stateSlot.item!=nullptr) {
    auto  vitm   = world.addView(stateSlot.item->handle());
    owner.setStateItem(std::move(vitm),stateSlot.slot);
    }
  }

void Inventory::updateArmorView(Npc& owner) {
  if(armor==nullptr)
    return;

  auto& itData = armor->handle();
  auto  flag   = ItmFlags(itData.main_flag);
  if(flag & ITM_CAT_ARMOR)
    owner.updateArmor();
  }

void Inventory::updateSwordView(Npc &owner) {
  if(melee==nullptr) {
    owner.setSword(MeshObjects::Mesh());
    return;
    }

  auto  vbody  = owner.world().addView(melee->handle());
  owner.setSword(std::move(vbody));
  }

void Inventory::updateBowView(Npc &owner) {
  if(range==nullptr) {
    owner.setRangedWeapon(MeshObjects::Mesh());
    return;
    }

  auto flag = range->mainFlag();
  if(flag & ITM_CAT_FF){
    auto  vbody  = owner.world().addView(range->handle());
    owner.setRangedWeapon(std::move(vbody));
    }
  }

void Inventory::updateShieldView(Npc& owner) {
  if(shield==nullptr) {
    owner.setShield(MeshObjects::Mesh());
    return;
    }

  auto flag = ItmFlags(shield->itemFlag());
  if(flag & ITM_SHIELD){
    auto  vbody  = owner.world().addView(shield->handle());
    owner.setShield(std::move(vbody));
    }
  }

void Inventory::updateRuneView(Npc &owner) {
  if(active==nullptr || *active==nullptr)
    return;

  auto* sp = *active;
  if(!sp->isSpellOrRune())
    return;

  const VisualFx* vfx = owner.world().script().spellVfx(sp->spellId());
  owner.setMagicWeapon(Effect(*vfx,owner.world(),owner,SpellFxKey::Init));
  }

void Inventory::equipBestMeleeWeapon(Npc &owner) {
  auto a = bestMeleeWeapon(owner);
  if(a!=nullptr)
    setSlot(melee,a,owner,false);
  }

void Inventory::equipBestRangedWeapon(Npc &owner) {
  auto a = bestRangedWeapon(owner);
  if(a!=nullptr)
    setSlot(range,a,owner,false);
  }

void Inventory::unequipWeapons(GameScript &, Npc &owner) {
  setSlot(melee,nullptr,owner,false);
  setSlot(range,nullptr,owner,false);
  }

void Inventory::unequipArmor(GameScript &, Npc &owner) {
  setSlot(armor,nullptr,owner,false);
  }

void Inventory::clear(GameScript&, Npc&, bool includeMissionItm) {
  std::vector<std::unique_ptr<Item>> used;
  for(auto& i:items)
    if(i->isEquipped() || (i->isMission() && !includeMissionItm)){
      used.emplace_back(std::move(i));
      }
  items = std::move(used); // Gothic don't clear items, which are in use
  }

void Inventory::clear(GameScript& vm, Interactive& owner, bool includeMissionItm) {
  std::vector<std::unique_ptr<Item>> used;
  for(auto& i:items)
    if(i->isMission() && !includeMissionItm){
      used.emplace_back(std::move(i));
      }
  items = std::move(used); // Gothic don't clear items, which are in use
  }

bool Inventory::hasSpell(int32_t splId) const {
  for(auto& i:items)
    if(i->spellId()==splId)
      return true;
  return false;
  }

bool Inventory::hasMissionItems() const {
  for(auto& i:items)
    if(i->isMission())
      return true;
  return false;
  }

const Item *Inventory::activeWeapon() const {
  if(active!=nullptr)
    return *active;
  return nullptr;
  }

bool Inventory::hasRangedWeaponWithAmmo() const {
  uint32_t munition = 0;
  for(auto& i:items) {
    uint32_t cls = uint32_t(i->handle().munition);
    if(cls>0 && cls!=munition) {
      for(auto& it:items)
        if(it->clsId()==cls)
          return true;
      munition = cls;
      }
    }
  return false;
  }

Item *Inventory::activeWeapon() {
  if(active!=nullptr)
    return *active;
  return nullptr;
  }

void Inventory::switchActiveWeaponFist() {
  if(active==&melee)
    active=nullptr; else
    active=&melee;
  }

void Inventory::switchActiveWeapon(Npc& owner, uint8_t slot) {
  if(active!=nullptr && *active!=nullptr)
    applyWeaponStats(owner,**active,-1);
  active=nullptr;

  if(slot==Item::NSLOT)
    return;

  Item** next=nullptr;
  if(slot==1)
    next=&melee;
  if(slot==2)
    next=&range;
  if(3<=slot && slot<=10)
    next=&numslot[slot-3];
  if(next==active)
    return;
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

bool Inventory::hasStateItem() const {
  return stateSlot.item!=nullptr || stateItem!=0;
  }

void Inventory::putCurrentToSlot(Npc& owner, std::string_view slot) {
  if(curItem>0) {
    putToSlot(owner,size_t(curItem),slot);
    curItem = 0;
    return;
    }
  if(stateItem>0)
    implPutState(owner,size_t(stateItem),slot);
  }

void Inventory::putToSlot(Npc& owner, size_t cls, std::string_view slot) {
  clearSlot(owner,slot,false);

  Item* it=findByClass(cls);
  if(it==nullptr)
    it = addItem(cls,1,owner.world());

  for(auto& i:mdlSlots)
    if(i.slot==slot) {
      i.item = it;

      auto  vitm   = owner.world().addView(it->handle());
      owner.setSlotItem(std::move(vitm),slot);
      return;
      }
  mdlSlots.emplace_back();
  MdlSlot& sl = mdlSlots.back();
  sl.slot = slot;
  sl.item = it;
  auto  vitm   = owner.world().addView(it->handle());
  owner.setSlotItem(std::move(vitm),slot);
  }

bool Inventory::clearSlot(Npc& owner, std::string_view slot, bool remove) {
  uint32_t count = 0;
  const bool all = slot.empty();
  for(size_t i=0;i<mdlSlots.size();)
    if(all || mdlSlots[i].slot==slot) {
      owner.clearSlotItem(slot);
      auto last = mdlSlots[i].item;
      mdlSlots[i] = mdlSlots.back();
      mdlSlots.pop_back();
      if(remove)
        delItem(last,1,owner);
      ++count;
      } else {
      ++i;
      }
  if(all || stateSlot.slot==slot) {
    if(stateSlot.item!=nullptr)
      ++count;
    implPutState(owner,0,stateSlot.slot);
    }
  return count>0;
  }

void Inventory::putAmmunition(Npc& owner, size_t cls, std::string_view slot) {
  Item* it = (cls==0 ? nullptr : findByClass(cls));
  if(it==nullptr) {
    ammotSlot.slot.clear();
    ammotSlot.item = nullptr;
    owner.setAmmoItem(MeshObjects::Mesh(),"");
    return;
    }

  ammotSlot.slot = slot;
  ammotSlot.item = it;
  auto& itData = it->handle();
  auto  vitm   = owner.world().addView(itData);
  owner.setAmmoItem(std::move(vitm),slot);
  }

void Inventory::implPutState(Npc& owner, size_t cls, std::string_view slot) {
  Item* it = (cls==0 ? nullptr : findByClass(cls));
  if(it==nullptr) {
    stateSlot.slot.clear();
    stateSlot.item = nullptr;
    owner.setStateItem(MeshObjects::Mesh(),"");
    return;
    }

  stateSlot.slot = slot;
  stateSlot.item = it;
  auto  vitm   = owner.world().addView(it->handle());
  owner.setStateItem(std::move(vitm),slot);
  }

bool Inventory::putState(Npc& owner, size_t cls, int state) {
  Item* it = (cls==0 ? nullptr : findByClass(cls));
  if(it==nullptr) {
    setStateItem(0);
    return owner.stopItemStateAnim();
    }

  if(!owner.setAnimItem(it->handle().scheme_name,state))
    return false;

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

bool Inventory::equipNumSlot(Item *next, uint8_t slotHint, Npc &owner, bool force) {
  if(slotHint!=Item::NSLOT) {
    return setSlot(numslot[slotHint-3],next,owner,force);
    }

  for(auto& i:numslot) {
    if(i==nullptr) {
      setSlot(i,next,owner,force);
      return true;
      }
    }
  return false;
  }

void Inventory::applyArmor(Item &it, Npc &owner, int32_t sgn) {
  for(size_t i=0;i<PROT_MAX;++i){
    auto v = owner.protection(Protection(i));
    owner.changeProtection(Protection(i),v+it.handle().protection[i]*sgn);
    }
  }

bool Inventory::use(size_t cls, Npc &owner, uint8_t slotHint, bool force) {
  Item* it=findByClass(cls);
  if(it==nullptr)
    return false;

  auto& itData   = it->handle();
  auto  mainflag = ItmFlags(itData.main_flag);
  auto  flag     = ItmFlags(itData.flags);

  if(flag & ITM_SHIELD)
    return setSlot(shield,it,owner,force);

  if(mainflag & ITM_CAT_NF)
    return setSlot(melee,it,owner,force);

  if(mainflag & ITM_CAT_FF)
    return setSlot(range,it,owner,force);

  if(mainflag & ITM_CAT_RUNE) {
    if(it->isEquipped() && slotHint==it->slot())
      return false;
    if(it->isEquipped())
      unequip(it,owner);
    return equipNumSlot(it,slotHint,owner,force);
    }

  if(mainflag & ITM_CAT_ARMOR)
    return setSlot(armor,it,owner,force);

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

  bool deleteLater = false;
  if(flag & ITM_TORCH) {
    if(owner.weaponState()!=WeaponState::NoWeapon)
      return false;
    if(owner.toggleTorch()) {
      deleteLater = true;
      } else {
      return true;
      }
    }

  if(!owner.setAnimItem(itData.scheme_name,-1))
    return false;

  // owner.stopDlgAnim();
  setCurrentItem(it->clsId());
  if(itData.on_state[0]!=0){
    auto& vm = owner.world().script();
    vm.invokeItem(&owner,uint32_t(itData.on_state[0]));
    }

  if(deleteLater)
    owner.delItem(cls,1);

  return true;
  }

bool Inventory::equip(size_t cls, Npc &owner, bool force) {
  Item* it=findByClass(cls);
  if(it==nullptr || it->isEquipped())
    return false;
  return use(cls,owner,Item::NSLOT,force);
  }

void Inventory::invalidateCond(Npc &owner) {
  if(!owner.isPlayer())
    return; // gothic doesn't care
  invalidateCond(armor,owner);
  invalidateCond(belt  ,owner);
  invalidateCond(amulet,owner);
  invalidateCond(ringL ,owner);
  invalidateCond(ringR ,owner);
  invalidateCond(melee ,owner);
  invalidateCond(range ,owner);
  invalidateCond(shield,owner);
  for(auto& i:numslot)
    invalidateCond(i,owner);
  }

void Inventory::invalidateCond(Item *&slot, Npc &owner) {
  if(slot && !slot->checkCond(owner)) {
    unequip(slot,owner);
    }
  }

void Inventory::autoEquipWeapons(Npc &owner) {
  if(owner.isMonster())
    return;
  equipBestMeleeWeapon(owner);
  equipBestRangedWeapon(owner);
  }

void Inventory::equipArmor(int32_t cls, Npc &owner) {
  if(cls<=0)
    return;
  auto it = findByClass(size_t(cls));
  if(it==nullptr)
    return;
  if(uint32_t(it->mainFlag()) & ITM_CAT_ARMOR){
    if(!it->isEquipped())
      use(size_t(cls),owner,Item::NSLOT,true);
    }
  }

void Inventory::equipBestArmor(Npc &owner) {
  auto a = bestArmor(owner);
  if(a!=nullptr)
    setSlot(armor,a,owner,false);
  }

Item *Inventory::findByClass(size_t cls) {
  for(auto& i:items)
    if(i->clsId()==cls)
      return i.get();
  return nullptr;
  }

// Find the Nth item with a specific flag
// num starts at "1" for first item
Item* Inventory::findByFlags(ItmFlags f, uint32_t num) const {
  uint32_t found = 0;

  for(auto& i:items) {
    auto& itData = i->handle();
    auto  flag   = ItmFlags(itData.main_flag);
    if((flag & f)==0)
      continue;

    found++;
    if (found==num)
      return i.get();
    }
  return nullptr;
  }

Item* Inventory::bestItem(Npc &owner, ItmFlags f) {
  Item*   ret    = nullptr;
  int32_t value  = std::numeric_limits<int32_t>::min();
  int32_t damage = std::numeric_limits<int32_t>::min();
  for(auto& i:items) {
    auto& itData = i->handle();
    auto  flag   = ItmFlags(itData.main_flag);
    if((flag & f)==0)
      continue;
    if(!i->checkCond(owner))
      continue;
    if(itData.munition>0 && findByClass(size_t(itData.munition))==nullptr)
      continue;

    if(std::make_tuple(itData.damage_total, itData.value)>std::make_tuple(damage, value)){
      ret    = i.get();
      damage = itData.damage_total;
      value  = itData.value;
      }
    }
  return ret;
  }

Item *Inventory::bestArmor(Npc &owner) {
  return bestItem(owner,ITM_CAT_ARMOR);
  }

Item *Inventory::bestMeleeWeapon(Npc &owner) {
  return bestItem(owner,ITM_CAT_NF);
  }

Item *Inventory::bestRangedWeapon(Npc &owner) {
  return bestItem(owner,ITM_CAT_FF);
  }

void Inventory::applyWeaponStats(Npc& owner, const Item &weapon, int sgn) {
  auto& hnpc = owner.handle();
  //hnpc.damagetype = sgn>0 ? weapon.handle()->damageType : (1 << GEngineClasses::DAM_INDEX_BLUNT);
  for(size_t i=0; i<zenkit::DamageType::NUM; ++i){
    hnpc.damage[i] += sgn*weapon.handle().damage[i];
    if(weapon.handle().damage_type & (1<<i)) {
      hnpc.damage[i] += sgn*weapon.handle().damage_total;
      }
    }

  // assert inconsistent plus/minus
  for(size_t i=0; i<zenkit::DamageType::NUM; ++i) {
    assert(hnpc.damage[i]>=0);
    }
  }

void Inventory::sortItems() const {
  if(sorted)
    return;
  sorted = true;
  std::sort(items.begin(),items.end(),[](std::unique_ptr<Item>& l, std::unique_ptr<Item>& r){
    return less(*l,*r);
    });
  }

bool Inventory::less(const Item &il, const Item &ir) {
  auto ordL = orderId(il);
  auto ordR = orderId(ir);

  if(ordL<ordR)
    return true;
  if(ordL>ordR)
    return false;

  int32_t lV = 0, rV = 0;
  if(il.mainFlag() & (ItmFlags::ITM_CAT_FOOD | ItmFlags::ITM_CAT_POTION | ItmFlags::ITM_CAT_DOCS)) {
    lV = 0;
    rV = 0;
    } else {
    lV = il.cost();
    rV = ir.cost();
    }

  return std::make_tuple(il.mainFlag(), -il.handle().damage_total, -lV, -il.clsId())
      <  std::make_tuple(ir.mainFlag(), -ir.handle().damage_total, -rV, -ir.clsId());
  }

int Inventory::orderId(const Item& i) {
  auto& invCatOrder = Gothic::invCatOrder();
  auto  mflg        = ItmFlags(i.mainFlag());

  for(size_t i=0; i<invCatOrder.size(); ++i)
    if(mflg!=0 && (mflg&invCatOrder[i]))
      return int(i);
  return int(invCatOrder.size());
  }

uint8_t Inventory::slotId(Item *&slt) const {
  if(&slt==&melee)
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



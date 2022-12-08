#include "item.h"

#include <Tempest/Log>

#include "game/serialize.h"
#include "game/gamescript.h"
#include "game/inventory.h"
#include "world/objects/npc.h"
#include "world/world.h"
#include "utils/fileext.h"

using namespace Tempest;

Item::Item(World &owner, size_t itemInstance, Type type)
  :Vob(owner) {
  assert(itemInstance!=size_t(-1));
  hitem = std::make_shared<phoenix::c_item>();
  hitem->user_ptr=this;
  owner.script().initializeInstanceItem(hitem, itemInstance);
  setCount(1);

  if(type!=T_Inventory) {
    view = world.addView(*hitem);
    if(type==T_WorldDyn)
      setPhysicsEnable(view);
    }
  }

Item::Item(World &owner, Serialize &fin, Type type)
  :Vob(owner) {
  auto& h = hitem;
  h = std::make_shared<phoenix::c_item>();
  h->user_ptr = this;

  Tempest::Matrix4x4 mat;

  uint32_t instanceSymbol=0;
  fin.read(instanceSymbol);

  auto& vm = owner.script().getVm();
  auto* sym = vm.find_symbol_by_index(instanceSymbol);

  if (sym != nullptr) {
    vm.allocate_instance(h, sym);
    } else {
    Tempest::Log::e("Loading unknown item from save: " , instanceSymbol);
    }

  fin.read(h->id,h->name,h->name_id,h->hp,h->hp_max,h->main_flag);
  fin.read(reinterpret_cast<int&>(h->flags),h->weight,h->value,h->damage_type,h->damage_total,h->damage);
  fin.read(h->wear,h->protection,h->nutrition,h->cond_atr,h->cond_value,h->change_atr,h->change_value,h->magic);
  fin.read(h->on_equip,h->on_unequip,h->on_state);
  fin.read(h->owner,h->owner_guild,h->disguise_guild,h->visual,h->visual_change);
  fin.read(h->effect,h->visual_skin,h->scheme_name,h->material);
  fin.read(h->munition,h->spell,h->range,h->mag_circle);
  fin.read(h->description,h->text,h->count);
  fin.read(h->inv_zbias,h->inv_rot_x,h->inv_rot_y,h->inv_rot_z,h->inv_animate);
  fin.read(amount);
  fin.read(pos,equiped,itSlot);
  fin.read(mat);

  if(type!=T_Inventory) {
    if(!FileExt::hasExt(hitem->visual,"ZEN"))
      view = world.addView(*hitem);
    if(type==T_WorldDyn)
      setPhysicsEnable(view);
    }

  setLocalTransform(mat);
  view  .setObjMatrix(mat);
  physic.setObjMatrix(mat);
  }

Item::Item(Item &&it)
  : Vob(it.world), hitem(it.hitem),
    pos(it.pos),equiped(it.equiped),itSlot(it.itSlot),view(std::move(it.view)) {
  setLocalTransform(it.localTransform());
  physic = std::move(it.physic);
  }

Item::~Item() {
  }

void Item::save(Serialize &fout) const {
  auto& h = *hitem;
  fout.write(uint32_t(h.symbol_index()));
  fout.write(h.id,h.name,h.name_id,h.hp,h.hp_max,h.main_flag);
  fout.write(reinterpret_cast<int&>(h.flags),h.weight,h.value,h.damage_type,h.damage_total,h.damage);
  fout.write(h.wear,h.protection,h.nutrition,h.cond_atr,h.cond_value,h.change_atr,h.change_value,h.magic);
  fout.write(h.on_equip,h.on_unequip,h.on_state);
  fout.write(h.owner,h.owner_guild,h.disguise_guild,h.visual,h.visual_change);
  fout.write(h.effect,h.visual_skin,h.scheme_name,h.material);
  fout.write(h.munition,h.spell,h.range,h.mag_circle);
  fout.write(h.description,h.text,h.count);
  fout.write(h.inv_zbias,h.inv_rot_x,h.inv_rot_y,h.inv_rot_z,h.inv_animate);
  fout.write(amount);
  fout.write(pos,equiped,itSlot);
  fout.write(localTransform());
  }

void Item::clearView() {
  view = MeshObjects::Mesh();
  }

bool Item::isTorchBurn() const {
  return false;
  }

void Item::setPosition(float x, float y, float z) {
  pos={x,y,z};
  updateMatrix();
  }

void Item::setDirection(float, float, float) {
  }

void Item::setObjMatrix(const Tempest::Matrix4x4 &m) {
  pos.x = m.at(3,0);
  pos.y = m.at(3,1);
  pos.z = m.at(3,2);
  setLocalTransform(m);
  view.setObjMatrix(m);
  }

bool Item::isMission() const {
  return (uint32_t(hitem->flags)&ITM_MISSION);
  }

void Item::setAsEquiped(bool e) {
  if(e)
    ++equiped; else
    --equiped;
  if(equiped>amount) {
    Log::e("[",displayName(),"] inconsistent inventory state");
    }
  if(equiped==0)
    itSlot=NSLOT;
  }

void Item::setPhysicsEnable(World& world) {
  setPhysicsEnable(view);
  world.invalidateVobIndex();
  }

void Item::setPhysicsDisable() {
  physic = DynamicWorld::Item();
  world.invalidateVobIndex();
  }

void Item::setPhysicsEnable(const MeshObjects::Mesh& view) {
  if(view.nodesCount()==0)
    return;
  auto& p = *world.physic();
  physic = p.dynamicObj(transform(),view.bounds(),phoenix::material_group(hitem->material));
  physic.setItem(this);
  }

void Item::setPhysicsEnable(const ProtoMesh* mesh) {
  if(mesh==nullptr)
    return;
  auto& p = *world.physic();
  Bounds b;
  b.assign(mesh->bbox);
  physic = p.dynamicObj(transform(),b,phoenix::material_group(hitem->material));
  physic.setItem(this);
  }

bool Item::isDynamic() const {
  return !physic.isEmpty();
  }

std::string_view Item::displayName() const {
  return hitem->name;
  }

std::string_view Item::description() const {
  return hitem->description;
  }

Tempest::Vec3 Item::position() const {
  return pos;
  }

Vec3 Item::midPosition() const {
  auto b = view.bounds();
  return pos + (b.bbox[1]-b.bbox[0])*0.5;
  }

bool Item::isGold() const {
  return hitem->symbol_index()==world.script().goldId()->index();
  }

ItmFlags Item::mainFlag() const {
  return ItmFlags(hitem->main_flag);
  }

int32_t Item::itemFlag() const {
  return static_cast<int>(hitem->flags);
  }

bool Item::isMulti() const {
  return uint32_t(hitem->flags)&ITM_MULTI;
  }

bool Item::isSpellShoot() const {
  if(!isSpellOrRune())
    return false;
  auto& spl = world.script().spellDesc(spellId());
  return spl.target_collect_algo!=TargetCollect::TARGET_COLLECT_NONE &&
         spl.target_collect_algo!=TargetCollect::TARGET_COLLECT_CASTER &&
         spl.target_collect_algo!=TargetCollect::TARGET_COLLECT_FOCUS;
  }

bool Item::isSpellOrRune() const {
  return (uint32_t(mainFlag()) & ITM_CAT_RUNE);
  }

bool Item::isSpell() const {
  if(isSpellOrRune())
    return isMulti();
  return false;
  }

bool Item::isRune() const {
  return isSpellOrRune() && !isSpell();
  }

bool Item::is2H() const {
  auto flg = uint32_t(itemFlag());
  return flg & (ITM_2HD_SWD | ITM_2HD_AXE);
  }

bool Item::isCrossbow() const {
  auto flg = uint32_t(itemFlag());
  return flg & ITM_CROSSBOW;
  }

bool Item::isRing() const {
  auto flg = uint32_t(itemFlag());
  return flg & ITM_RING;
  }

bool Item::isArmour() const {
  auto flg = ItmFlags(mainFlag());
  return flg & ITM_CAT_ARMOR;
  }

int32_t Item::spellId() const {
  return hitem->spell;
  }

int32_t Item::swordLength() const {
  return hitem->range;
  }

void Item::setCount(size_t cnt) {
  amount = uint32_t(cnt);
  }

size_t Item::count() const {
  return amount;
  }

std::string_view Item::uiText(size_t id) const {
  return hitem->text[id];
  }

int32_t Item::uiValue(size_t id) const {
  return hitem->count[id];
  }

int32_t Item::cost() const {
  return hitem->value;
  }

int32_t Item::sellCost() const {
  return int32_t(std::ceil(world.script().tradeValueMultiplier()*float(cost())));
  }

bool Item::checkCond(const Npc &other) const {
  int32_t a=0,v=0;
  return checkCondUse(other,a,v) && checkCondRune(other,a,v);
  }

bool Item::checkCondUse(const Npc &other, int32_t &a, int32_t &nv) const {
  for(size_t i=0;i<phoenix::c_item::condition_count;++i){
    auto atr = Attribute(hitem->cond_atr[i]);
    if(other.attribute(atr)<hitem->cond_value[i] && hitem->cond_value[i]!=0) {
      a  = atr;
      nv = hitem->cond_value[i];
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
  return hitem->symbol_index();
  }

void Item::updateMatrix() {
  Tempest::Matrix4x4 mat;
  mat.identity();
  mat.translate(pos.x,pos.y,pos.z);
  setLocalTransform(mat);
  }

void Item::moveEvent() {
  view  .setObjMatrix(transform());
  physic.setObjMatrix(transform());
  if(!isDynamic())
    world.invalidateVobIndex();
  }

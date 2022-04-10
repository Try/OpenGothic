#include "itemtorchburning.h"

#include "world/world.h"

ItemTorchBurning::ItemTorchBurning(World& owner, size_t inst, Item::Type type)
  :Item(owner,inst,type) {
  auto& sc = owner.script();
  Daedalus::GEngineClasses::C_Item  hitem={};
  sc.initializeInstance(hitem,inst);
  sc.clearReferences(hitem);
  view.setVisual(hitem,owner,false);

  size_t torchId = sc.getSymbolIndex("ItLsTorchburned");
  if(torchId!=size_t(-1)) {
    Daedalus::GEngineClasses::C_Item  hitem={};
    sc.initializeInstance(hitem,torchId);
    sc.clearReferences(hitem);

    auto m = Resources::loadMesh(hitem.visual.c_str());
    setPhysicsEnable(m);
    }
  }

void ItemTorchBurning::clearView() {
  Item::clearView();
  view = ObjVisual();
  }

bool ItemTorchBurning::isTorchBurn() const {
  return true;
  }

void ItemTorchBurning::moveEvent() {
  Item::moveEvent();
  view.setObjMatrix(transform());
  }

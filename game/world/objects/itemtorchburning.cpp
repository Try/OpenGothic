#include "itemtorchburning.h"

#include "world/world.h"

ItemTorchBurning::ItemTorchBurning(World& owner, size_t inst, Item::Type type)
  :Item(owner,inst,type) {
  auto& sc = owner.script();

  auto hitem = std::make_shared<phoenix::c_item>();
  sc.initializeInstanceItem(hitem, inst);
  view.setVisual(*hitem,owner,false);

  size_t torchId = sc.getSymbolIndex("ItLsTorchburned");
  if(torchId!=size_t(-1)) {
    auto hitem = std::make_shared<phoenix::c_item>();
    sc.initializeInstanceItem(hitem, torchId);
    auto m = Resources::loadMesh(hitem->visual);
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

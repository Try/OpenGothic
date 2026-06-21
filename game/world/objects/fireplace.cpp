#include "fireplace.h"

#include <zenkit/vobs/MovableObject.hh>

FirePlace::FirePlace(Vob* parent, World& world, const zenkit::VFire& vob, Flags flags)
  : Interactive(parent,world,vob,flags){
  // NOTE: in original-game oCMobFire::oCMobFire (Gothic2.exe @0x00722460) the ctor seeds
  // fireSlot="BIP01 FIRE" and fireVobtreeName="FIRETREE_LARGE.ZEN"; the archiver's keyed
  // ReadString (Unarchive @0x00722d70) leaves those defaults in place when the ZEN omits the
  // keys. ZenKit's VFire::load returns "" instead, so a default-templated fire would render no
  // flame/smoke vobtree at all -- re-apply the defaults here.
  fireVobtreeName = vob.vob_tree;
  fireSlot        = vob.slot;
  if(fireVobtreeName.empty())
    fireVobtreeName = "FIRETREE_LARGE.ZEN";
  if(fireSlot.empty())
    fireSlot = "BIP01 FIRE";
  }

void FirePlace::load(Serialize& fin) {
  Interactive::load(fin);
  onStateChanged();
  }

void FirePlace::moveEvent() {
  Interactive::moveEvent();

  auto at = this->mapBone(fireSlot);
  fireVobtree.setObjMatrix(at);
  }

void FirePlace::onStateChanged() {
  if(stateId()>0) {
    auto at = this->mapBone(fireSlot);
    fireVobtree = VobBundle(world,fireVobtreeName,Vob::Startup);
    fireVobtree.setObjMatrix(at);
    } else {
    fireVobtree = VobBundle();
    }
  }

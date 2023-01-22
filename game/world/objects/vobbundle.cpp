#include "vobbundle.h"

#include "world/world.h"
#include "resources.h"

VobBundle::VobBundle(World& owner, std::string_view filename, Vob::Flags flags) {
  auto* bundle = Resources::loadVobBundle(filename);
  for(auto& vob:*bundle)
    rootVobs.emplace_back(Vob::load(nullptr,owner,*vob,(flags | Vob::Startup)));
  }

void VobBundle::setObjMatrix(const Tempest::Matrix4x4& obj) {
  for(auto& i:rootVobs)
    i->setLocalTransform(obj);
  }

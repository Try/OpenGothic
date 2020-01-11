#pragma once

#include "graphics/meshobjects.h"
#include "graphics/pfxobjects.h"
#include "physics/dynamicworld.h"

class StaticObj final {
  public:
    StaticObj(const ZenLoad::zCVobData &vob, World& owner);

  private:
    MeshObjects::Mesh        mesh;
    DynamicWorld::StaticItem physic;
    PfxObjects::Emitter      pfx;
  };


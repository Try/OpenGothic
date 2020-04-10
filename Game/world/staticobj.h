#pragma once

#include "graphics/meshobjects.h"
#include "graphics/pfxobjects.h"
#include "physics/physicmesh.h"

class StaticObj final {
  public:
    StaticObj(const ZenLoad::zCVobData &vob, World& owner);

  private:
    MeshObjects::Mesh          mesh;
    PhysicMesh                 physic;
    PfxObjects::Emitter        pfx;
    std::unique_ptr<ProtoMesh> decalMesh;
  };


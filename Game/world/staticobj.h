#pragma once

#include "graphics/meshobjects.h"
#include "graphics/pfxobjects.h"
#include "physics/physicmesh.h"
#include "vob.h"

class StaticObj : public Vob {
  public:
    StaticObj(Vob* parent, World& world, ZenLoad::zCVobData&& vob, bool startup);

  private:
    void  moveEvent() override;

    MeshObjects::Mesh          mesh;
    PhysicMesh                 physic;
    PfxObjects::Emitter        pfx;
    std::unique_ptr<ProtoMesh> decalMesh;
  };


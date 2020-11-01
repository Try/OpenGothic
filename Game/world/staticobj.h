#pragma once

#include "graphics/meshobjects.h"
#include "graphics/pfxobjects.h"
#include "graphics/mdlvisual.h"
#include "physics/physicmesh.h"
#include "vob.h"

class StaticObj : public Vob {
  public:
    StaticObj(Vob* parent, World& world, ZenLoad::zCVobData&& vob, bool startup);

  private:
    void  moveEvent() override;
    bool  setMobState(const char* scheme,int32_t st) override;

    PhysicMesh                 physic;
    PfxObjects::Emitter        pfx;

    MdlVisual                  visual;
    std::string                scheme;
  };


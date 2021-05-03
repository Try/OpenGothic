#pragma once

#include "world/objects/pfxemitter.h"
#include "world/objects/vobbundle.h"
#include "physics/physicmesh.h"
#include "graphics/mdlvisual.h"

class ObjVisual {
  public:
    ObjVisual();
    ObjVisual(ObjVisual&& other);
    ObjVisual& operator = (ObjVisual&& other);
    ~ObjVisual();

    void setVisual(const Daedalus::GEngineClasses::C_Item& visual, World& world);
    void setVisual(const ZenLoad::zCVobData& visual, World& world);
    void setObjMatrix(const Tempest::Matrix4x4& obj);

    const Animation::Sequence* startAnimAndGet(const char* name, uint64_t tickCount);

  private:
    enum Type : uint8_t {
      M_None = 0,
      M_Mdl,
      M_Mesh,
      M_Pfx,
      M_Bundle,
      };

    struct Mdl {
      PhysicMesh  physic;
      MdlVisual   view;
      };

    union {
      Mdl               mdl;
      MeshObjects::Mesh mesh;
      PfxEmitter        pfx;
      VobBundle         bundle;
      };
    Type type = M_None;

    void cleanup();
    void setType(Type t);
  };


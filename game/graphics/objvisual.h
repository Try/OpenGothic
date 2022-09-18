#pragma once

#include "world/objects/pfxemitter.h"
#include "world/objects/vobbundle.h"
#include "physics/physicmesh.h"
#include "graphics/mdlvisual.h"

class Npc;
class World;

class ObjVisual {
  public:
    ObjVisual();
    ObjVisual(ObjVisual&& other);
    ObjVisual& operator = (ObjVisual&& other);
    ~ObjVisual();

    void save(Serialize& fout, const Interactive& mob) const;
    void load(Serialize& fin,  Interactive &mob);

    void setVisual(const phoenix::c_item& visual, World& world, bool staticDraw);
    void setVisual(const phoenix::vob& visual, World& world, bool staticDraw);
    void setObjMatrix(const Tempest::Matrix4x4& obj);

    void setInteractive(Interactive* it);

    const Animation::Sequence* startAnimAndGet(std::string_view name, uint64_t tickCount, bool force = false);
    bool isAnimExist(std::string_view name) const;

    bool updateAnimation(Npc* npc, World& world, uint64_t dt);
    void processLayers(World& world);
    void syncPhysics();

    const ProtoMesh* protoMesh() const;
    const Tempest::Matrix4x4& bone(size_t i) const;

  private:
    enum Type : uint8_t {
      M_None = 0,
      M_Mdl,
      M_Mesh,
      M_Pfx,
      M_Bundle,
      };

    struct Mdl {
      PhysicMesh       physic;
      MdlVisual        view;
      const ProtoMesh* proto = nullptr;
      };

    struct Mesh {
      PhysicMesh        physic;
      MeshObjects::Mesh view;
      const ProtoMesh*  proto = nullptr;
      };

    union {
      Mdl        mdl;
      Mesh       mesh;
      PfxEmitter pfx;
      VobBundle  bundle;
      };
    Type type = M_None;

    void cleanup();
    void setType(Type t);
  };


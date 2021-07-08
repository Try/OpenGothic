#pragma once

#include <Tempest/Device>
#include <Tempest/Shader>

#include "physics/dynamicworld.h"
#include "graphics/mesh/landscape.h"
#include "graphics/meshobjects.h"
#include "graphics/mesh/protomesh.h"
#include "graphics/pfx/pfxobjects.h"
#include "lightsource.h"
#include "sceneglobals.h"
#include "visualobjects.h"

class World;
class ParticleFx;
class PackedMesh;

class WorldView {
  public:
    WorldView(const World &world, const PackedMesh& wmesh);
    ~WorldView();

    const LightSource& mainLight() const;
    bool isInPfxRange(const Tempest::Vec3& pos) const;

    void tick(uint64_t dt);

    void updateCmd (uint8_t frameId, const World &world,
                    const Tempest::Attachment& main, const Tempest::Attachment& shadow,
                    const Tempest::FrameBufferLayout &mainLay, const Tempest::FrameBufferLayout &shadowLay);
    void setViewProject (const Tempest::Matrix4x4& view, const Tempest::Matrix4x4& proj);
    void setModelView   (const Tempest::Matrix4x4& viewProj, const Tempest::Matrix4x4* shadow, size_t shCount);

    void setFrameGlobals(const Tempest::Texture2d* shadow[], uint64_t tickCount, uint8_t fId);
    void setGbuffer     (const Tempest::Texture2d& lightingBuf, const Tempest::Texture2d& diffuse, const Tempest::Texture2d& norm, const Tempest::Texture2d& depth);
    void setupUbo();

    void dbgLights    (DbgPainter& p) const;

    void visibilityPass(const Frustrum fr[], const Tempest::Pixmap& hiZ);
    void drawShadow    (Tempest::Encoder<Tempest::CommandBuffer> &cmd, uint8_t frameId, uint8_t layer);
    void drawGBufferOcc(Tempest::Encoder<Tempest::CommandBuffer> &cmd, uint8_t frameId);
    void drawGBuffer   (Tempest::Encoder<Tempest::CommandBuffer> &cmd, uint8_t frameId);
    void drawMain      (Tempest::Encoder<Tempest::CommandBuffer> &cmd, uint8_t frameId);
    void drawLights    (Tempest::Encoder<Tempest::CommandBuffer> &cmd, uint8_t frameId);

    MeshObjects::Mesh   addView      (std::string_view visual, int32_t headTex, int32_t teethTex, int32_t bodyColor);
    MeshObjects::Mesh   addView      (const ProtoMesh* visual);
    MeshObjects::Mesh   addItmView   (std::string_view visual, int32_t material);
    MeshObjects::Mesh   addAtachView (const ProtoMesh::Attach& visual, const int32_t version);
    MeshObjects::Mesh   addStaticView(const ProtoMesh* visual);
    MeshObjects::Mesh   addStaticView(std::string_view visual);
    MeshObjects::Mesh   addDecalView (const ZenLoad::zCVobData& vob);

  private:
    const World&  owner;

    SceneGlobals  sGlobal;
    VisualObjects visuals;

    MeshObjects   objGroup;
    PfxObjects    pfxGroup;
    Landscape     land;

    bool          needToUpdateUbo = false;

    bool needToUpdateCmd(uint8_t frameId) const;
    void invalidateCmd();

    void updateLight();

  friend class LightGroup::Light;
  friend class PfxEmitter;
  friend class TrlObjects;
  };

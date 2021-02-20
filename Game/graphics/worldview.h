#pragma once

#include <Tempest/Device>
#include <Tempest/Shader>

#include "physics/dynamicworld.h"

#include "graphics/sky/sky.h"
#include "graphics/mesh/landscape.h"
#include "graphics/meshobjects.h"
#include "graphics/pfx/pfxobjects.h"
#include "lightsource.h"
#include "sceneglobals.h"
#include "visualobjects.h"

class World;
class RendererStorage;
class ParticleFx;
class PackedMesh;
class Painter3d;

class WorldView {
  public:
    WorldView(const World &world, const PackedMesh& wmesh, const RendererStorage& storage);
    ~WorldView();

    const LightSource& mainLight() const;

    void tick(uint64_t dt);

    void updateCmd (uint8_t frameId, const World &world,
                    const Tempest::Attachment& main, const Tempest::Attachment& shadow,
                    const Tempest::FrameBufferLayout &mainLay, const Tempest::FrameBufferLayout &shadowLay);
    void setModelView   (const Tempest::Matrix4x4& viewProj, const Tempest::Matrix4x4* shadow, size_t shCount);
    void setFrameGlobals(const Tempest::Texture2d& shadow, uint64_t tickCount, uint8_t fId);
    void setGbuffer     (const Tempest::Texture2d& lightingBuf, const Tempest::Texture2d& diffuse, const Tempest::Texture2d& norm, const Tempest::Texture2d& depth);

    void dbgLights    (DbgPainter& p) const;
    void drawShadow   (Tempest::Encoder<Tempest::CommandBuffer> &cmd, Painter3d& painter, uint8_t frameId, uint8_t layer);
    void drawGBuffer  (Tempest::Encoder<Tempest::CommandBuffer> &cmd, Painter3d& painter, uint8_t frameId);
    void drawMain     (Tempest::Encoder<Tempest::CommandBuffer> &cmd, Painter3d& painter, uint8_t frameId);
    void drawLights   (Tempest::Encoder<Tempest::CommandBuffer> &cmd, Painter3d& painter, uint8_t frameId);
    void setupUbo     ();

    MeshObjects::Mesh   addView      (const char* visual, int32_t headTex, int32_t teethTex, int32_t bodyColor);
    MeshObjects::Mesh   addItmView   (const char* visual, int32_t material);
    MeshObjects::Mesh   addAtachView (const ProtoMesh::Attach& visual, const int32_t version);
    MeshObjects::Mesh   addStaticView(const char* visual);
    MeshObjects::Mesh   addDecalView (const ZenLoad::zCVobData& vob);

  private:
    const World&            owner;
    const RendererStorage&  storage;

    SceneGlobals            sGlobal;
    VisualObjects           visuals;

    MeshObjects             objGroup;
    PfxObjects              pfxGroup;
    Landscape               land;

    bool                    needToUpdateUbo = false;

    bool needToUpdateCmd(uint8_t frameId) const;
    void invalidateCmd();

    void updateLight();

  friend class LightGroup::Light;
  friend class PfxEmitter;
  friend class TrlObjects;
  };

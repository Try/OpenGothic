#pragma once

#include <Tempest/Device>
#include <Tempest/Shader>

#include "physics/dynamicworld.h"

#include "graphics/sky/sky.h"
#include "graphics/meshobjects.h"
#include "graphics/landscape.h"
#include "graphics/meshobjects.h"
#include "graphics/pfxobjects.h"
#include "light.h"
#include "sceneglobals.h"

class World;
class RendererStorage;
class ParticleFx;
class PackedMesh;
class Painter3d;

class WorldView {
  public:
    WorldView(const World &world, const PackedMesh& wmesh, const RendererStorage& storage);
    ~WorldView();

    void initPipeline(uint32_t w, uint32_t h);

    Tempest::Matrix4x4        viewProj(const Tempest::Matrix4x4 &view) const;
    const Tempest::Matrix4x4& projective() const { return proj; }
    const Light&              mainLight() const;

    void tick(uint64_t dt);

    void addLight(const ZenLoad::zCVobData &vob);

    void updateCmd (uint8_t frameId, const World &world,
                    const Tempest::Attachment& main, const Tempest::Attachment& shadow,
                    const Tempest::FrameBufferLayout &mainLay, const Tempest::FrameBufferLayout &shadowLay);
    void setModelView   (const Tempest::Matrix4x4 &view, const Tempest::Matrix4x4* shadow, size_t shCount);
    void setFrameGlobals(const Tempest::Texture2d& shadow, uint64_t tickCount, uint8_t fId);

    void drawShadow(Tempest::Encoder<Tempest::PrimaryCommandBuffer> &cmd, Painter3d& painter, uint8_t frameId, uint8_t layer);
    void drawMain  (Tempest::Encoder<Tempest::PrimaryCommandBuffer> &cmd, Painter3d& painter, uint8_t frameId);
    void resetCmd  ();

    MeshObjects::Mesh   getLand(Tempest::VertexBuffer<Resources::Vertex>& vbo,
                                Tempest::IndexBuffer<uint32_t>&           ibo,
                                const Material&                           mat,
                                const Bounds&                             bbox);
    MeshObjects::Mesh   getView      (const char* visual, int32_t headTex, int32_t teethTex, int32_t bodyColor);
    MeshObjects::Mesh   getItmView   (const char* visual, int32_t material);
    MeshObjects::Mesh   getAtachView (const ProtoMesh::Attach& visual);
    MeshObjects::Mesh   getStaticView(const char* visual);
    MeshObjects::Mesh   getDecalView (const ZenLoad::zCVobData& vob, const Tempest::Matrix4x4& obj, ProtoMesh& out);
    PfxObjects::Emitter getView      (const ParticleFx* decl);
    PfxObjects::Emitter getView      (const Tempest::Texture2d* spr, const ZenLoad::zCVobData& vob);

  private:
    const World&            owner;
    const RendererStorage&  storage;

    SceneGlobals            sGlobal;
    Sky                     sky;
    MeshObjects             objGroup;
    PfxObjects              pfxGroup;
    Landscape               land;

    std::vector<Light>      pendingLights;

    const Tempest::FrameBufferLayout* mainLay   = nullptr;
    const Tempest::FrameBufferLayout* shadowLay = nullptr;

    Tempest::Matrix4x4      proj;
    uint32_t                vpWidth=0;
    uint32_t                vpHeight=0;

    bool needToUpdateCmd(uint8_t frameId) const;
    void invalidateCmd();

    void updateLight();
    void setupSunDir(float pulse,float ang);
  };

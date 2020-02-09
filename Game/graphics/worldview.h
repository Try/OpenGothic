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

class World;
class RendererStorage;
class ParticleFx;
class PackedMesh;

class WorldView {
  public:
    WorldView(const World &world, const PackedMesh& wmesh, const RendererStorage& storage);
    ~WorldView();

    void initPipeline(uint32_t w, uint32_t h);

    Tempest::Matrix4x4        viewProj(const Tempest::Matrix4x4 &view) const;
    const Tempest::Matrix4x4& projective() const { return proj; }
    const Light&              mainLight() const;

    void tick(uint64_t dt);

    bool needToUpdateCmd() const;
    void updateCmd (uint32_t frameId, const World &world,
                    const Tempest::Attachment& main, const Tempest::Attachment& shadow,
                    const Tempest::FrameBufferLayout &mainLay, const Tempest::FrameBufferLayout &shadowLay);
    void updateUbo (uint32_t frameId, const Tempest::Matrix4x4 &view, const Tempest::Matrix4x4* shadow, size_t shCount);
    void drawShadow(Tempest::Encoder<Tempest::PrimaryCommandBuffer> &cmd, uint8_t frameId, uint8_t layer);
    void drawMain  (Tempest::Encoder<Tempest::PrimaryCommandBuffer> &cmd, uint8_t frameId);
    void resetCmd  ();

    MeshObjects::Mesh   getView      (const char* visual, int32_t headTex, int32_t teethTex, int32_t bodyColor);
    MeshObjects::Mesh   getItmView   (const char* visual, int32_t material);
    MeshObjects::Mesh   getStaticView(const char* visual);
    PfxObjects::Emitter getView      (const ParticleFx* decl);

  private:
    const World&            owner;
    const RendererStorage&  storage;

    Light                   sun;
    Tempest::Vec3           ambient;

    Sky                     sky;
    Landscape               land;
    MeshObjects             vobGroup;
    MeshObjects             objGroup;
    MeshObjects             itmGroup;
    PfxObjects              pfxGroup;

    bool                    nToUpdateCmd=true;
    const Tempest::FrameBufferLayout* mainLay   = nullptr;
    const Tempest::FrameBufferLayout* shadowLay = nullptr;

    Tempest::Matrix4x4      proj;
    uint32_t                vpWidth=0;
    uint32_t                vpHeight=0;

    struct PerFrame {
      Tempest::CommandBuffer cmdMain;
      Tempest::CommandBuffer cmdShadow[2];
      bool                   actual     =true;
      };
    std::unique_ptr<PerFrame[]> frame;

    void updateLight();
    void setupSunDir(float pulse,float ang);
    void builtCmdBuf(uint32_t frameId, const World &world,
                     const Tempest::Attachment& main,
                     const Tempest::Attachment& shadowMap,
                     const Tempest::FrameBufferLayout &mainLay,
                     const Tempest::FrameBufferLayout &shadowLay);
  };

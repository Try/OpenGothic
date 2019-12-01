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

class WorldView {
  public:
    WorldView(const World &world, const ZenLoad::PackedMesh& wmesh, const RendererStorage& storage);
    ~WorldView();

    void initPipeline(uint32_t w, uint32_t h);

    Tempest::Matrix4x4        viewProj(const Tempest::Matrix4x4 &view) const;
    const Tempest::Matrix4x4& projective() const { return proj; }
    const Light&              mainLight() const;

    void tick(uint64_t dt);

    bool needToUpdateCmd() const;
    void updateCmd (const World &world, const Tempest::Texture2d &shadow,
                    const Tempest::FrameBufferLayout &mainLay, const Tempest::FrameBufferLayout &shadowLay);
    void updateUbo (const Tempest::Matrix4x4 &view, const Tempest::Matrix4x4 *shadow, size_t shCount);
    void drawShadow(Tempest::Encoder<Tempest::PrimaryCommandBuffer> &cmd, uint8_t layer);
    void drawMain  (Tempest::Encoder<Tempest::PrimaryCommandBuffer> &cmd);
    void resetCmd  ();

    MeshObjects::Mesh   getView      (const std::string& visual, int32_t headTex, int32_t teethTex, int32_t bodyColor);
    MeshObjects::Mesh   getStaticView(const std::string& visual, int32_t material);
    PfxObjects::Emitter getView      (const ParticleFx* decl);

    void addStatic(const ZenLoad::zCVobData &vob);
    void addPfx   (const ZenLoad::zCVobData &vob);

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

    struct StaticObj {
      MeshObjects::Mesh        mesh;
      DynamicWorld::StaticItem physic;
      PfxObjects::Emitter      pfx;
      };

    struct PerFrame {
      Tempest::CommandBuffer cmdMain;
      Tempest::CommandBuffer cmdShadow[2];
      bool                   actual     =true;
      };
    std::unique_ptr<PerFrame[]> frame;
    std::vector<StaticObj>      objStatic;

    void setupSunDir(float pulse,float ang);
    void builtCmdBuf(const World &world, const Tempest::Texture2d &shadowMap,
                     const Tempest::FrameBufferLayout &mainLay,
                     const Tempest::FrameBufferLayout &shadowLay);
  };

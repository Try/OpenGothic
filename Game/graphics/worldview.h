#pragma once

#include <Tempest/Device>
#include <Tempest/Shader>

#include "physics/dynamicworld.h"

#include "graphics/sky/sky.h"
#include "graphics/staticobjects.h"
#include "graphics/landscape.h"

class World;
class RendererStorage;

class WorldView {
  public:
    WorldView(const World &world,const RendererStorage& storage);

    void initPipeline(uint32_t w, uint32_t h);
    Tempest::Matrix4x4  viewProj(const Tempest::Matrix4x4 &view) const;
    const Tempest::Matrix4x4& projective() const { return proj; }

    void updateCmd(const World &world);
    void updateUbo(const Tempest::Matrix4x4 &view, uint32_t imgId);
    void draw     (Tempest::CommandBuffer &cmd, Tempest::FrameBuffer &fbo);

    StaticObjects::Mesh getView(const std::string& visual, int32_t headTex, int32_t teethTex, int32_t bodyColor);

  private:
    const RendererStorage&  storage;

    Sky                     sky;
    Landscape               land;
    StaticObjects           vobGroup;
    StaticObjects           objGroup;
    bool                    nToUpdateCmd=true;

    Tempest::Matrix4x4      proj;

    struct StaticObj {
      StaticObjects::Mesh mesh;
      DynamicWorld::Item  physic;
      };

    std::vector<Tempest::CommandBuffer> cmdLand;
    std::vector<StaticObj>              objStatic;

    void prebuiltCmdBuf(const World &world);
  };

#pragma once

#include <Tempest/Device>
#include <Tempest/Shader>

#include "graphics/staticobjects.h"
#include "graphics/landscape.h"

class World;
class RendererStorage;

class WorldView {
  public:
    WorldView(const World &world,const RendererStorage& storage);

    void initPipeline(uint32_t w, uint32_t h);

    void updateCmd(const World &world);
    void updateUbo(const Tempest::Matrix4x4 &view, uint32_t imgId);
    void draw     (Tempest::CommandBuffer &cmd, Tempest::FrameBuffer &fbo);

    StaticObjects::Mesh getView(const std::string& visual);

  private:
    const RendererStorage&  storage;

    Landscape               land;
    StaticObjects           vobGroup;
    StaticObjects           objGroup;
    bool                    nToUpdateCmd=true;

    Tempest::Matrix4x4      projective;

    std::vector<Tempest::CommandBuffer> cmdLand;
    std::vector<StaticObjects::Mesh>    objStatic;

    void prebuiltCmdBuf(const World &world);
  };

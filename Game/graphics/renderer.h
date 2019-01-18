#pragma once

#include <Tempest/RenderPipeline>
#include <Tempest/RenderPass>
#include <Tempest/CommandBuffer>
#include <Tempest/Matrix4x4>
#include <Tempest/Widget>
#include <Tempest/Device>
#include <Tempest/UniformBuffer>

#include "landscape.h"
#include "staticobjects.h"

class Gothic;
class StaticMesh;

class Renderer final {
  public:
    Renderer(Tempest::Device& device, Gothic &gothic);

    void initSwapchain(uint32_t w,uint32_t h);

    void setDebugView(const Tempest::PointF& spin,const float zoom);
    void draw(Tempest::CommandBuffer& cmd, uint32_t imgId, const Gothic& gothic);
    void draw(Tempest::CommandBuffer& cmd, Tempest::FrameBuffer& imgId, const Gothic& gothic);

    Tempest::CommandBuffer &probuilt();

  private:
    struct Object {
      Tempest::Matrix4x4 objMat;
      StaticObjects::Obj obj;
      };
    std::vector<Object> objStatic;

    Tempest::RenderPipeline& landPipeline(Tempest::RenderPass& pass, uint32_t w, uint32_t h);
    Tempest::RenderPipeline& objPipeline (Tempest::RenderPass& pass, uint32_t w, uint32_t h);

    Tempest::Device&        device;
    Gothic&                 gothic;
    Tempest::Texture2d      zbuffer;
    Tempest::Matrix4x4      view,projective;
    size_t                  isUboReady=0;
    bool                    needToUpdateUbo[3]={};

    Tempest::RenderPass     mainPass;
    Tempest::RenderPipeline pLand, pObject;
    std::vector<Tempest::FrameBuffer> fbo3d;

    Tempest::Shader         vsLand,fsLand,vsObject,fsObject;

    Landscape                      land;
    StaticObjects                  vobGroup;

    std::vector<Tempest::CommandBuffer> cmdLand;

    Tempest::PointF         spin;
    float                   zoom=1.f;

    void initWorld();
    void prebuiltCmdBuf();

    void updateUbo  (const Tempest::FrameBuffer &fbo, const Gothic &gothic, uint32_t imgId);
  };

#pragma once

#include <Tempest/RenderPipeline>
#include <Tempest/RenderPass>
#include <Tempest/CommandBuffer>
#include <Tempest/Widget>
#include <Tempest/Device>

class Gothic;

class Renderer final {
  public:
    Renderer(Tempest::Device& device);

    Tempest::RenderPipeline& landPipeline(Tempest::RenderPass& pass, int w, int h);

    void draw(Tempest::CommandBuffer& cmd,const Tempest::Widget& window,const Gothic& gothic);

  private:
    Tempest::Device&        device;

    Tempest::RenderPass     mainPass;
    Tempest::RenderPipeline pLand;

    Tempest::Shader         vsLand,fsLand;
  };

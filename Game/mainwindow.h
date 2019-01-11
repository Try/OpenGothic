#pragma once

#include "resources.h"

#include <Tempest/Window>
#include <Tempest/CommandBuffer>
#include <Tempest/Fence>
#include <Tempest/Semaphore>
#include <Tempest/VulkanApi>
#include <Tempest/Device>
#include <Tempest/VertexBuffer>
#include <Tempest/UniformsLayout>
#include <Tempest/UniformBuffer>
#include <Tempest/VectorImage>
#include <Tempest/Event>
#include <Tempest/Pixmap>
#include <Tempest/Sprite>
#include <Tempest/Font>
#include <Tempest/TextureAtlas>

#include <daedalus/DaedalusVM.h>

#include <vector>

#include "resources.h"

class Gothic;

class MainWindow : public Tempest::Window {
  public:
    explicit MainWindow(Gothic& gothic,Tempest::VulkanApi& api);
    ~MainWindow() override;

  private:
    void paintEvent    (Tempest::PaintEvent& event) override;
    void resizeEvent   (Tempest::SizeEvent & event) override;
    void mouseMoveEvent(Tempest::MouseEvent& event) override;

    void setupUi();

    void render() override;
    void initSwapchain();

    Tempest::Device       device;
    Tempest::TextureAtlas atlas;
    Resources             resources;

    Tempest::Font         font;
    Tempest::Sprite       spr;

    Tempest::RenderPass  mainPass;
    Tempest::VectorImage surface;

    Tempest::Point       mpos;

    struct FrameLocal {
      explicit FrameLocal(Tempest::Device& dev):imageAvailable(dev),gpuLock(dev){}
      Tempest::Semaphore imageAvailable;
      Tempest::Fence     gpuLock;
      };

    std::vector<FrameLocal>             fLocal;

    std::vector<Tempest::CommandBuffer> commandDynamic;
    std::vector<Tempest::Semaphore>     commandBuffersSemaphores;

    std::vector<Tempest::FrameBuffer>   fbo;

    Gothic&                             gothic;
    Tempest::Texture2d                  background;
  };

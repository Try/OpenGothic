#pragma once

#include "camera.h"
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
#include <Tempest/Timer>

#include <daedalus/DaedalusVM.h>

#include <vector>

#include "game/playercontrol.h"
#include "graphics/renderer.h"
#include "resources.h"

class MenuRoot;
class Gothic;

class MainWindow : public Tempest::Window {
  public:
    explicit MainWindow(Gothic& gothic,Tempest::VulkanApi& api);
    ~MainWindow() override;

  private:
    void paintEvent    (Tempest::PaintEvent& event) override;
    void resizeEvent   (Tempest::SizeEvent & event) override;

    void mouseDownEvent (Tempest::MouseEvent& event) override;
    void mouseDragEvent (Tempest::MouseEvent& event) override;
    void mouseWheelEvent(Tempest::MouseEvent& event) override;

    void keyDownEvent   (Tempest::KeyEvent&   event) override;
    void keyUpEvent     (Tempest::KeyEvent&   event) override;

    void setWorld(const std::string& name);

    void setupUi();

    void render() override;
    void initSwapchain();

    void tick();

    Tempest::Point        mpos;
    Tempest::PointF       spin;
    Camera                camera;
    PlayerControl         player;
    uint64_t              lastTick=0;

    Tempest::Device       device;
    Tempest::TextureAtlas atlas;
    Resources             resources;

    Tempest::Font         font;

    Tempest::RenderPass  uiPass;
    Tempest::VectorImage surface;

    Renderer             draw;

    MenuRoot*            rootMenu=nullptr;
    Tempest::Timer       timer;

    struct FrameLocal {
      explicit FrameLocal(Tempest::Device& dev):imageAvailable(dev),gpuLock(dev){}
      Tempest::Semaphore imageAvailable;
      Tempest::Fence     gpuLock;
      };

    std::vector<FrameLocal>             fLocal;

    std::vector<Tempest::CommandBuffer> commandDynamic;
    std::vector<Tempest::Semaphore>     commandBuffersSemaphores;

    std::vector<Tempest::FrameBuffer>   fboUi;

    Gothic&                             gothic;
    const Tempest::Texture2d*           background=nullptr;
    bool                                pressed[Tempest::KeyEvent::K_Last]={};

    struct Fps {
      uint64_t dt[10]={};
      double   get() const;
      void     push(uint64_t t);
      };
    Fps fps;
  };

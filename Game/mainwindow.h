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
#include <thread>

#include "world/world.h"
#include "world/focus.h"
#include "game/playercontrol.h"
#include "graphics/renderer.h"
#include "resources.h"
#include "ui/dialogmenu.h"
#include "ui/inventorymenu.h"
#include "ui/chapterscreen.h"
#include "ui/documentmenu.h"

class MenuRoot;
class Gothic;
class GameSession;
class Interactive;

class MainWindow : public Tempest::Window {
  public:
    explicit MainWindow(Gothic& gothic, Tempest::VulkanApi& api);
    ~MainWindow() override;

  private:
    void paintEvent    (Tempest::PaintEvent& event) override;
    void resizeEvent   (Tempest::SizeEvent & event) override;

    void mouseDownEvent (Tempest::MouseEvent& event) override;
    void mouseUpEvent   (Tempest::MouseEvent& event) override;
    void mouseDragEvent (Tempest::MouseEvent& event) override;
    void mouseMoveEvent (Tempest::MouseEvent& event) override;
    void mouseWheelEvent(Tempest::MouseEvent& event) override;

    void keyDownEvent   (Tempest::KeyEvent&   event) override;
    void keyUpEvent     (Tempest::KeyEvent&   event) override;

    void drawBar(Tempest::Painter& p, const Tempest::Texture2d *bar, int x, int y, float v, Tempest::AlignFlag flg);   
    void drawProgress(Tempest::Painter& p, int x, int y, int w, int h, float v);
    void drawLoading (Tempest::Painter& p,int x,int y,int w,int h);
    void drawSaving  (Tempest::Painter& p);

    void startGame(const std::string& name);
    void loadGame (const std::string& name);
    void saveGame (const std::string& name);

    void onWorldLoaded();
    void onSessionExit();
    void setGameImpl(std::unique_ptr<GameSession>&& w);
    void clearInput();
    void setFullscreen(bool fs);
    void processMouse(Tempest::MouseEvent& event, bool fs);

    void setupUi();

    void render() override;

    void tick();
    Camera::Mode solveCameraMode() const;

    Tempest::Device       device;
    Tempest::TextureAtlas atlas;
    Resources             resources;

    Tempest::Font         font;
    Tempest::VectorImage  surface;

    Renderer              renderer;

    MenuRoot*             rootMenu=nullptr;
    Tempest::Timer        timer;

    struct FrameLocal {
      explicit FrameLocal(Tempest::Device& dev):imageAvailable(dev.semaphore()),renderDone(dev.semaphore()),gpuLock(dev.fence()){}
      Tempest::Semaphore imageAvailable;
      Tempest::Semaphore renderDone;
      Tempest::Fence     gpuLock;
      };

    std::vector<FrameLocal>             fLocal;

    std::vector<Tempest::PrimaryCommandBuffer> commandDynamic;

    Gothic&                             gothic;

    const Tempest::Texture2d*           background=nullptr;
    const Tempest::Texture2d*           loadBox=nullptr;
    const Tempest::Texture2d*           loadVal=nullptr;

    const Tempest::Texture2d*           barBack=nullptr;
    const Tempest::Texture2d*           barHp  =nullptr;
    const Tempest::Texture2d*           barMana=nullptr;

    const Tempest::Texture2d*           saveback=nullptr;

    bool                                mouseP[Tempest::MouseEvent::ButtonBack]={};
    bool                                pressed[Tempest::KeyEvent::K_Last]={};

    InventoryMenu                       inventory;
    DialogMenu                          dialogs;
    DocumentMenu                        document;
    ChapterScreen                       chapter;

    Tempest::Widget*                    uiKeyUp=nullptr;
    Tempest::Point                      mpos;
    Tempest::PointF                     spin;
    Camera                              camera;
    PlayerControl                       player;
    Focus                               currentFocus;
    uint64_t                            lastTick=0;

    struct Fps {
      uint64_t dt[10]={};
      double   get() const;
      void     push(uint64_t t);
      };
    Fps fps;
  };

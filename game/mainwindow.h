/**
 * @file mainwindow.h
 * @brief Main application window for OpenGothic
 */

#pragma once

#include "camera.h"
#include "resources.h"

#include <Tempest/Window>
#include <Tempest/CommandBuffer>
#include <Tempest/Fence>
#include <Tempest/VulkanApi>
#include <Tempest/Device>
#include <Tempest/VertexBuffer>
#include <Tempest/UniformBuffer>
#include <Tempest/VectorImage>
#include <Tempest/Event>
#include <Tempest/Pixmap>
#include <Tempest/Sprite>
#include <Tempest/Font>
#include <Tempest/TextureAtlas>
#include <Tempest/Timer>
#include <Tempest/Swapchain>

#include <vector>
#include <thread>

#include "world/world.h"
#include "world/focus.h"
#include "game/playercontrol.h"
#include "graphics/renderer.h"
#include "ui/dialogmenu.h"
#include "ui/inventorymenu.h"
#include "ui/chapterscreen.h"
#include "ui/documentmenu.h"
#include "ui/videowidget.h"
#include "ui/menuroot.h"
#include "ui/consolewidget.h"
#include "ui/touchinput.h"

#include "utils/keycodec.h"
#include "resources.h"

class MenuRoot;
class GameSession;
class Interactive;

/**
 * @brief Main application window handling game rendering and input
 *
 * MainWindow is the primary window class that manages the game's rendering loop,
 * user input handling, UI composition, and game state transitions. It coordinates
 * between the renderer, game session, and various UI components (menus, dialogs,
 * inventory, etc.).
 */
class MainWindow : public Tempest::Window {
  public:
    /**
     * @brief Constructs the main window
     * @param device The graphics device used for rendering
     */
    explicit MainWindow(Tempest::Device& device);

    ~MainWindow() override;

    /**
     * @brief Returns the current UI scaling factor
     * @return Scale factor based on system DPI settings
     */
    float uiScale() const;

  private:
    /// @name Event Handlers
    /// @{
    void paintEvent     (Tempest::PaintEvent& event) override;
    void resizeEvent    (Tempest::SizeEvent & event) override;

    void mouseDownEvent (Tempest::MouseEvent& event) override;
    void mouseUpEvent   (Tempest::MouseEvent& event) override;
    void mouseDragEvent (Tempest::MouseEvent& event) override;
    void mouseMoveEvent (Tempest::MouseEvent& event) override;
    void mouseWheelEvent(Tempest::MouseEvent& event) override;

    void keyDownEvent   (Tempest::KeyEvent&   event) override;
    void keyRepeatEvent (Tempest::KeyEvent&   event) override;
    void keyUpEvent     (Tempest::KeyEvent&   event) override;

    void focusEvent     (Tempest::FocusEvent&  event) override;
    /// @}

    /// @name Drawing Helpers
    /// @{
    void paintFocus     (Tempest::Painter& p, const Focus& fc, const Tempest::Matrix4x4& vp);
    void paintFocus     (Tempest::Painter& p, Tempest::Rect rect);

    void drawBar(Tempest::Painter& p, const Tempest::Texture2d *bar, int x, int y, float v, Tempest::AlignFlag flg);
    void drawMsg(Tempest::Painter& p);
    void drawProgress(Tempest::Painter& p, int x, int y, int w, int h, float v);
    void drawLoading (Tempest::Painter& p,int x,int y,int w,int h);
    void drawSaving  (Tempest::Painter& p);
    void drawSaving  (Tempest::Painter& p, const Tempest::Texture2d& back, int w, int h, float scale);
    /// @}

    /// @name Game Session Control
    /// @{
    void startGame(std::string_view slot);                        ///< Start a new game from the given world slot
    void loadGame (std::string_view slot);                        ///< Load a saved game from the given slot
    void saveGame (std::string_view slot, std::string_view name); ///< Save current game to slot
    /// @}

    void onVideo(std::string_view fname);
    void onStartLoading();
    void onWorldLoaded();
    void onSessionExit();
    void onBenchmarkFinished();
    void setGameImpl(std::unique_ptr<GameSession>&& w);
    void clearInput();
    void setFullscreen(bool fs);

    void processMouse(Tempest::MouseEvent& event, bool enable);
    void tickMouse();
    void onSettings();

    void setupUi();

    void render() override;

    uint64_t tick();
    void     updateAnimation(uint64_t dt);
    void     tickCamera(uint64_t dt);
    void     isDialogClosed(bool& ret);

    template<Tempest::KeyEvent::KeyType k>
    void     onMarvinKey();

    Camera::Mode solveCameraMode() const;

    /**
     * @brief Runtime execution mode for debugging
     */
    enum RuntimeMode : uint8_t {
      R_Normal,     ///< Normal game execution
      R_Suspended,  ///< Game logic paused (Marvin mode)
      R_Step,       ///< Single-step execution
      };

    Tempest::Device&      device;
    Tempest::Swapchain    swapchain;
    Tempest::TextureAtlas atlas;
    Tempest::Font         font;
    Renderer              renderer;

    Tempest::VectorImage  uiLayer, numOverlay;
    Tempest::VectorImage::Mesh uiMesh [Resources::MaxFramesInFlight];
    Tempest::VectorImage::Mesh numMesh[Resources::MaxFramesInFlight];

    Tempest::Fence         fence   [Resources::MaxFramesInFlight];
    Tempest::CommandBuffer commands[Resources::MaxFramesInFlight];
    uint8_t                cmdId = 0;

    Tempest::Texture2d        background;
    const Tempest::Texture2d* loadBox=nullptr;
    const Tempest::Texture2d* loadVal=nullptr;

    const Tempest::Texture2d* barBack=nullptr;
    const Tempest::Texture2d* barHp  =nullptr;
    const Tempest::Texture2d* barMisc=nullptr;
    const Tempest::Texture2d* barMana=nullptr;

    const Tempest::Texture2d* focusImg=nullptr;

    const Tempest::Texture2d* saveback=nullptr;

    bool                      mouseP[Tempest::MouseEvent::ButtonBack]={};

    KeyCodec                  keycodec;

    MenuRoot                  rootMenu;
    VideoWidget               video;
    InventoryMenu             inventory;
    DialogMenu                dialogs;
    DocumentMenu              document;
    ChapterScreen             chapter;
    ConsoleWidget             console;
#if defined(__MOBILE_PLATFORM__)
    TouchInput                mobileUi;
#endif
    RuntimeMode               runtimeMode = R_Normal;

    Tempest::Widget*          uiKeyUp=nullptr;
    Tempest::Point            dMouse;
    PlayerControl             player;
    uint64_t                  lastTick=0;

    Tempest::Shortcut         funcKey[11];
    Tempest::Shortcut         displayPos;

    /**
     * @brief Stores benchmark metrics for performance analysis
     */
    struct BenchmarkData {
      std::vector<uint64_t> low1procent;  ///< Frame times for lowest 1% calculation
      size_t                numFrames = 0;
      double                fpsSum = 0;
      void                  push(uint64_t t);
      void                  clear();
      };

    /**
     * @brief Rolling FPS counter using last 10 frame times
     */
    struct Fps {
      uint64_t dt[10]={};        ///< Ring buffer of frame delta times
      double   get() const;      ///< Calculate average FPS
      void     push(uint64_t t);
      };

    Fps           fps;           ///< Current FPS tracker
    BenchmarkData benchmark;     ///< Benchmark data collector
    uint64_t      maxFpsInv = 0; ///< Inverse of max FPS limit (ms per frame)
  };

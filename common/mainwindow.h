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
#include <unordered_map>
#include <unordered_set>
#include <thread>

#include "world/world.h"
#include "world/focus.h"
#include "game/playercontrol.h"
#include "graphics/renderer.h"
#include "ui/dialogmenu.h"
#include "ui/inventorymenu.h"
#include "ui/arpgmenu.h"
#include "ui/arpgworldmap.h"
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

class MainWindow : public Tempest::Window {
  public:
    explicit MainWindow(Tempest::Device& device);
    ~MainWindow() override;

    float uiScale() const;

  private:
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

    void paintFocus     (Tempest::Painter& p, const Focus& fc, const Tempest::Matrix4x4& vp);
    void paintFocus     (Tempest::Painter& p, Tempest::Rect rect);
    void paintArpgLabels(Tempest::Painter& p, const Tempest::Matrix4x4& vp, const Focus& hover);
    void paintArpgFocus (Tempest::Painter& p, const Focus& fc, const Tempest::Matrix4x4& vp);
    void paintArpgCombo (Tempest::Painter& p, const Tempest::Matrix4x4& vp);
    void paintArpgVitals(Tempest::Painter& p);
    void paintArpgMinimap(Tempest::Painter& p);
    void paintArpgCombat(Tempest::Painter& p, const Tempest::Matrix4x4& vp);
    void paintArpgBar   (Tempest::Painter& p);
    void paintArpgBarOverlay(Tempest::Painter& p);
    bool arpgBarVisible () const;
    Tempest::Rect arpgBarSlot(int i) const;
    bool arpgLabelRect  (const Focus& f, const Tempest::Matrix4x4& vp, Tempest::Rect& out) const;
    void drawArpgLabel  (Tempest::Painter& p, const Tempest::Rect& r, const Focus& f, bool hi);
    Focus arpgLabelAt   (const Tempest::Point& pos) const;

    void drawBar(Tempest::Painter& p, const Tempest::Texture2d *bar, int x, int y, float v, Tempest::AlignFlag flg);
    void drawMsg(Tempest::Painter& p);
    void drawDiathicBar(Tempest::Painter& p, const Tempest::Rect& r, float v);
    void drawLoadingScreen(Tempest::Painter& p);
    void drawSaving  (Tempest::Painter& p);
    std::string loadingTip() const;

    void startGame(std::string_view slot);
    void loadGame (std::string_view slot);
    void saveGame (std::string_view slot, std::string_view name);

    void onVideo(std::string_view fname);
    void onStartLoading();
    void onWorldLoaded();
    void onSessionExit();
    void onBenchmarkFinished();
    void setGameImpl(std::unique_ptr<GameSession>&& w);
    void clearInput();
    void setFullscreen(bool fs);

    void processMouse(Tempest::MouseEvent& event, bool enable);
    void tickMouse(uint64_t dt);
    void updateArpgAim();
    bool isArpgMouse() const;
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

    enum RuntimeMode : uint8_t {
      R_Normal,
      R_Suspended,
      R_Step,
      };

    Tempest::Device&      device;
    Tempest::Swapchain    swapchain;
    Tempest::TextureAtlas atlas;
    Shaders               shaders;
    Renderer              renderer;

    Tempest::VectorImage  uiLayer, numOverlay;
    Tempest::VectorImage::Mesh uiMesh [Resources::MaxFramesInFlight];
    Tempest::VectorImage::Mesh numMesh[Resources::MaxFramesInFlight];

    Tempest::Fence         fence   [Resources::MaxFramesInFlight];
    Tempest::CommandBuffer commands[Resources::MaxFramesInFlight];
    uint8_t                cmdId = 0;

    Tempest::Texture2d        background;
    Tempest::Texture2d        diathicBg;                // Diathic: menu_bg.jpg for the loading screens
    bool                      diathicBgTried = false;
    const Tempest::Texture2d* diathicBackground();
    float                     loadShown   = 0.f;        // the loading bar, following the progress smoothly
    uint64_t                  loadShownAt = 0;
    std::string               loadTip;                  // the hint of this loading screen
    std::string               loadWorldHint;            // the world of a new game / of the loaded save

    const Tempest::Texture2d* barBack=nullptr;
    const Tempest::Texture2d* barHp  =nullptr;
    const Tempest::Texture2d* barMisc=nullptr;
    const Tempest::Texture2d* barMana=nullptr;

    const Tempest::Texture2d* focusImg=nullptr;

    bool                      mouseP[Tempest::MouseEvent::ButtonBack]={};

    KeyCodec                  keycodec;

    MenuRoot                  rootMenu;
    VideoWidget               video;
    InventoryMenu             inventory;
    InventoryRenderer         arpgBarItems;        // gothic-diablo: item models on the skill bar and in the ARPG window
    InventoryRenderer         arpgCursorItem;      // Diathic: the sword cursor
    static constexpr const char* ArpgCursorModel = "ItMw_030_1h_PAL_Sword_02.3DS"; // Miecz paladyna (ITMW_1H_PAL_SWORD)
    static constexpr bool     ArpgCursorFlip = false;  // true: the other end of the model is the tip
    void                      paintArpgCursor();
    void                      paintMenuBackground(Tempest::Painter& p); // Diathic: picture + embers
    void                      paintCover (Tempest::Painter& p, const Tempest::Texture2d& tex);
    void                      paintEmbers(Tempest::Painter& p, size_t count);
    struct MenuEmber { float x = 0, y = 0, vx = 0, vy = 0, size = 1, life = 0; };
    std::vector<MenuEmber>    menuEmbers;
    uint64_t                  menuEmbersAt = 0;
    ArpgMenu                  arpgMenu;            // gothic-diablo: equipment / character / skill trees (I, C, T)
    LightGroup::Light         arpgDlgLight;        // gothic-diablo: soft light on the faces in dialog shots
    const World*              arpgDlgLightWorld = nullptr;
    bool                      arpgDlgLightOn    = false;
    void                      updateArpgDialogLight(const Camera& camera);
    // gothic-diablo: remarks in passing (a line said outside a talk): a subtitle over the head
    struct ArpgRemark {
      const Npc*  npc   = nullptr;
      std::string text;
      uint64_t    start = 0;
      uint64_t    until = 0;
      };
    std::vector<ArpgRemark>   arpgRemarks;
    void                      onArpgSpeech(Npc& npc, std::string_view text, uint64_t ms, bool scripted);
    void                      paintArpgRemarks(Tempest::Painter& p, const Tempest::Matrix4x4& vp);
    void                      dropArpgDialogLight();
    Tempest::KeyEvent::KeyType arpgMenuKey = Tempest::KeyEvent::K_NoKey; // pressed: acted on when released
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
    Tempest::Point            arpgCursor;          // gothic-diablo: visible mouse cursor (window pixels)
    bool                      arpgCursorValid = false;
    bool                      arpgLeftDown    = false;
    bool                      arpgRightDown   = false;
    bool                      arpgMidDown     = false; // middle button: drag = rotate the camera, click = original binding
    bool                      arpgMidRotating = false;
    bool                      arpgMidBlock    = false; // middle button held as a block
    bool                      arpgMidPush     = false; // middle button with a bow: push (release does nothing)
    Tempest::Point            arpgMidStart;
    Tempest::Point            arpgMidLast;
    bool                      arpgShowAll     = false; // Z held: labels of all items/usable objects in sight
    bool                      uiWasActive     = false;
    const Npc*                arpgLevelHero   = nullptr; // gothic-diablo: the hero whose level is watched
    int32_t                   arpgLevelSeen   = 0;       // ... and his level last frame (a level up = a skill point)
    // gothic-diablo: minimap (Tab), map texture from the world map item of the scripts, explored 4 m cells
    bool                      arpgMinimap     = false;
    bool                      arpgOverlay     = false;  // Diathic: the explored map over the game (Tab, second time)
    float                     arpgHpGhost     = -1.f;   // Diathic: trail of the life / mana just lost
    float                     arpgMpGhost     = -1.f;
    uint64_t                  arpgHpHit       = 0;
    uint64_t                  arpgMpHit       = 0;
    uint64_t                  arpgVitalsAt    = 0;
    Tempest::KeyEvent::KeyType arpgMapKey     = Tempest::KeyEvent::K_NoKey; // M / Esc pressed for the big map
    ArpgWorldMap              arpgWorldMap;
    bool                      arpgTabDown     = false;
    bool                      arpgIDown       = false;
    bool                      arpgLogDown     = false;  // Diathic: the diary key (ArpgKeys::Log)
    std::string               arpgMapWorld;
    std::string               arpgMapTex;
    Tempest::Rect             arpgMapBounds;
    bool                      arpgMapValid    = false;
    float                     arpgMapHalf     = 3500.f; // minimap: cm from the center to the edge (zoom)
    Tempest::Rect             arpgMapRect;              // where the minimap was drawn last frame
    struct ArpgLabel {
      Tempest::Rect           rect;
      Focus                   focus;
      };
    std::vector<ArpgLabel>    arpgLabels;
    std::vector<Focus>        arpgLabelObjs;
    PlayerControl             player;
    uint64_t                  lastTick=0;

    Tempest::Shortcut         funcKey[11];
    Tempest::Shortcut         displayPos;

    struct BenchmarkData {
      std::vector<uint64_t> low1procent;
      size_t                numFrames = 0;
      double                fpsSum = 0;
      void                  push(uint64_t t);
      void                  clear();
      };
    struct Fps {
      uint64_t dt[10]={};
      double   get() const;
      void     push(uint64_t t);
      };
    Fps           fps;
    BenchmarkData benchmark;
    uint64_t      maxFpsInv = 0;
  };

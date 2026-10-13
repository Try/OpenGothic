#include "mainwindow.h"

#include <Tempest/Except>
#include <Tempest/Painter>

#include <Tempest/Brush>
#include <Tempest/Pen>
#include <Tempest/Layout>
#include <Tempest/Application>
#include <Tempest/Log>

#include "ui/dialogmenu.h"
#include "ui/menuroot.h"
#include "ui/stacklayout.h"
#include "ui/videowidget.h"

#include "utils/mouseutil.h"
#include "utils/string_frm.h"
#include "world/triggers/abstracttrigger.h"
#include "world/objects/npc.h"
#include "game/serialize.h"
#include "game/globaleffects.h"
#include "utils/gthfont.h"
#include "utils/dbgpainter.h"

#include "commandline.h"
#include "gothic.h"

using namespace Tempest;

MainWindow::MainWindow(Device& device)
  : Window(Maximized),device(device),swapchain(device,hwnd()),
    atlas(device),rootMenu(keycodec),inventory(keycodec),
    dialogs(inventory),document(keycodec),
    console(*this),
#if defined(__MOBILE_PLATFORM__)
    mobileUi(player),
#endif
    player(dialogs,inventory) {
  Gothic::inst().onSettingsChanged.bind(this,&MainWindow::onSettings);
  onSettings();

  if(Gothic::inst().version().game==2)
    setWindowTitle("Gothic II"); else
    setWindowTitle("Gothic");

  if(!CommandLine::inst().isWindowMode()) {
    setFullscreen(true);
    }

  //renderer.resetSwapchain();
  setupUi();

  barBack    = Resources::loadTexture("BAR_BACK.TGA");
  barHp      = Resources::loadTexture("BAR_HEALTH.TGA");
  barMisc    = Resources::loadTexture("BAR_MISC.TGA");
  barMana    = Resources::loadTexture("BAR_MANA.TGA");

  focusImg   = Resources::loadTexture("FOCUS_HIGHLIGHT.TGA");

  loadBox    = Resources::loadTexture("PROGRESS.TGA");
  loadVal    = Resources::loadTexture("PROGRESS_BAR.TGA");

  Gothic::inst().onStartGame   .bind(this,&MainWindow::startGame);
  Gothic::inst().onLoadGame    .bind(this,&MainWindow::loadGame);
  Gothic::inst().onSaveGame    .bind(this,&MainWindow::saveGame);

  Gothic::inst().onStartLoading.bind(this,&MainWindow::onStartLoading);
  Gothic::inst().onWorldLoaded .bind(this,&MainWindow::onWorldLoaded);
  Gothic::inst().onSessionExit .bind(this,&MainWindow::onSessionExit);

  Gothic::inst().onVideo       .bind(this,&MainWindow::onVideo);

  Gothic::inst().onBenchmarkFinished.bind(this,&MainWindow::onBenchmarkFinished);

  if(!Gothic::inst().defaultSave().empty()){
    Gothic::inst().load(Gothic::inst().defaultSave());
    rootMenu.popMenu();
    }
  else if(!CommandLine::inst().doStartMenu()) {
    startGame(Gothic::inst().defaultWorld());
    rootMenu.popMenu();
    }
  else {
    rootMenu.processMusicTheme();
    }

  funcKey[2] = Shortcut(*this,Event::M_NoModifier,Event::K_F2);
  funcKey[2].onActivated.bind(this, &MainWindow::onMarvinKey<Event::K_F2>);

  funcKey[3] = Shortcut(*this,Event::M_NoModifier,Event::K_F3);
  funcKey[3].onActivated.bind(this, &MainWindow::onMarvinKey<Event::K_F3>);

  funcKey[4] = Shortcut(*this,Event::M_NoModifier,Event::K_F4);
  funcKey[4].onActivated.bind(this, &MainWindow::onMarvinKey<Event::K_F4>);

  funcKey[5] = Shortcut(*this,Event::M_NoModifier,Event::K_F5);
  funcKey[5].onActivated.bind(this, &MainWindow::onMarvinKey<Event::K_F5>);

  funcKey[6] = Shortcut(*this,Event::M_NoModifier,Event::K_F6);
  funcKey[6].onActivated.bind(this, &MainWindow::onMarvinKey<Event::K_F6>);

  funcKey[7] = Shortcut(*this,Event::M_NoModifier,Event::K_F7);
  funcKey[7].onActivated.bind(this, &MainWindow::onMarvinKey<Event::K_F7>);

  funcKey[9] = Shortcut(*this,Event::M_NoModifier,Event::K_F9);
  funcKey[9].onActivated.bind(this, &MainWindow::onMarvinKey<Event::K_F9>);

  funcKey[10] = Shortcut(*this,Event::M_NoModifier,Event::K_F10);
  funcKey[10].onActivated.bind(this, &MainWindow::onMarvinKey<Event::K_F10>);

  displayPos = Shortcut(*this,Event::M_Alt,Event::K_P);
  displayPos.onActivated.bind(this, &MainWindow::onMarvinKey<Event::K_P>);
  }

MainWindow::~MainWindow() {
  GameMusic::inst().stopMusic();
  Gothic::inst().cancelLoading();
  device.waitIdle();
  takeWidget(&dialogs);
  takeWidget(&inventory);
  takeWidget(&chapter);
  takeWidget(&document);
  takeWidget(&video);
  takeWidget(&rootMenu);
#if defined(__MOBILE_PLATFORM__)
  takeWidget(&mobileUi);
#endif
  removeAllWidgets();
  // unload
  Gothic::inst().setGame(std::unique_ptr<GameSession>());
  }

float MainWindow::uiScale() const {
  return SystemApi::uiScale(hwnd());
  }

void MainWindow::setupUi() {
  setLayout(new StackLayout());
  addWidget(&document);
  addWidget(&dialogs);
  addWidget(&inventory);
  addWidget(&chapter);
  addWidget(&video);
  addWidget(&rootMenu);
#if defined(__MOBILE_PLATFORM__)
  addWidget(&mobileUi);
#endif

  rootMenu.setMainMenu();

  Gothic::inst().onDialogPipe  .bind(&dialogs,&DialogMenu::openPipe);
  Gothic::inst().isNpcInDialogFn = std::bind(&DialogMenu::isNpcInDialog, &dialogs, std::placeholders::_1);

  Gothic::inst().onPrintScreen .bind(&dialogs,&DialogMenu::printScreen);
  Gothic::inst().onPrint       .bind(&dialogs,&DialogMenu::print);

  Gothic::inst().onIntroChapter.bind(&chapter, &ChapterScreen::show);
  Gothic::inst().onShowDocument.bind(&document,&DocumentMenu::show);
  }

void MainWindow::paintEvent(PaintEvent& event) {
  Painter p(event);
  auto world = Gothic::inst().world();
  auto st    = Gothic::inst().checkLoading();

  if(!Gothic::inst().isInGame() && st==Gothic::LoadState::Idle && background.isEmpty()) {
    background = Resources::loadTextureUncached("STARTSCREEN.TGA");
    }

  if(world==nullptr && !background.isEmpty()) {
    p.setBrush(Color(0.0));
    p.drawRect(0,0,w(),h());

    if(st==Gothic::LoadState::Idle) {
      p.setBrush(Brush(background,Painter::NoBlend));
      p.drawRect(0,0,w(),h(),
                 0,0,background.w(),background.h());
      }
    }

  if(world!=nullptr) {
    world->globalFx()->scrBlend(p,Rect(0,0,w(),h()));
    }

  if(st!=Gothic::LoadState::Idle && st!=Gothic::LoadState::Finalize) {
    if(st==Gothic::LoadState::Saving) {
      drawSaving(p);
      } else {
      if(auto back = Gothic::inst().loadingBanner()) {
        p.setBrush(Brush(*back,Painter::NoBlend));
        p.drawRect(0,0,this->w(),this->h(),
                   0,0,back->w(),back->h());
        }
      if(loadBox!=nullptr && !loadBox->isEmpty()) {
        if(Gothic::inst().version().game==1) {
          int lw = int(w()*0.5);
          int lh = int(h()*0.05);
          drawLoading(p,(w()-lw)/2, int(h()*0.75), lw, lh);
          } else {
          drawLoading(p,int(w()*0.92)-loadBox->w(), int(h()*0.12), loadBox->w(),loadBox->h());
          }
        }
      }
    } else {
    if(world!=nullptr && world->view()){
      auto& camera = *Gothic::inst().camera();

      auto vp = camera.viewProj();
      p.setBrush(Color(1.0));

      drawMsg(p);

      auto focus = world->validateFocus(player.focus());
      if(isArpgMouse()) {
        // gothic-diablo: labels like in Diablo; Z = labels of everything in sight
        if(arpgShowAll)
          paintArpgLabels(p,vp,focus); else
          arpgLabels.clear();
        if(!arpgShowAll || focus.npc!=nullptr)
          paintArpgFocus(p,focus,vp);
        paintArpgCombat(p,vp);
        paintArpgCombo(p,vp);
        paintArpgMinimap(p);
        } else {
        arpgLabels.clear();
        paintFocus(p,focus,vp);
        }

      if(auto pl = Gothic::inst().player()){
        if (!Gothic::inst().isDesktop()) {
          auto& opt = Gothic::options();
          float hp  = float(pl->attribute(ATR_HITPOINTS))/float(pl->attribute(ATR_HITPOINTSMAX));
          float mp  = float(pl->attribute(ATR_MANA))     /float(pl->attribute(ATR_MANAMAX));

          bool showHealthBar = opt.showHealthBar;
          bool showManaBar   = (opt.showManaBar==2) || (opt.showManaBar==1 && (pl->weaponState()==WeaponState::Mage || inventory.isActive()));
          bool showSwimBar   = (opt.showSwimBar==2) || (opt.showSwimBar==1 && pl->isDive());

          if(showHealthBar)
            drawBar(p,barHp, 10, h()-10, hp, AlignLeft | AlignBottom);
          if(showManaBar)
            drawBar(p,barMana, w()-10, h()-10, mp, AlignRight | AlignBottom);
          if(showSwimBar) {
            uint32_t gl = pl->guild();
            auto     v  = float(pl->world().script().guildVal().dive_time[gl]);
            if(v>0) {
              auto t = float(pl->diveTime())/1000.f;
              drawBar(p,barMisc,w()/2,h()-10, (v-t)/(v), AlignHCenter | AlignBottom);
              }
            }
          }
        }
      }
    }

  if(auto c = Gothic::inst().camera()) {
    DbgPainter dbg(p,c->viewProj(),w(),h());
    c->debugDraw(dbg);
    if(world!=nullptr) {
      world->marchPoints(dbg);
      world->marchInteractives(dbg);
      world->view()->dbgLights(dbg);
      world->marchCsCameras(dbg);
      }
    }

  renderer.dbgDraw(p);

  const float scale = Gothic::interfaceScale(this);
  if(Gothic::inst().doFrate() && !Gothic::inst().isDesktop()) {
    char fpsT[64]={};
    std::snprintf(fpsT,sizeof(fpsT),"fps = %.2f",fps.get());

    auto& fnt = Resources::font(scale);
    fnt.drawText(p,5,fnt.pixelSize()+5,fpsT);
    }

  if(!Gothic::inst().isDesktop() && world!=nullptr) {
    if(Gothic::inst().doClock()) {
      auto hour = world->time().hour();
      auto min  = world->time().minute();
      auto& fnt = Resources::font(scale);
      string_frm clockT(int(hour),":",int(min));
      fnt.drawText(p,w()-fnt.textSize(clockT).w-5,fnt.pixelSize()+5,clockT);
      }

    auto c = Gothic::inst().camera();
    if(Gothic::inst().doVobBox() && c!=nullptr) {
      DbgPainter dbg(p,c->viewProj(),w(),h());
      world->drawVobBoxNpcNear(dbg);
      }

    if(Gothic::inst().doVobRays() && c!=nullptr) {
      DbgPainter dbg(p,c->viewProj(),w(),h());
      player.drawVobRay(dbg);
      }
    }

  if(auto wx = Gothic::inst().worldView()) {
    wx->dbgClusters(p, Vec2(float(w()), float(h())));
    }
  }

void MainWindow::resizeEvent(SizeEvent&) {
  for(auto& i:fence)
    i.wait();
  swapchain.reset();
  if(auto camera = Gothic::inst().camera())
    camera->setViewport(swapchain.w(),swapchain.h());

  if(SystemApi::isFullscreen(hwnd()) && !Camera::isArpgEnabled()) {
    setCursorPosition(Point(w()/2,h()/2));
    }

  dMouse = Point();
  }

void MainWindow::mouseDownEvent(MouseEvent &event) {
  if(event.button==Event::ButtonLeft && isArpgMouse()) {
    // gothic-diablo: left button = move / interact / attack under the cursor
    arpgCursor      = event.pos();
    arpgCursorValid = true;
    arpgLeftDown    = true;
    updateArpgAim();
    player.tickFocus();
    if(arpgShowAll) {
      // click on a label picks that object even if something else is under the cursor
      auto w = Gothic::inst().world();
      auto f = arpgLabelAt(event.pos());
      if(w!=nullptr && f)
        f = w->validateFocus(f);
      if(f) {
        player.arpgMouseDown(f);
        return;
        }
      }
    player.arpgMouseDown();
    return;
    }
  if(event.button==Event::ButtonMid && isArpgMouse() && player.arpgCanBlock()) {
    // gothic-diablo: melee weapon in hand: middle button held = block
    arpgMidBlock = true;
    player.arpgBlock(true);
    return;
    }
  if(event.button==Event::ButtonMid && isArpgMouse()) {
    // gothic-diablo: middle button drag = rotate the camera; a plain click keeps its binding
    arpgMidDown     = true;
    arpgMidRotating = false;
    arpgMidStart    = event.pos();
    arpgMidLast     = event.pos();
    return;
    }
  if(event.button==Event::ButtonRight && isArpgMouse()) {
    // gothic-diablo: right button held = charge the special attack, release = strike (dodge is on Space)
    arpgRightDown = true;
    updateArpgAim();
    player.tickFocus();
    player.arpgChargeStart();
    return;
    }
  if(event.button<sizeof(mouseP))
    mouseP[event.button]=true;
  auto act     = keycodec.tr(event);
  auto mapping = keycodec.mapping(event);
  player.onKeyPressed(act,KeyEvent::K_NoKey,mapping);
  }

void MainWindow::mouseUpEvent(MouseEvent &event) {
  if(event.button==Event::ButtonLeft && arpgLeftDown) {
    arpgLeftDown = false;
    player.arpgMouseUp();
    return;
    }
  if(event.button==Event::ButtonRight && arpgRightDown) {
    arpgRightDown = false;
    player.arpgChargeRelease();
    return;
    }
  if(event.button==Event::ButtonMid && arpgMidBlock) {
    arpgMidBlock = false;
    player.arpgBlock(false);
    return;
    }
  if(event.button==Event::ButtonMid && arpgMidDown) {
    arpgMidDown = false;
    if(!arpgMidRotating) {
      auto act     = keycodec.tr(event);
      auto mapping = keycodec.mapping(event);
      player.onKeyPressed (act,KeyEvent::K_NoKey,mapping);
      player.onKeyReleased(act,mapping);
      }
    arpgMidRotating = false;
    return;
    }
  auto act     = keycodec.tr(event);
  auto mapping = keycodec.mapping(event);
  player.onKeyReleased(act,mapping);
  if(event.button<sizeof(mouseP))
    mouseP[event.button]=false;
  }

void MainWindow::mouseDragEvent(MouseEvent &event) {
  if(Camera::isArpgEnabled()) {
    if(arpgMidDown) {
      if(!arpgMidRotating && std::abs(event.x-arpgMidStart.x)>6)
        arpgMidRotating = true;
      if(arpgMidRotating) {
        if(auto c = Gothic::inst().camera())
          c->arpgRotate(-float(event.x-arpgMidLast.x)*0.3f);
        }
      arpgMidLast = event.pos();
      }
    arpgCursor      = event.pos();
    arpgCursorValid = true;
    return;
    }
  const bool fs = SystemApi::isFullscreen(hwnd());
  if(!mouseP[Event::ButtonLeft] && !fs)
    return;
  if(player.focus().npc && !fs)
    return;
  processMouse(event,true);
  }

void MainWindow::mouseMoveEvent(MouseEvent &event) {
  processMouse(event,SystemApi::isFullscreen(hwnd()));
  }

void MainWindow::processMouse(MouseEvent& event, bool enable) {
  if(Camera::isArpgEnabled()) {
    // gothic-diablo: free, visible cursor; the hero turns towards it
    arpgCursor      = event.pos();
    arpgCursorValid = true;
    return;
    }
  auto center = Point(w()/2,h()/2);
  if(enable && event.pos()!=center && hasFocus()) {
    dMouse += (event.pos()-center);
    setCursorPosition(center);
    }
  }

void MainWindow::tickMouse(uint64_t dt) {
  auto camera = Gothic::inst().camera();
  if(dialogs.hasContent() || Gothic::inst().isPause() || camera==nullptr || camera->isCutscene()) {
    dMouse = Point();
    return;
    }

  const bool enableMouse = Gothic::inst().settingsGetI("GAME","enableMouse");
  if(enableMouse==0) {
    dMouse = Point();
    return;
    }

  if(dMouse==Point())
    return;

  const bool  camLookaroundInverse = Gothic::inst().settingsGetI("GAME","camLookaroundInverse");
  const float mouseSensitivity     = Gothic::inst().settingsGetF("GAME","mouseSensitivity")/MouseUtil::mouseSysSpeed();
  PointF dpScaled = PointF(float(dMouse.x)*mouseSensitivity,float(dMouse.y)*mouseSensitivity);
  dpScaled.x/=float(w());
  dpScaled.y/=float(h());
  if(camLookaroundInverse)
    dpScaled.y *= -1.f;

  static float mul = 270.f;
  dpScaled *= mul;

  static float psMax = 720.f;
  const float  dtF   = float(dt)/1000.f;
  dpScaled.x = std::clamp(dpScaled.x, -(psMax*dtF), psMax*dtF);
  dpScaled.y = std::clamp(dpScaled.y, -(psMax*dtF), psMax*dtF);

  // Log::d("mouse dMouse   = ", dMouse.x,   ", ", dMouse.y);
  // Log::d("mouse dpScaled = ", dpScaled.x, ", ", dpScaled.y);

  camera->onRotateMouse(PointF(dpScaled.y,-dpScaled.x));
  if(!inventory.isActive()) {
    player.onRotateMouse(-dpScaled.x, -dpScaled.y);
    }

  dMouse = Point();
  }

void MainWindow::updateArpgAim() {
  // gothic-diablo: world ray under the mouse cursor, for picking and click-to-move
  PlayerControl::ArpgCursor cur;
  auto camera = Gothic::inst().camera();
  auto pl     = Gothic::inst().player();
  if(!Camera::isArpgEnabled() || !arpgCursorValid || camera==nullptr || pl==nullptr ||
     !camera->isArpgActive() || w()<=0 || h()<=0) {
    player.setArpgCursor(cur);
    return;
    }

  const float x = (float(arpgCursor.x)+0.5f)/float(w())*2.f - 1.f;
  const float y = (float(arpgCursor.y)+0.5f)/float(h())*2.f - 1.f;

  const auto vp  = camera->viewProj();
  auto       inv = vp;
  inv.inverse();
  Vec4 p0 = {x, y, 0.f, 1.f};
  Vec4 p1 = {x, y, 1.f, 1.f};
  inv.project(p0);
  inv.project(p1);
  if(p0.w==0.f || p1.w==0.f) {
    player.setArpgCursor(cur);
    return;
    }

  cur.valid    = true;
  cur.rayFrom  = Vec3(p0.x, p0.y, p0.z)/p0.w;
  cur.rayTo    = Vec3(p1.x, p1.y, p1.z)/p1.w;
  cur.viewProj = vp;
  cur.pixel    = Vec2(float(arpgCursor.x), float(arpgCursor.y));
  cur.screen   = Vec2(float(w()), float(h()));
  player.setArpgCursor(cur);
  }

bool MainWindow::isArpgMouse() const {
  auto camera = Gothic::inst().camera();
  return Camera::isArpgEnabled() && camera!=nullptr && camera->isArpgActive() &&
         Gothic::inst().player()!=nullptr &&
         !dialogs.isActive() && !inventory.isActive() && !document.isActive();
  }

void MainWindow::onSettings() {
  auto zMaxFps = Gothic::options().fpsLimit;
  if(zMaxFps<=0)
    zMaxFps = Gothic::inst().settingsGetI("ENGINE", "zMaxFps");
  if(zMaxFps>0)
    maxFpsInv = 1000u/uint64_t(zMaxFps); else
    maxFpsInv = 0;
  }

void MainWindow::mouseWheelEvent(MouseEvent &event) {
  if(arpgMinimap && arpgMapRect.w>0 && arpgMapRect.contains(event.pos()) && isArpgMouse()) {
    // gothic-diablo: wheel over the minimap zooms the minimap (20..100 m across)
    const float k = event.delta>0 ? 1.f/1.2f : 1.2f;
    arpgMapHalf = std::clamp(arpgMapHalf*k, 1000.f, 5000.f);
    return;
    }
  if(auto camera = Gothic::inst().camera())
    camera->changeZoom(event.delta);
  }

void MainWindow::keyDownEvent(KeyEvent &event) {
  if(video.isActive()){
    event.accept();
    video.keyDownEvent(event);
    if(event.isAccepted()){
      uiKeyUp=&video;
      return;
      }
    }

  if(rootMenu.isActive()) {
    event.accept();
    rootMenu.keyDownEvent(event);
    if(event.isAccepted()){
      uiKeyUp=&rootMenu;
      return;
      }
    }

  if(chapter.isActive()){
    event.accept();
    chapter.keyDownEvent(event);
    if(event.isAccepted()){
      uiKeyUp=&chapter;
      return;
      }
    }

  if(document.isActive()){
    event.accept();
    document.keyDownEvent(event);
    if(event.isAccepted()){
      uiKeyUp=&document;
      return;
      }
    }

  if(dialogs.isActive()){
    event.accept();
    dialogs.keyDownEvent(event);
    if(event.isAccepted()){
      uiKeyUp=&dialogs;
      return;
      }
    }

  if(inventory.isActive()){
    event.accept();
    inventory.keyDownEvent(event);
    if(event.isAccepted()){
      uiKeyUp=&inventory;
      return;
      }
    }
  uiKeyUp=nullptr;

  if(event.key==Event::K_Z && Camera::isArpgEnabled())
    arpgShowAll = true; // gothic-diablo: hold Z to show all items / usable objects in sight

  if(Camera::isArpgEnabled() && Gothic::inst().player()!=nullptr) {
    // gothic-diablo: Tab = minimap, I = inventory (Backspace keeps working too)
    if(event.key==Event::K_Tab) {
      arpgMinimap = !arpgMinimap;
      arpgTabDown = true;
      event.accept();
      return;
      }
    if(event.key==Event::K_I) {
      arpgIDown = true;
      event.accept();
      return;
      }
    }

  auto act     = keycodec.tr(event);
  auto mapping = keycodec.mapping(event);
  player.onKeyPressed(act,event.key,mapping);

  if(event.key==Event::K_F11) {
    auto tex = renderer.screenshoot(cmdId, Size(int(swapchain.w()),int(swapchain.h())));
    auto pm  = device.readPixels(textureCast<const Texture2d&>(tex));
    pm.save("dbg.png");
    }
  event.accept();
  }

void MainWindow::keyRepeatEvent(KeyEvent& event) {
  if(uiKeyUp==&video){
    if(event.isAccepted())
      return;
    }
  if(uiKeyUp==&rootMenu){
    rootMenu.keyRepeatEvent(event);
    if(event.isAccepted())
      return;
    }
  if(uiKeyUp==&chapter){
    if(event.isAccepted())
      return;
    }
  if(uiKeyUp==&document){
    if(event.isAccepted())
      return;
    }
  if(uiKeyUp==&dialogs){
    if(event.isAccepted())
      return;
    }
  if(uiKeyUp==&inventory){
    inventory.keyRepeatEvent(event);
    if(event.isAccepted())
      return;
    }
  }

void MainWindow::keyUpEvent(KeyEvent &event) {
  if(event.key==Event::K_Z)
    arpgShowAll = false;
  if(event.key==Event::K_Tab && arpgTabDown) {
    arpgTabDown = false;
    return;
    }
  if(event.key==Event::K_I && arpgIDown) {
    arpgIDown = false;
    if(!dialogs.isActive() && !inventory.isActive()) {
      if(auto pl = Gothic::inst().player())
        inventory.open(*pl);
      clearInput();
      }
    return;
    }
  if(uiKeyUp==&video){
    video.keyUpEvent(event);
    if(event.isAccepted())
      return;
    }
  if(uiKeyUp==&rootMenu){
    if(event.isAccepted())
      return;
    }
  if(uiKeyUp==&chapter){
    chapter.keyUpEvent(event);
    if(event.isAccepted())
      return;
    }
  if(uiKeyUp==&document){
    document.keyUpEvent(event);
    if(event.isAccepted())
      return;
    }
  if(uiKeyUp==&dialogs){
    dialogs.keyUpEvent(event);
    if(event.isAccepted())
      return;
    }
  if(uiKeyUp==&inventory){
    inventory.keyUpEvent(event);
    if(event.isAccepted())
      return;
    }


  auto act     = keycodec.tr(event);
  auto mapping = keycodec.mapping(event);

  std::string_view menuEv;
  if(act==KeyCodec::Escape)
    menuEv = Gothic::inst().menuMain();
  else if(act==KeyCodec::Log)
    menuEv = "MENU_LOG";
  else if(act==KeyCodec::Status)
    menuEv = "MENU_STATUS";

  if(!menuEv.empty()) {
    rootMenu.setMenu(menuEv,act);
    rootMenu.showVersion(act==KeyCodec::Escape);
    if(auto pl = Gothic::inst().player())
      rootMenu.setPlayer(*pl);
    clearInput();
    }
  else if(act==KeyCodec::Inventory && !dialogs.isActive()) {
    if(inventory.isActive()) {
      inventory.close();
      } else {
      auto pl = Gothic::inst().player();
      if(pl!=nullptr)
        inventory.open(*pl);
      }
    clearInput();
    }
  player.onKeyReleased(act, mapping);
  }

void MainWindow::focusEvent(FocusEvent &event) {
  if(!event.in) {
    // gothic-diablo: key-ups are lost while the window is in the background
    player.clearInput();
    player.arpgBlock(false);
    player.arpgChargeRelease();
    arpgLeftDown = arpgRightDown = arpgMidDown = arpgShowAll = arpgMidBlock = false;
    return;
    }
  dMouse = Point();

  if(SystemApi::isFullscreen(hwnd()) && !Camera::isArpgEnabled())
    setCursorPosition(Point(w()/2,h()/2));
  }

void MainWindow::paintFocus(Painter& p, const Focus& focus, const Matrix4x4& vp) {
  if(!focus || dialogs.isActive())
    return;

  const float scale = Gothic::interfaceScale(this);
  auto        world = Gothic::inst().world();
  auto        pl    = world==nullptr ? nullptr : world->player();
  if(pl==nullptr)
    return;

  auto pw  = 1.f;
  auto pos = focus.displayPosition();
  vp.project(pos.x,pos.y,pos.z,pw);

  if(pw<=0.f)
    return;

  pos /= pw;

  int   ix  = int((0.5f*pos.x+0.5f)*float(w()));
  int   iy  = int((0.5f*pos.y+0.5f)*float(h()));
  auto& fnt = Resources::font(scale);

  auto tsize = fnt.textSize(focus.displayName());
  ix-=tsize.w/2;
  if(iy<tsize.h)
    iy = tsize.h;
  if(iy>h())
    iy = h();
  fnt.drawText(p,ix,iy,focus.displayName());

  if(focus.npc!=nullptr && !focus.npc->isDead()) {
    float hp = float(focus.npc->attribute(ATR_HITPOINTS))/float(focus.npc->attribute(ATR_HITPOINTSMAX));
    drawBar(p,barHp, w()/2,10, hp, AlignHCenter|AlignTop);
    }

  const int foc = Gothic::settingsGetI("GAME","highlightMeleeFocus");
  if(focus.npc!=nullptr  &&
     (foc==1 || foc==3) &&
     player.isPressed(KeyCodec::ActionGeneric) &&
     pl->weaponState()!=WeaponState::NoWeapon &&
     pl->weaponState()!=WeaponState::Fist) {
    auto tr = vp;
    tr.mul(focus.npc->transform());

    const auto b    = focus.npc->bounds();
    const auto bbox = b.bbox; //focus.npc->bBox();

    Vec3 bx[] = {
      {bbox[0].x, bbox[0].y, bbox[0].z},
      {bbox[1].x, bbox[0].y, bbox[0].z},
      {bbox[1].x, bbox[1].y, bbox[0].z},
      {bbox[0].x, bbox[1].y, bbox[0].z},
      {bbox[0].x, bbox[0].y, bbox[1].z},
      {bbox[1].x, bbox[0].y, bbox[1].z},
      {bbox[1].x, bbox[1].y, bbox[1].z},
      {bbox[0].x, bbox[1].y, bbox[1].z},
      };

    int min[2]={ix,iy-20}, max[2]={ix,iy-20};
    for(int i=0; i<8; ++i) {
      tr.project(bx[i]);
      int x = int((bx[i].x*0.5f+0.5f)*float(w()));
      int y = int((bx[i].y*0.5f+0.5f)*float(h()));
      min[0] = std::min(x,min[0]);
      min[1] = std::min(y,min[1]);
      max[0] = std::max(x,max[0]);
      max[1] = std::max(y,max[1]);
      }

    paintFocus(p,Rect(min[0],min[1],max[0]-min[0],max[1]-min[1]));
    }

  // focusImg
  /*
  if(auto pl = focus.interactive){
    pl->marchInteractives(p,vp,w(),h());
    }*/
  }

bool MainWindow::arpgLabelRect(const Focus& f, const Matrix4x4& vp, Rect& out) const {
  auto name = f.displayName();
  if(name.empty())
    return false;
  auto pw  = 1.f;
  auto pos = f.displayPosition();
  vp.project(pos.x,pos.y,pos.z,pw);
  if(pw<=0.f)
    return false;
  pos /= pw;
  if(pos.x<-1.f || pos.x>1.f || pos.y<-1.f || pos.y>1.f)
    return false;
  const float scale = Gothic::interfaceScale(this);
  auto&       fnt   = Resources::font(scale);
  const int   pad   = int(4.f*scale);
  const int   ix    = int((0.5f*pos.x+0.5f)*float(w()));
  const int   iy    = int((0.5f*pos.y+0.5f)*float(h()));
  const auto  ts    = fnt.textSize(name);
  out = Rect(ix-ts.w/2-pad, iy-ts.h-pad, ts.w+2*pad, ts.h+2*pad);
  out.y = std::clamp(out.y, 0, std::max(0, h()-out.h));
  return true;
  }

void MainWindow::drawArpgLabel(Painter& p, const Rect& r, const Focus& f, bool hi) {
  const float scale = Gothic::interfaceScale(this);
  const int   pad   = int(4.f*scale);
  const bool  enemy = (f.npc!=nullptr && player.arpgIsEnemy(*f.npc));

  p.setBrush(Brush(hi ? Color(0.25f,0.2f,0.05f,0.8f) : Color(0.f,0.f,0.f,0.6f), Painter::Alpha));
  p.drawRect(r.x, r.y, r.w, r.h);

  auto type = Resources::FontType::Normal;
  if(enemy)
    type = Resources::FontType::Red;
  else if(hi)
    type = Resources::FontType::Hi;
  else if(f.interactive!=nullptr)
    type = Resources::FontType::Yellow;
  auto& fnt = Resources::font(type, scale);
  p.setBrush(Color(1.0));
  fnt.drawText(p, r.x+pad, r.y+r.h-pad, f.displayName());
  }

void MainWindow::paintArpgFocus(Painter& p, const Focus& focus, const Matrix4x4& vp) {
  if(!focus || dialogs.isActive())
    return;
  Rect r;
  if(arpgLabelRect(focus, vp, r))
    drawArpgLabel(p, r, focus, true);
  if(focus.npc!=nullptr && !focus.npc->isDead()) {
    float hp = float(focus.npc->attribute(ATR_HITPOINTS))/float(std::max(1,focus.npc->attribute(ATR_HITPOINTSMAX)));
    drawBar(p,barHp, w()/2,10, hp, AlignHCenter|AlignTop);
    }
  }

void MainWindow::paintArpgCombo(Painter& p, const Matrix4x4& vp) {
  // small bar above the hero: fills up until the moment for the next combo hit, gold = now
  auto pl = Gothic::inst().player();
  if(pl==nullptr || dialogs.isActive() || inventory.isActive())
    return;
  const auto ws = pl->weaponState();
  if(ws!=WeaponState::Fist && ws!=WeaponState::W1H && ws!=WeaponState::W2H)
    return;
  if(player.arpgChargeTime()>=0.f)
    return; // the charge bar is shown instead
  const auto ct = pl->comboTiming();
  if(!ct.attack || ct.last || ct.missed)
    return;

  auto pw  = 1.f;
  auto pos = pl->position() + Vec3(0,210.f,0);
  vp.project(pos.x,pos.y,pos.z,pw);
  if(pw<=0.f)
    return;
  pos /= pw;

  const float scale = Gothic::interfaceScale(this);
  const int   bw    = int(70.f*scale);
  const int   bh    = std::max(3, int(6.f*scale));
  const int   x     = int((0.5f*pos.x+0.5f)*float(w())) - bw/2;
  const int   y     = int((0.5f*pos.y+0.5f)*float(h())) - bh/2;

  p.setBrush(Brush(Color(0.f,0.f,0.f,0.6f), Painter::Alpha));
  p.drawRect(x-1, y-1, bw+2, bh+2);
  if(ct.broken) {
    p.setBrush(Brush(Color(0.8f,0.15f,0.1f,0.85f), Painter::Alpha));
    p.drawRect(x, y, bw, bh);
    }
  else if(ct.open) {
    const int fw = std::max(2, int(float(bw)*ct.windowLeft));
    p.setBrush(Brush(Color(1.f,0.78f,0.2f,0.95f), Painter::Alpha));
    p.drawRect(x+(bw-fw)/2, y, fw, bh);
    }
  else {
    p.setBrush(Brush(Color(0.8f,0.8f,0.8f,0.6f), Painter::Alpha));
    p.drawRect(x, y, int(float(bw)*ct.progress), bh);
    p.setBrush(Brush(Color(1.f,0.78f,0.2f,0.95f), Painter::Alpha));
    p.drawRect(x+bw-2, y, 2, bh); // where the window opens
    }

  // hits chained so far, and a dot when the next one is already queued
  const int pip = std::max(3, int(5.f*scale));
  for(int i=0; i<int(ct.len) && i<6; ++i) {
    p.setBrush(Brush(Color(1.f,0.78f,0.2f,0.9f), Painter::Alpha));
    p.drawRect(x + i*(pip+2), y+bh+3, pip, pip);
    }
  if(player.arpgAttackQueued()) {
    p.setBrush(Brush(Color(1.f,1.f,1.f,0.9f), Painter::Alpha));
    p.drawRect(x+bw+4, y+(bh-pip)/2, pip, pip);
    }
  }

void MainWindow::paintArpgLabels(Painter& p, const Matrix4x4& vp, const Focus& focus) {
  arpgLabels.clear();
  player.arpgVisibleObjects(arpgLabelObjs);
  if(arpgLabelObjs.empty())
    return;

  for(auto& f:arpgLabelObjs) {
    ArpgLabel l;
    if(!arpgLabelRect(f, vp, l.rect))
      continue;
    l.focus = f;
    arpgLabels.push_back(l);
    }

  // lowest on screen (usually closest) first, then push overlapping labels upwards
  std::sort(arpgLabels.begin(), arpgLabels.end(), [](const ArpgLabel& a, const ArpgLabel& b){
    return a.rect.y>b.rect.y;
    });
  auto overlaps = [](const Rect& a, const Rect& b) {
    return a.x<b.x+b.w && b.x<a.x+a.w && a.y<b.y+b.h && b.y<a.y+a.h;
    };
  for(size_t i=0; i<arpgLabels.size(); ++i) {
    for(int iter=0; iter<12; ++iter) {
      bool hit = false;
      for(size_t r=0; r<i; ++r)
        if(overlaps(arpgLabels[i].rect, arpgLabels[r].rect)) {
          arpgLabels[i].rect.y = arpgLabels[r].rect.y - arpgLabels[i].rect.h - 1;
          hit = true;
          }
      if(!hit)
        break;
      }
    }

  // highlighted: label under the cursor, otherwise the object picked under the cursor
  Focus hover = arpgLabelAt(arpgCursor);
  if(!hover && focus.npc==nullptr)
    hover = focus;
  for(auto& l:arpgLabels) {
    const bool hi = bool(hover) && l.focus.item==hover.item && l.focus.interactive==hover.interactive;
    drawArpgLabel(p, l.rect, l.focus, hi);
    }
  }

Focus MainWindow::arpgLabelAt(const Point& pos) const {
  // labels drawn last are on top
  for(size_t i=arpgLabels.size(); i>0; --i) {
    auto& l = arpgLabels[i-1];
    if(l.rect.contains(pos))
      return l.focus;
    }
  return Focus();
  }

void MainWindow::paintFocus(Painter& p, Rect rect) {
  if(focusImg==nullptr)
    return;
  const int w2 = focusImg->w();
  const int h2 = focusImg->h();
  const int w  = w2/2;
  const int h  = h2/2;

  if(rect.w<w) {
    int dw = w-rect.w;
    rect.x -= dw/2;
    rect.w += dw;
    }
  if(rect.h<h) {
    int dh = h-rect.h;
    rect.y -= dh/2;
    rect.h += dh;
    }

  p.setBrush(Brush(*focusImg,Painter::Add));
  p.drawRect(rect.x,         rect.y,         w,h, 0,0, w, h);
  p.drawRect(rect.x+rect.w-w,rect.y,         w,h, w,0, w2,h);
  p.drawRect(rect.x,         rect.y+rect.h-h,w,h, 0,h, w, h2);
  p.drawRect(rect.x+rect.w-w,rect.y+rect.h-h,w,h, w,h, w2,h2);
  }

void MainWindow::drawBar(Painter &p, const Tempest::Texture2d* bar, int x, int y, float v, AlignFlag flg) {
  if(barBack==nullptr || bar==nullptr)
    return;
  const float scale   = Gothic::interfaceScale(this);
  const float destW   = 200.f*scale*float(std::min(w(),800))/800.f;
  const float k       = float(destW)/float(std::max(barBack->w(),1));
  const float destH   = float(barBack->h())*k;
  const float destHin = float(destH)*24.f/32.f;
  //const float destHin = 20;//float(destH)*24.f/32.f;

  v = std::max(0.f,std::min(v,1.f));
  if(flg & AlignRight)
    x-=int(destW);
  else if(flg & AlignHCenter)
    x-=int(destW)/2;
  if(flg & AlignBottom)
    y-=int(destH);

  p.setBrush(*barBack);
  p.drawRect(x,y,int(destW),int(destH), 0,0,barBack->w(),barBack->h());

  int   dy = int(0.5f*(destH-destHin));
  float pd = 9.f*k;
  p.setBrush(*bar);
  p.drawRect(x+int(pd),y+dy,int(float(destW-pd*2)*v),int(destHin),
             0,0,bar->w(),bar->h());
  }

void MainWindow::drawMsg(Tempest::Painter& p) {
  if(barBack==nullptr)
    return;
  const float scale   = Gothic::interfaceScale(this);
  const float destW   = 200.f*scale*float(std::min(w(),800))/800.f;
  const float k       = float(destW)/float(std::max(barBack->w(),1));
  const float destH   = float(barBack->h())*k;

  const int y = 10 + int(destH) + 10;
  dialogs.drawMsg(p, y);
  }

void MainWindow::drawProgress(Painter &p, int x, int y, int w, int h, float v) {
  if(v<0.1f)
    v=0.1f;
  p.setBrush(*loadBox);
  p.drawRect(x,y,w,h, 0,0,loadBox->w(),loadBox->h());

  int paddL = int((float(w)*75.f)/float(loadBox->w()));
  int paddT = int((float(h)*10.f)/float(loadBox->h()));

  if(Gothic::inst().version().game==1) {
    paddL = int((float(w)*30.f)/float(loadBox->w()));
    paddT = int((float(h)* 5.f)/float(loadBox->h()));
    }

  p.setBrush(*loadVal);
  p.drawRect(x+paddL,y+paddT,int(float(w-2*paddL)*v),h-2*paddT,
             0,0,loadVal->w(),loadVal->h());
  }

void MainWindow::drawLoading(Painter &p, int x, int y, int w, int h) {
  float v = float(Gothic::inst().loadingProgress())/100.f;
  drawProgress(p,x,y,w,h,v);
  }

void MainWindow::drawSaving(Painter &p) {
  if(auto back = Gothic::inst().loadingBanner()) {
    p.setBrush(Brush(*back,Painter::NoBlend));
    p.drawRect(0,0,this->w(),this->h(),
               0,0,back->w(),back->h());
    }

  if(saveback==nullptr)
    saveback = Resources::loadTexture("SAVING.TGA");
  if(saveback==nullptr)
    return;

  const float scale = Gothic::interfaceScale(this);
  int         szX   = Gothic::options().saveGameImageSize.w;
  int         szY   = Gothic::options().saveGameImageSize.h;

  if(szX<=460 || szY<=300) {
    // way too small otherwise
    szX = 460;
    szY = 300;
    }
  szX = int(float(szX)*scale);
  szY = int(float(szY)*scale);
  drawSaving(p,*saveback,szX,szY,scale);
  }

void MainWindow::drawSaving(Painter& p, const Tempest::Texture2d& back, int sw, int sh, float scale) {
  const int x = (w()-sw)/2, y = (h()-sh)/2;

  // SAVING.TGA is semi-transparent image with the idea to accomulate alpha over time
  // ... for loop for now
  p.setBrush(back);
  for(int i=0;i<10;++i) {
    p.drawRect(x,y,sw,sh, 0,0,back.w(),back.h());
    }

  float v = float(Gothic::inst().loadingProgress())/100.f;
  drawProgress(p, x+int(100.f*scale), y+sh-int(75.f*scale), sw-2*int(100.f*scale), int(40.f*scale), v);
  }

void MainWindow::isDialogClosed(bool& ret) {
  ret = !(dialogs.isActive() || document.isActive());
  }

template<Tempest::KeyEvent::KeyType k>
void MainWindow::onMarvinKey() {
  switch(k) {
    case Event::K_F2:
      if(Gothic::inst().isMarvinEnabled()) {
        console.resize(w(),h());
        console.setFocus(true);
        console.exec();
        }
      break;
    case Event::K_F3:
      setFullscreen(!SystemApi::isFullscreen(hwnd()));
      break;
    case Event::K_F4:
      if(Gothic::inst().isMarvinEnabled()) {
        auto camera = Gothic::inst().camera();
        auto pl = Gothic::inst().player();
        if(camera!=nullptr && pl!=nullptr) {
          camera->setMarvinMode(Camera::M_Normal);
          camera->reset(pl);
          }
        }
      break;
    case Event::K_F5: {
      const bool useQuickSaveKeys = Gothic::settingsGetI("GAME", "useQuickSaveKeys")!=0;
#ifdef NDEBUG
      const bool debug = false;
#else
      const bool debug = true;
#endif
      if(!debug && Gothic::inst().isMarvinEnabled() && !dialogs.isActive()) {
        if(auto camera = Gothic::inst().camera()) {
          camera->setMarvinMode(Camera::M_Freeze);
          }
        }
      else if(Gothic::inst().isInGameAndAlive() && !Gothic::inst().isPause() && useQuickSaveKeys) {
        Gothic::inst().quickSave();
        }
      break;
      }

    case Event::K_F6:
      if(Gothic::inst().isMarvinEnabled() && !dialogs.isActive()) {
        auto camera = Gothic::inst().camera();
        auto pl     = Gothic::inst().player();
        auto inter  = pl!=nullptr ? pl->interactive() : nullptr;
        if(camera!=nullptr && inter==nullptr)
          camera->setMarvinMode(Camera::M_Free);
        }
      break;
    case Event::K_F7:
      if(Gothic::inst().isMarvinEnabled() && !dialogs.isActive()) {
        if(auto camera = Gothic::inst().camera()) {
          camera->setMarvinMode(Camera::M_Pinned);
          }
        }
      break;
    case Event::K_F8:
      //player.marvinF8();
      break;

    case Event::K_F9: {
      const bool useQuickSaveKeys = Gothic::settingsGetI("GAME", "useQuickSaveKeys")!=0;
      if(Gothic::inst().isMarvinEnabled()) {
        if(runtimeMode==R_Normal)
          runtimeMode = R_Suspended; else
          runtimeMode = R_Normal;
        }
      else if(!Gothic::inst().isPause() && useQuickSaveKeys) {
        Gothic::inst().quickLoad();
        }
      break;
      }
    case Event::K_F10:
      if(runtimeMode==R_Suspended)
        runtimeMode = R_Step;
      break;
    case Event::K_P:
      if(Gothic::inst().isMarvinEnabled()) {
        if(auto p = Gothic::inst().player()) {
          auto pos = p->position();
          string_frm buf("Position: ", pos.x,'/',pos.y,'/',pos.z);
          Gothic::inst().onPrint(buf);
          }
        }
      break;
    }
  }

uint64_t MainWindow::tick() {
  auto time = Application::tickCount();
  auto dt   = time-lastTick;
  // NOTE: limit to ~200 FPS in game logic to avoid math issues
  if(dt<5)
    return 0;
  lastTick  = time;

  auto st = Gothic::inst().checkLoading();
  if(st==Gothic::LoadState::Finalize || st==Gothic::LoadState::FailedLoad || st==Gothic::LoadState::FailedSave) {
    Gothic::inst().finishLoading();
    if(st==Gothic::LoadState::FailedLoad)
      rootMenu.setMainMenu();
    if(st==Gothic::LoadState::FailedSave)
      Gothic::inst().onPrint("unable to write savegame file");
    return 0;
    }
  else if(st!=Gothic::LoadState::Idle) {
    if(st==Gothic::LoadState::Loading)
      GameMusic::inst().setMusic(GameMusic::SysLoading); else
      rootMenu.processMusicTheme();
    return 0;
    }

  video.tick();
  if(video.isActive())
    return 0;

  if(Gothic::inst().isPause()) {
    return 0;
    }

  if(dt>50)
    dt=50;

  if(runtimeMode==R_Step) {
    runtimeMode = R_Suspended;
    dt = 1000/60; //60 fps
    }
  else if(runtimeMode==R_Suspended) {
    auto camera = Gothic::inst().camera();
    if(camera!=nullptr && camera->isFree()) {
      tickMouse(dt);
      }
    update();
    return 0;
    }

  dialogs.tick(dt);
  inventory.tick(dt);
  Gothic::inst().tick(dt);
  player.tickFocus();

  if(dialogs.isActive())
    ;//clearInput();
  if(document.isActive())
    clearInput();
  {
    // gothic-diablo: a menu takes the keyboard (loot, inventory, dialog): key-ups may never reach the player
    const bool ui = dialogs.isActive() || inventory.isActive() || document.isActive();
    if(ui!=uiWasActive)
      player.clearInput();
    uiWasActive = ui;
  }
  tickMouse(dt);
  updateArpgAim();
  player.tickMove(dt);
  update();
  return dt;
  }

void MainWindow::updateAnimation(uint64_t dt) {
  Gothic::inst().updateAnimation(dt);
  }

void MainWindow::tickCamera(uint64_t dt) {
  auto pcamera = Gothic::inst().camera();
  auto pl      = Gothic::inst().player();
  if(pcamera==nullptr)
    return;

  auto&      camera       = *pcamera;
  const auto ws           = pl!=nullptr ? pl->weaponState() : WeaponState::NoWeapon;
  const bool meleeFocus   = (ws==WeaponState::Fist ||
                             ws==WeaponState::W1H  ||
                             ws==WeaponState::W2H);
  auto       pos          = pl!=nullptr ? pl->cameraBone(camera.isFirstPerson()) : Vec3();

  if(!camera.isCutscene() && !camera.isFree()) {
    const bool fs = SystemApi::isFullscreen(hwnd());
    if(!fs && mouseP[Event::ButtonLeft]) {
      camera.setSpin(camera.spin());
      camera.setTarget(pos);
      }
    else if(dialogs.isActive() && !dialogs.isMobsiDialog()) {
      dialogs.dialogCamera(camera);
      }
    else if(inventory.isActive()) {
      camera.setTarget(pos);
      }
    else if(player.focus().npc!=nullptr && meleeFocus && pl!=nullptr) {
      auto spin = camera.spin();
      spin.y = pl->rotation();
      camera.setSpin(spin);
      camera.setTarget(pos);
      }
    else if(pl!=nullptr && !camera.isFree()) {
      auto spin = camera.spin();
      if(pl->interactive()==nullptr && !pl->isDown())
        spin.y = pl->rotation();
      if(pl->isDive() && !camera.isMarvin())
        spin.x = -pl->rotationY();
      camera.setSpin(spin);
      camera.setTarget(pos);
      }
    }

  if(dt==0)
    return;
  if(camera.isToggleEnabled() && !camera.isCutscene())
    camera.setMode(solveCameraMode());
  camera.tick(dt);
  }

Camera::Mode MainWindow::solveCameraMode() const {
  const auto camera = Gothic::inst().camera();
  if(camera!=nullptr && camera->isFree())
    return Camera::Normal;

  if(dialogs.isActive())
    return Camera::Dialog;

  if(camera!=nullptr && camera->isFirstPerson())
    return Camera::FirstPerson;

  if(inventory.isOpen()==InventoryMenu::State::Equip ||
     inventory.isOpen()==InventoryMenu::State::Ransack)
    return Camera::Inventory;

  if(auto pl=Gothic::inst().player()) {
    if(pl->interactive()!=nullptr)
      return Camera::Mobsi;
    }

  if(auto pl = Gothic::inst().player()) {
    if(pl->isDead())
      return Camera::Death;
    if(pl->isDive())
      return Camera::Dive;
    if(pl->isSwim())
      return Camera::Swim;
    if(pl->isFallingDeep())
      return Camera::Fall;
    bool g2 = Gothic::inst().version().game==2;
    switch(pl->weaponState()){
      case WeaponState::Fist:
      case WeaponState::W1H:
      case WeaponState::W2H:
        return Camera::Melee;
      case WeaponState::Bow:
      case WeaponState::CBow:
        return g2 ? Camera::Ranged : Camera::Normal;
      case WeaponState::Mage:
        return g2 ? Camera::Ranged : Camera::Melee;
      case WeaponState::NoWeapon:
        return Camera::Normal;
      }
    }

  return Camera::Normal;
  }

void MainWindow::startGame(std::string_view slot) {
  // gothic.emitGlobalSound(gothic.loadSoundFx("NEWGAME"));

  if(Gothic::inst().checkLoading()==Gothic::LoadState::Idle){
    setGameImpl(nullptr);
    }

  Gothic::inst().startLoad("LOADING.TGA",[slot=std::string(slot)](std::unique_ptr<GameSession>&& game){
    game = nullptr; // clear world-memory now
    std::unique_ptr<GameSession> w(new GameSession(slot));
    return w;
    });

  background = Texture2d();
  update();
  }

void MainWindow::loadGame(std::string_view slot) {
  if(Gothic::inst().checkLoading()==Gothic::LoadState::Idle) {
    setGameImpl(nullptr);
    }

  Gothic::inst().setBenchmarkMode(Benchmark::None);
  Gothic::inst().startLoad("LOADING.TGA",[slot=std::string(slot)](std::unique_ptr<GameSession>&& game){
    game = nullptr; // clear world-memory now
    Tempest::RFile file(slot);
    Serialize      s(file);
    std::unique_ptr<GameSession> w(new GameSession(s));
    return w;
    });

  background = Texture2d();
  update();
  }

void MainWindow::saveGame(std::string_view slot, std::string_view name) {
  if(dialogs.isActive())
    return;
  if(auto w = Gothic::inst().world(); w!=nullptr && w->currentCs()!=nullptr)
    return;

  auto tex  = renderer.screenshoot(cmdId, Size(int(swapchain.w()),int(swapchain.h())));
  auto lres = Attachment();

  static int32_t kThumbW = 800;
  const  int32_t kThumbH = tex.w()>0 ? int32_t((tex.h() * kThumbW) / tex.w()) : 0;
  if(kThumbW>0 && kThumbH>0 && kThumbW<tex.w() && kThumbH<tex.h()) {
    lres = device.attachment(Tempest::TextureFormat::RGBA8, uint32_t(kThumbW), uint32_t(kThumbH));
    }

  if(!lres.isEmpty()) {
    // reduce size of the save entry preview screenshot for faster save & load
    CommandBuffer cmd;
    {
    auto enc = cmd.startEncoding(device);
    enc.setDebugMarker("Downscale screenhoot");
    enc.setFramebuffer({{lres, Vec4(), Tempest::Preserve}});
    enc.setPushData(IVec2(lres.w(), lres.h()));
    enc.setBinding(0, tex, Sampler::nearest());
    enc.setPipeline(Shaders::inst().downscale);
    enc.draw(nullptr, 0, 3);
    }
    auto sync = device.submit(cmd);
    sync.wait();
    }

  auto& thumb = lres.isEmpty() ? tex : lres;
  auto  pm    = device.readPixels(textureCast<const Texture2d&>(thumb));

  Gothic::inst().startSave(std::move(textureCast<Texture2d&>(tex)),[slot=std::string(slot),name=std::string(name),pm](std::unique_ptr<GameSession>&& game){
    if(!game)
      return std::move(game);

    {
    Tempest::WFile  f(slot);
    Serialize      s(f);
    game->save(s,name,pm);
    }

    // no print yet, because threading
    // gothic.print("Game saved");
    return std::move(game);
    });

  update();
  }

void MainWindow::onVideo(std::string_view fname) {
  if(Gothic::inst().isBenchmarkMode())
    return;
  video.pushVideo(fname);
  }

void MainWindow::onStartLoading() {
  player   .clearInput();
  inventory.onWorldChanged();
  dialogs  .onWorldChanged();
  }

void MainWindow::onWorldLoaded() {
  dMouse = Point();

  if(Gothic::inst().isBenchmarkMode()) {
    if(auto world = Gothic::inst().world()) {
      const TriggerEvent evt("TIMEDEMO","",world->tickCount(),TriggerEvent::T_Trigger);
      world->execTriggerEvent(evt);
      }
    benchmark.clear();
    }

  player   .clearInput();
  inventory.onWorldChanged();
  dialogs  .onWorldChanged();

  device.waitIdle();
  for(auto& c:commands)
    c = device.commandBuffer();

  if(auto c = Gothic::inst().camera())
    c->setViewport(uint32_t(w()),uint32_t(h()));

  renderer.onWorldChanged();

  if(auto pl = Gothic::inst().player())
    rootMenu.setPlayer(*pl);

  if(auto pl = Gothic::inst().player())
    pl->multSpeed(1.f);
  lastTick = Application::tickCount();
  player.clearFocus();
  }

void MainWindow::onSessionExit() {
  rootMenu.setMainMenu();
  }

void MainWindow::onBenchmarkFinished() {
  if(benchmark.numFrames==0)
    return;

  double fps  = benchmark.fpsSum/double(benchmark.numFrames);
  double low1 = 0;
  size_t num1 = 0;
  for(size_t i=0; i<benchmark.low1procent.size(); ++i) {
    auto v = benchmark.low1procent[i];
    if(v<=0)
      continue;
    low1 += 1000.0/double(v);
    num1 += 1;
    }
  low1 = num1>0 ? low1/double(num1) : 0.0;
  benchmark.clear();

  string_frm str("Benchmark: low 1% = ", low1, " fps = ", fps);
  Log::i(str.c_str());
  console.printLine(str);

  if(Gothic::inst().isBenchmarkModeCi()) {
    Log::i("Exiting benchmark");
    Tempest::SystemApi::exit();
    return;
    }

  console.setFocus(true);
  console.exec();
  }

void MainWindow::setGameImpl(std::unique_ptr<GameSession> &&w) {
  // clear pointers to outdated game-state
  inventory.onWorldChanged();
  dialogs  .onWorldChanged();
  Gothic::inst().setGame(std::move(w));
  }

void MainWindow::clearInput() {
  player.clearInput();
  std::memset(mouseP,0,sizeof(mouseP));
  }

void MainWindow::setFullscreen(bool fs) {
  SystemApi::setAsFullscreen(hwnd(),fs);
  setCursorShape((fs && !Camera::isArpgEnabled()) ? CursorShape::Hidden : CursorShape::Arrow);
  }

void MainWindow::render(){
  try {
    static uint64_t time=Application::tickCount();

    static bool once=true;
    if(once) {
      Gothic::inst().emitGlobalSoundWav("GAMESTART.WAV");
      once=false;
      }

    if(T_UNLIKELY(Gothic::inst().isBenchmarkModeCi())) {
      const auto st = Gothic::inst().checkLoading();
      if(st==Gothic::LoadState::Loading) {
        // skip loading frames in benchmark-ci mode, for sake of easier tooling
        return;
        }
      }

    /*
      Note: game update goes first
      once player position is updated, animation bones(cameraBone in particular) can be updated
      lastly - camera position
      */
    const uint64_t dt = tick();
    updateAnimation(dt);
    tickCamera(dt);

    auto& sync = fence[cmdId];
    if(!sync.wait(0)) {
      // GPU rendering is not done, pass to next frame
      std::this_thread::yield();
      return;
      }
    Resources::resetRecycled(cmdId);

    if(video.isActive()) {
      video.paint(device,cmdId);
      uiLayer.clear();
      PaintEvent p(uiLayer,atlas,this->w(),this->h());
      video.paintEvent(p);
      }
    else if(needToUpdate() || Gothic::inst().checkLoading()!=Gothic::LoadState::Idle) {
      dispatchPaintEvent(uiLayer,atlas);

      numOverlay.clear();
      PaintEvent p(numOverlay,atlas,this->w(),this->h());
      inventory.paintNumOverlay(p);
      }
    uiMesh [cmdId].update(device,uiLayer);
    numMesh[cmdId].update(device,numOverlay);

    CommandBuffer& cmd = commands[cmdId];
    {
    auto enc = cmd.startEncoding(device);
    renderer.draw(swapchain[swapchain.currentImage()],enc,cmdId,uiMesh[cmdId],numMesh[cmdId],inventory,video);
    }
    sync = device.submit(cmd);
    device.present(swapchain);
    cmdId = (cmdId+1u)%Resources::MaxFramesInFlight;

    auto t = Application::tickCount();
    auto frameTime = maxFpsInv;
    if(!Gothic::inst().isInGame() && !video.isActive())
      frameTime = std::max<uint64_t>(frameTime,16);
    if(t-time<frameTime) {
      uint32_t delay = uint32_t(frameTime-(t-time));
      Application::sleep(delay);
      t += delay;
      }
    fps.push(t-time);
    if(Gothic::inst().isBenchmarkMode() && Gothic::inst().world()!=nullptr && Gothic::inst().world()->currentCs()!=nullptr)
      benchmark.push(t-time);
    time = t;
    }
  catch(const Tempest::SwapchainSuboptimal&) {
    Log::e("swapchain is outdated - reset renderer");
    device.waitIdle();
    swapchain.reset();
    }
  }

double MainWindow::Fps::get() const {
  uint64_t sum=0,num=0;
  for(auto& i:dt)
    if(i>0) {
      sum+=i;
      num++;
      }
  if(num==0 || sum==0)
    return 60;
  uint64_t fps = (1000*100*num)/sum;
  return double(fps)/100.0;
  }

void MainWindow::Fps::push(uint64_t t) {
  for(size_t i=9;i>0;--i)
    dt[i]=dt[i-1];
  dt[0]=t;
  }

void MainWindow::BenchmarkData::push(uint64_t t) {
  fpsSum += t>0 ? (1000.0/double((t))) : 60.0;
  numFrames++;
  auto at = std::lower_bound(low1procent.begin(), low1procent.end(), t, std::greater<uint64_t>());
  low1procent.insert(at, t);
  low1procent.resize(std::min(low1procent.size(), (numFrames+99)/100));
  }

void MainWindow::BenchmarkData::clear() {
  low1procent.reserve(128);
  low1procent.clear();
  numFrames = 0;
  fpsSum = 0;
  }

void MainWindow::paintArpgMinimap(Painter& p) {
  auto world = Gothic::inst().world();
  auto pl    = Gothic::inst().player();
  auto cam   = Gothic::inst().camera();
  arpgMapRect = Rect();
  if(!arpgMinimap || world==nullptr || pl==nullptr || cam==nullptr || dialogs.isActive() || inventory.isActive())
    return;

  // the world map of the current level (from the map items of the scripts)
  if(arpgMapWorld!=world->name()) {
    arpgMapWorld = std::string(world->name());
    arpgMapValid = false;
    for(auto& m:world->script().arpgWorldMaps()) {
      if(m.level!=arpgMapWorld)
        continue;
      arpgMapTex    = m.texture;
      arpgMapBounds = Rect(m.left, m.top, m.right-m.left, m.bottom-m.top);
      arpgMapValid  = (m.right!=m.left && m.bottom!=m.top);
      }
    }
  const Texture2d* tex = arpgMapValid ? Resources::loadTexture(arpgMapTex) : nullptr;

  const Vec3  hero = pl->position();
  const auto& view = cam->arpgView();

  // explored cells: everything around the hero within the sight range
  constexpr float cell = 400.f;
  auto& explored = arpgExplored[arpgMapWorld];
  auto  key      = [](int i, int j) { return (int64_t(i)<<32) ^ int64_t(uint32_t(j)); };
  {
    const float rr = std::min(view.fowRadius>0.f ? view.fowRadius : 1500.f, 2000.f);
    const int   ci = int(std::floor(hero.x/cell)), cj = int(std::floor(hero.z/cell));
    const int   n  = int(std::ceil(rr/cell));
    for(int j=-n; j<=n; ++j)
      for(int i=-n; i<=n; ++i) {
        const float x = (float(ci+i)+0.5f)*cell - hero.x;
        const float z = (float(cj+j)+0.5f)*cell - hero.z;
        if(x*x+z*z<=rr*rr)
          explored.insert(key(ci+i, cj+j));
        }
  }

  const float scale = Gothic::interfaceScale(this);
  const int   S     = int(220.f*scale);
  const int   x0    = w() - S - int(12.f*scale);
  const int   y0    = int(48.f*scale);
  const float cx    = float(x0) + float(S)*0.5f;
  const float cy    = float(y0) + float(S)*0.5f;
  const float half  = arpgMapHalf;            // world cm from the center to the edge (wheel over the map)
  arpgMapRect = Rect(x0, y0, S, S);
  const float k     = float(S)/(2.f*half);    // pixels per cm

  // screen up = where the camera looks, screen right = camera right (same as W/D)
  const float yaw = cam->arpgYaw()*float(M_PI)/180.f;
  const Vec2  f   = {std::cos(yaw),  std::sin(yaw)};
  const Vec2  r   = {std::sin(yaw), -std::cos(yaw)};
  auto toScreen = [&](float wx, float wz) {
    const float dx = wx-hero.x, dz = wz-hero.z;
    return Vec2(cx + k*(dx*r.x + dz*r.y), cy - k*(dx*f.x + dz*f.y));
    };
  auto toWorld = [&](float a, float b) { // a: along screen right, b: along screen up (cm)
    return Vec2(hero.x + a*r.x + b*f.x, hero.z + a*r.y + b*f.y);
    };

  p.pushState();
  p.setScissor(x0, y0, S, S);

  p.setBrush(Brush(Color(0.06f,0.05f,0.04f,0.85f), Painter::Alpha));
  p.drawRect(x0, y0, S, S);

  if(tex!=nullptr) {
    const auto& wb  = arpgMapBounds;
    const float tw  = float(tex->w()), th = float(tex->h());
    auto uv = [&](const Vec2& wp) {
      return Vec2((wp.x-float(wb.x))/float(wb.w)*tw, (wp.y-float(wb.y))/float(wb.h)*th);
      };
    const Vec2 tl = uv(toWorld(-half,  half)), tr = uv(toWorld( half,  half));
    const Vec2 br = uv(toWorld( half, -half)), bl = uv(toWorld(-half, -half));
    const float X0 = float(x0), Y0 = float(y0), X1 = float(x0+S), Y1 = float(y0+S);
    p.setBrush(Brush(*tex, Color(1.f,1.f,1.f,0.9f), Painter::Alpha, ClampMode::ClampToBorder));
    p.drawTriangle(X0,Y0, tl.x,tl.y,  X1,Y0, tr.x,tr.y,  X1,Y1, br.x,br.y);
    p.drawTriangle(X0,Y0, tl.x,tl.y,  X1,Y1, br.x,br.y,  X0,Y1, bl.x,bl.y);
    }

  // not explored yet: dark
  {
    p.setBrush(Brush(Color(0.f,0.f,0.f,0.8f), Painter::Alpha));
    const float reach = half*1.42f;
    const int   i0 = int(std::floor((hero.x-reach)/cell)), i1 = int(std::floor((hero.x+reach)/cell));
    const int   j0 = int(std::floor((hero.z-reach)/cell)), j1 = int(std::floor((hero.z+reach)/cell));
    for(int j=j0; j<=j1; ++j)
      for(int i=i0; i<=i1; ++i) {
        if(explored.find(key(i,j))!=explored.end())
          continue;
        const Vec2 a = toScreen(float(i)*cell,   float(j)*cell);
        const Vec2 b = toScreen(float(i+1)*cell, float(j)*cell);
        const Vec2 c = toScreen(float(i+1)*cell, float(j+1)*cell);
        const Vec2 d = toScreen(float(i)*cell,   float(j+1)*cell);
        p.drawTriangle(a.x,a.y,0,0, b.x,b.y,0,0, c.x,c.y,0,0);
        p.drawTriangle(a.x,a.y,0,0, c.x,c.y,0,0, d.x,d.y,0,0);
        }
  }

  // creatures in sight: enemies red, others light
  {
    const float sight = view.fowRadius>0.f ? view.fowRadius : 1500.f;
    const int   dot   = std::max(3, int(4.f*scale));
    for(uint32_t i=0; i<world->npcCount(); ++i) {
      auto npc = world->npcById(i);
      if(npc==nullptr || npc==pl || npc->isDead())
        continue;
      const auto  np = npc->position();
      const float dx = np.x-hero.x, dz = np.z-hero.z;
      if(dx*dx+dz*dz > sight*sight)
        continue;
      const Vec2 sp = toScreen(np.x, np.z);
      if(player.arpgIsEnemy(*npc))
        p.setBrush(Brush(Color(0.9f,0.15f,0.1f,1.f), Painter::Alpha)); else
        p.setBrush(Brush(Color(0.85f,0.85f,0.7f,1.f), Painter::Alpha));
      p.drawRect(int(sp.x)-dot/2, int(sp.y)-dot/2, dot, dot);
      }
  }

  // the hero: arrow in the direction he faces
  {
    const float rot = pl->rotation()*float(M_PI)/180.f;
    Vec2 hd = {std::cos(rot)*r.x + std::sin(rot)*r.y, -(std::cos(rot)*f.x + std::sin(rot)*f.y)};
    const float l = std::sqrt(hd.x*hd.x+hd.y*hd.y);
    if(l>0.f)
      hd = hd/l;
    const Vec2  pr  = {-hd.y, hd.x};
    const float a   = 8.f*scale, b = 5.f*scale;
    const Vec2  tip = Vec2(cx,cy) + hd*a;
    const Vec2  b0  = Vec2(cx,cy) - hd*b + pr*b;
    const Vec2  b1  = Vec2(cx,cy) - hd*b - pr*b;
    p.setBrush(Brush(Color(1.f,0.8f,0.25f,1.f), Painter::Alpha));
    p.drawTriangle(tip.x,tip.y,0,0, b0.x,b0.y,0,0, b1.x,b1.y,0,0);
  }

  // north
  {
    Vec2 nd = {r.y, -f.y};
    const float l = std::sqrt(nd.x*nd.x+nd.y*nd.y);
    if(l>0.f)
      nd = nd/l;
    auto&      fnt = Resources::font(scale);
    const auto ts  = fnt.textSize("N");
    const Vec2 np  = Vec2(cx,cy) + nd*(float(S)*0.5f - float(ts.h));
    p.setBrush(Color(1.0));
    fnt.drawText(p, int(np.x)-ts.w/2, int(np.y)+ts.h/2, "N");
  }

  // frame
  {
    const int bw = std::max(1, int(2.f*scale));
    p.setBrush(Brush(Color(0.55f,0.45f,0.25f,0.9f), Painter::Alpha));
    p.drawRect(x0,        y0,        S,  bw);
    p.drawRect(x0,        y0+S-bw,   S,  bw);
    p.drawRect(x0,        y0,        bw, S);
    p.drawRect(x0+S-bw,   y0,        bw, S);
  }

  p.popState();
  }

void MainWindow::paintArpgCombat(Painter& p, const Matrix4x4& vp) {
  // gothic-diablo: life bars above enemies in a fight, floating damage numbers, special attack cooldown
  auto world = Gothic::inst().world();
  auto pl    = Gothic::inst().player();
  auto cam   = Gothic::inst().camera();
  if(world==nullptr || pl==nullptr || cam==nullptr || dialogs.isActive() || inventory.isActive())
    return;
  const float scale = Gothic::interfaceScale(this);
  auto toScreen = [&](Vec3 pos, Point& out) {
    float pw = 1.f;
    vp.project(pos.x,pos.y,pos.z,pw);
    if(pw<=0.f)
      return false;
    pos /= pw;
    if(pos.x<-1.2f || pos.x>1.2f || pos.y<-1.2f || pos.y>1.2f)
      return false;
    out = Point(int((0.5f*pos.x+0.5f)*float(w())), int((0.5f*pos.y+0.5f)*float(h())));
    return true;
    };

  // life bars
  {
    const auto& view  = cam->arpgView();
    const float sight = view.fowRadius>0.f ? view.fowRadius : 1500.f;
    const int   bw    = int(46.f*scale);
    const int   bh    = std::max(3, int(5.f*scale));
    const Vec3  hero  = pl->position();
    world->detectNpc(hero, sight, [&](Npc& n) {
      if(&n==pl || n.isDead() || !player.arpgIsEnemy(n))
        return;
      const int32_t hp  = n.attribute(ATR_HITPOINTS);
      const int32_t hpM = std::max(1, n.attribute(ATR_HITPOINTSMAX));
      if(hp>=hpM && n.target()!=pl)
        return; // not in a fight yet
      Point sp;
      if(!toScreen(n.position()+Vec3(0,n.isDown() ? 60.f : 210.f,0), sp))
        return;
      const float v = std::clamp(float(hp)/float(hpM), 0.f, 1.f);
      p.setBrush(Brush(Color(0.f,0.f,0.f,0.7f), Painter::Alpha));
      p.drawRect(sp.x-bw/2-1, sp.y-1, bw+2, bh+2);
      p.setBrush(Brush(Color(0.75f,0.08f,0.05f,0.95f), Painter::Alpha));
      p.drawRect(sp.x-bw/2, sp.y, int(float(bw)*v), bh);
      });
  }

  // floating numbers: rise and fade in about a second
  {
    auto&          list = world->arpgPopups();
    const uint64_t now  = world->tickCount();
    for(size_t i=0; i<list.size();) {
      if(now>list[i].time+1100)
        list.erase(list.begin()+long(i)); else
        ++i;
      }
    for(auto& e:list) {
      const float t = float(now-e.time)/1000.f;
      Point sp;
      if(!toScreen(e.pos+Vec3(0,70.f*t,0), sp))
        continue;
      const float a = std::clamp(1.f-(t-0.6f)/0.5f, 0.f, 1.f);
      Resources::FontType type = Resources::FontType::Normal;
      char txt[32] = {};
      if(e.kind==2) {
        type = Resources::FontType::Disabled;
        std::snprintf(txt, sizeof(txt), "Blok");
        } else {
        type = (e.kind==1) ? Resources::FontType::Red : Resources::FontType::Normal;
        std::snprintf(txt, sizeof(txt), "%d", int(e.value));
        }
      auto&      fnt = Resources::font(type, scale*(e.kind==1 ? 1.f : 1.15f));
      const auto ts  = fnt.textSize(txt);
      p.setBrush(Color(1.f,1.f,1.f,a));
      fnt.drawText(p, sp.x-ts.w/2, sp.y, txt);
      }
  }

  // charging the special attack: bar above the head, gold = release now, red = held too long
  {
    const float t = player.arpgChargeTime();
    Point sp;
    if(t>=0.f && toScreen(pl->position()+Vec3(0,230.f,0), sp)) {
      const int   bw   = int(90.f*scale), bh = std::max(4, int(8.f*scale));
      const float full = PlayerControl::ChargeFull, win = PlayerControl::ChargeWindow;
      p.setBrush(Brush(Color(0.f,0.f,0.f,0.7f), Painter::Alpha));
      p.drawRect(sp.x-bw/2-2, sp.y-2, bw+4, bh+4);
      if(t<full) {
        p.setBrush(Brush(Color(0.85f,0.85f,0.85f,0.85f), Painter::Alpha));
        p.drawRect(sp.x-bw/2, sp.y, int(float(bw)*t/full), bh);
        }
      else if(t<=full+win) {
        p.setBrush(Brush(Color(1.f,0.8f,0.2f,1.f), Painter::Alpha));
        p.drawRect(sp.x-bw/2-2, sp.y-2, bw+4, bh+4);
        }
      else {
        p.setBrush(Brush(Color(0.85f,0.15f,0.1f,0.9f), Painter::Alpha));
        p.drawRect(sp.x-bw/2, sp.y, bw, bh);
        }
      }
  }

  // slash of the charged attack: a bright 180 degree crescent sweeping around the hero
  {
    const auto&    fx  = player.arpgSlash();
    const uint64_t now = world->tickCount();
    if(fx.time!=0 && now>=fx.time && now<fx.time+600) {
      const float t     = float(now-fx.time)/1000.f;
      const float sweep = std::clamp((t-0.06f)/0.18f, 0.f, 1.f);   // follows the swing
      const float fade  = 1.f - std::clamp((t-0.25f)/0.35f, 0.f, 1.f);
      if(sweep>0.f && fade>0.f) {
        const int   N  = 24;
        const float r0 = 90.f, r1 = 260.f, y = 110.f;
        auto pt = [&](float a, float r, Point& out) {
          const float rad = a*float(M_PI)/180.f;
          return toScreen(fx.pos + Vec3(std::cos(rad)*r, y, std::sin(rad)*r), out);
          };
        p.setBrush(Brush(Color(1.f,0.85f,0.45f,1.f), Painter::Add));
        for(int i=0; i<N; ++i) {
          const float k0 = float(i)/float(N), k1 = float(i+1)/float(N);
          if(k1>sweep)
            break;
          // the newest part of the sweep is brightest
          const float a  = fade*(0.25f + 0.75f*std::clamp(1.f-(sweep-k1)*2.f, 0.f, 1.f));
          const float a0 = fx.ang + fx.dir*(90.f - 180.f*k0);
          const float a1 = fx.ang + fx.dir*(90.f - 180.f*k1);
          Point p00, p01, p10, p11;
          if(!pt(a0,r0,p00) || !pt(a0,r1,p01) || !pt(a1,r0,p10) || !pt(a1,r1,p11))
            continue;
          p.setBrush(Brush(Color(1.f*a, 0.82f*a, 0.4f*a, 1.f), Painter::Add));
          p.drawTriangle(float(p00.x),float(p00.y),0,0, float(p01.x),float(p01.y),0,0, float(p11.x),float(p11.y),0,0);
          p.drawTriangle(float(p00.x),float(p00.y),0,0, float(p11.x),float(p11.y),0,0, float(p10.x),float(p10.y),0,0);
          }
        }
      }
  }

  // special attack cooldown: thin bar under the hero's feet
  {
    const float left = player.arpgSpecialLeft();
    if(left>0.f) {
      Point sp;
      if(toScreen(pl->position()-Vec3(0,20.f,0), sp)) {
        const int bw = int(50.f*scale), bh = std::max(2, int(4.f*scale));
        p.setBrush(Brush(Color(0.f,0.f,0.f,0.6f), Painter::Alpha));
        p.drawRect(sp.x-bw/2-1, sp.y-1, bw+2, bh+2);
        p.setBrush(Brush(Color(0.35f,0.6f,1.f,0.9f), Painter::Alpha));
        p.drawRect(sp.x-bw/2, sp.y, int(float(bw)*(1.f-left)), bh);
        }
      }
  }
  }

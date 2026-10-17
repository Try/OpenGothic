#include "mainwindow.h"
#include "ui/cursorshape.h"

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
#include "world/objects/item.h"
#include "game/serialize.h"
#include "game/globaleffects.h"
#include "utils/gthfont.h"
#include "utils/haptics.h"
#include "utils/dbgpainter.h"

#include "commandline.h"
#include "gothic.h"

using namespace Tempest;

MainWindow::MainWindow(Device& device)
  : Window(Maximized),device(device),swapchain(device,hwnd()),
    atlas(device),rootMenu(keycodec),inventory(keycodec),
    dialogs(inventory),document(keycodec),
    console(*this),
#if defined(__MOBILE_PLATFORM__) && !defined(OPENGOTHIC_TOUCH_UI)
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
#ifdef OPENGOTHIC_TOUCH_UI
  setupMobileControls();
#endif
  }

MainWindow::~MainWindow() {
#ifdef OPENGOTHIC_TOUCH_UI
  androidControls.unbind();
#endif
  GameMusic::inst().stopMusic();
  Gothic::inst().cancelLoading();
  device.waitIdle();
  takeWidget(&dialogs);
  takeWidget(&inventory);
  takeWidget(&chapter);
  takeWidget(&document);
  takeWidget(&video);
  takeWidget(&rootMenu);
#if defined(__MOBILE_PLATFORM__) && !defined(OPENGOTHIC_TOUCH_UI)
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
#if defined(__MOBILE_PLATFORM__) && !defined(OPENGOTHIC_TOUCH_UI)
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
      paintFocus(p,focus,vp);

#ifdef OPENGOTHIC_TOUCH_UI
      constexpr bool screenBars = false;
#else
      const bool     screenBars = !Gothic::inst().isDesktop();
#endif
      if(auto pl = Gothic::inst().player()){
#ifdef OPENGOTHIC_TOUCH_UI
        drawTouchBars(p,*pl);
#endif
        if (screenBars) {
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

  if(SystemApi::isFullscreen(hwnd())) {
    setCursorPosition(Point(w()/2,h()/2));
    }

  dMouse = Point();
  }

void MainWindow::mouseDownEvent(MouseEvent &event) {
#if defined(OPENGOTHIC_TOUCH_UI) && !defined(__ANDROID__)
  // The mouse emulates a touch screen: left button is a finger, right button is "back"
  if(event.button==Event::ButtonLeft)
    androidControls.pointerDown(0,float(event.x),float(event.y)); else
  if(event.button==Event::ButtonRight)
    androidControls.physical(Mobile::Key::Escape,true);
  return;
#endif
  if(event.button<sizeof(mouseP))
    mouseP[event.button]=true;
  auto act     = keycodec.tr(event);
  auto mapping = keycodec.mapping(event);
  player.onKeyPressed(act,KeyEvent::K_NoKey,mapping);
  }

void MainWindow::mouseUpEvent(MouseEvent &event) {
#if defined(OPENGOTHIC_TOUCH_UI) && !defined(__ANDROID__)
  if(event.button==Event::ButtonLeft)
    androidControls.pointerUp(0,float(event.x),float(event.y)); else
  if(event.button==Event::ButtonRight)
    androidControls.physical(Mobile::Key::Escape,false);
  return;
#endif
  auto act     = keycodec.tr(event);
  auto mapping = keycodec.mapping(event);
  player.onKeyReleased(act,mapping);
  if(event.button<sizeof(mouseP))
    mouseP[event.button]=false;
  }

void MainWindow::mouseDragEvent(MouseEvent &event) {
#if defined(OPENGOTHIC_TOUCH_UI) && !defined(__ANDROID__)
  if(event.button==Event::ButtonLeft)
    androidControls.pointerMove(0,float(event.x),float(event.y));
  return;
#endif
  const bool fs = SystemApi::isFullscreen(hwnd());
  if(!mouseP[Event::ButtonLeft] && !fs)
    return;
  if(player.focus().npc && !fs)
    return;
  processMouse(event,true);
  }

void MainWindow::mouseMoveEvent(MouseEvent &event) {
#if defined(OPENGOTHIC_TOUCH_UI) && !defined(__ANDROID__)
  (void)event;
  return;
#endif
  processMouse(event,SystemApi::isFullscreen(hwnd()));
  }

void MainWindow::processMouse(MouseEvent& event, bool enable) {
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

void MainWindow::onSettings() {
  auto zMaxFps = Gothic::options().fpsLimit;
  if(zMaxFps<=0)
    zMaxFps = Gothic::inst().settingsGetI("ENGINE", "zMaxFps");
#ifdef OPENGOTHIC_TOUCH_UI
  zMaxFps = std::clamp(AndroidInput::preferences().fps,30,60);
#endif
  if(zMaxFps>0)
    maxFpsInv = 1000u/uint64_t(zMaxFps); else
    maxFpsInv = 0;
  }

void MainWindow::mouseWheelEvent(MouseEvent &event) {
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
#ifdef OPENGOTHIC_TOUCH_UI
  if(!event.in) {
    androidControls.cancel();
    clearInput();
    dMouse = Point();
    }
#endif
  if(!event.in)
    return;
  dMouse = Point();

  if(SystemApi::isFullscreen(hwnd()))
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
  const float scale = Gothic::interfaceScale(this);
  drawBar(p,bar,x,y,int(200.f*scale*float(std::min(w(),800))/800.f),v,flg);
  }

void MainWindow::drawBar(Painter &p, const Tempest::Texture2d* bar, int x, int y, int width, float v, AlignFlag flg) {
  if(barBack==nullptr || bar==nullptr)
    return;
  const float destW   = float(width);
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

#ifdef OPENGOTHIC_TOUCH_UI
/// The bars of the original interface: life on the left above the hotbar, mana on the right, oxygen under water
void MainWindow::drawTouchBars(Painter& p, Npc& pl) {
  const auto mode = mobileContext();
  const float hp  = float(pl.attribute(ATR_HITPOINTS))/float(std::max(1,pl.attribute(ATR_HITPOINTSMAX)));
  const float mp  = float(pl.attribute(ATR_MANA))/float(std::max(1,pl.attribute(ATR_MANAMAX)));

  if(mode==Mobile::Mode::Inventory) {
    const int width = int(200.f*Gothic::interfaceScale(this)*float(std::min(w(),800))/800.f);
    drawBar(p,barHp,10,10,width,hp,AlignLeft);
    drawBar(p,barMana,10,10+int(float(width)*0.125f)+2,width,mp,AlignLeft);
    return;
    }
  if(mode!=Mobile::Mode::Game)
    return;

  // above the hotbar of the controls, same units as the model
  const float s     = std::min(float(w())/1000.f,float(h())/440.f);
  const int   half  = int(Mobile::HotbarSlot*5*s);
  const int   width = int(Mobile::HotbarSlot*3.2f*s);   // the frame keeps its proportions, so it is not too tall
  const int   y     = h()-int((Mobile::HotbarBottom+Mobile::HotbarSlot+9)*s);

  // the bars stand over the ends of the hotbar
  drawBar(p,barHp,w()/2-half,y,width,hp,AlignLeft | AlignBottom);
  const int xr = w()/2+half-width;
  if(pl.isDive()) {
    const float total = float(pl.world().script().guildVal().dive_time[pl.guild()]);
    const float left  = total>0 ? (total-float(pl.diveTime())/1000.f)/total : 0.f;
    drawBar(p,barMisc,xr,y,width,left,AlignLeft | AlignBottom);
    } else {
    drawBar(p,barMana,xr,y,width,mp,AlignLeft | AlignBottom);
    }
  }
#endif

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
#ifdef OPENGOTHIC_TOUCH_UI
  updateMobileControls();
#endif
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
  tickMouse(dt);
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
  setCursorShape(fs ? HiddenCursor : CursorShape::Arrow);
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

#ifdef OPENGOTHIC_TOUCH_UI
    updateMobileOverlay();
#endif
    // Models of the inventory are drawn over everything else, hide them below a document
    inventory.setCovered(document.isActive() || chapter.isActive() || video.isActive());

    CommandBuffer& cmd = commands[cmdId];
    {
    auto enc = cmd.startEncoding(device);
    renderer.draw(swapchain[swapchain.currentImage()],enc,cmdId,uiMesh[cmdId],numMesh[cmdId],inventory,video);
#ifdef OPENGOTHIC_TOUCH_UI
    // The last pass: controls stay above menus, video and 3D inventory
    enc.setViewport(0,0,w(),h());
    touchMesh[cmdId].draw(enc);
    if(mobileContext()==Mobile::Mode::Game && !androidControls.settingsVisible())
      renderer.drawTouchItems(swapchain[swapchain.currentImage()],enc,mobileItems[cmdId]);
#endif
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

#ifdef OPENGOTHIC_TOUCH_UI
static_assert(Mobile::CategoryCount==InventoryMenu::CatCount, "touch categories do not match the inventory");

namespace {
// Hotbar slots: melee weapon, ranged weapon and the magic slots
const Item* hotbarItem(Npc& pl, int index) {
  if(index==0)
    return pl.currentMeleeWeapon();
  if(index==1)
    return pl.currentRangedWeapon();
  return pl.inventory().currentSpell(uint8_t(index-2));
  }

void cycleSpell(Npc& pl) {
  constexpr int count = 8; // magic slots 3..10
  const int slot  = int(pl.inventory().currentSpellSlot());
  const int start = (3<=slot && slot<=10) ? slot-3 : -1;
  for(int n=1; n<=count; ++n) {
    const int next = (start+n)%count;
    if(pl.inventory().currentSpell(uint8_t(next))) {
      pl.drawMage(uint8_t(next+3));
      break;
      }
    }
  }
}

Mobile::Mode MainWindow::mobileContext() const {
  if(Gothic::inst().checkLoading()!=Gothic::LoadState::Idle)
    return Mobile::Mode::Loading;
  if(video.isActive() || chapter.isActive() || document.isActive())
    return Mobile::Mode::Video;
  if(rootMenu.isActive())
    return rootMenu.isTouchMenu() ? Mobile::Mode::Menu : Mobile::Mode::Dialog;
  if(inventory.isOpen()==InventoryMenu::State::LockPicking)
    return Mobile::Mode::Lockpick;
  if(inventory.isActive())
    return Mobile::Mode::Inventory;
  if(dialogs.isActive())
    return Mobile::Mode::Dialog;
  return Gothic::inst().isInGame() ? Mobile::Mode::Game : Mobile::Mode::Loading;
  }

void MainWindow::setupMobileControls() {
  androidControls.update(*this,mobileContext());

  androidControls.look = [this](float dx, float dy) {
    auto camera = Gothic::inst().camera();
    if(!hasFocus() || mobileContext()!=Mobile::Mode::Game || camera==nullptr ||
       camera->isCutscene() || Gothic::inst().isPause())
      return;
    if(Gothic::inst().settingsGetI("GAME","camLookaroundInverse"))
      dy = -dy;
    camera->onRotateMouse(PointF(dy,-dx));
    player.onRotateMouse(-dx,-dy);
    };
  androidControls.menuTouch = [this](float x, float y, int phase) {
    if(rootMenu.isActive())
      rootMenu.mobileTouch(x,y,phase); else
    if(inventory.isActive())
      inventory.mobileTouch(x,y,phase); else
    if(dialogs.isActive())
      dialogs.mobileTouch(x,y,phase);
    };
  androidControls.menuTap = [this](float x, float y) {
    if(rootMenu.isActive())
      rootMenu.mobileTap(x,y); else
    if(inventory.isActive())
      inventory.mobileTap(x,y); else
    if(dialogs.isActive())
      dialogs.mobileTap(x,y);
    };
  androidControls.menuScroll = [this](float dy) {
    if(rootMenu.isActive())
      rootMenu.mobileScroll(dy); else
    if(inventory.isActive())
      inventory.mobileScroll(dy);
    };
  androidControls.action = [this](int id) {
    onMobileAction(id);
    };

  androidControls.loadIcons(atlas);
  inventory.setTouchSprites(androidControls.icon(AndroidInput::IconInvSlot),
                            androidControls.icon(AndroidInput::IconInvSlotSelected));
  androidControls.bind();
  }

void MainWindow::onMobileAction(int id) {
  const int category = id-Mobile::Special::FirstCategory;
  if(0<=category && category<Mobile::CategoryCount) {
    if(mobileContext()==Mobile::Mode::Inventory)
      inventory.setCategory(category);
    return;
    }

  if(mobileContext()!=Mobile::Mode::Game)
    return;
  if(id==Mobile::Special::Target)
    player.cycleTarget();
  if(id==Mobile::Special::Spell) {
    if(auto pl = Gothic::inst().player())
      cycleSpell(*pl);
    }
  }

/// Damage is felt: the stronger the hit, the longer and stronger the vibration
void MainWindow::updateHaptics() {
  auto pl = Gothic::inst().player();
  if(pl==nullptr) {
    lastHitpoints = -1;
    return;
    }

  const int hp = pl->attribute(ATR_HITPOINTS);
  if(lastHitpoints>0 && hp<lastHitpoints && AndroidInput::preferences().vibration!=0) {
    const int lost = lastHitpoints-hp;
    if(hp<=0)
      Haptics::vibrate(450,255); else
      Haptics::vibrate(std::clamp(30+lost*2,30,160),std::clamp(90+lost*5,90,255));
    }
  lastHitpoints = hp;
  }

void MainWindow::updateMobileControls() {
  updateHaptics();
  updateMobileAvailability();
  androidControls.inventoryLists(inventory.mobileTwoLists());
  androidControls.gui(Gothic::interfaceScale(this));
  if(androidControls.update(*this,mobileContext())) {
    clearInput();
    dMouse = Point();
    }
  }

/// Controls of actions that are not possible at the moment are hidden or disabled
void MainWindow::updateMobileAvailability() {
  using Mobile::Key;
  using Mobile::keyMask;

  uint64_t allowed = keyMask(Key::Escape);
  const auto mode  = mobileContext();

  if(mode==Mobile::Mode::Game) {
    bool target = false, spells = false, movement = false;
    if(auto pl = Gothic::inst().player()) {
      const auto camera = Gothic::inst().camera();
      const bool alive  = !pl->isDead() && !pl->isDown();
      const bool active = !Gothic::inst().isPause() && camera!=nullptr && !camera->isCutscene();
      const bool free   = alive && pl->interactive()==nullptr && active;
      const bool ground = free && !pl->isSwim() && !pl->isDive();

      const auto ws     = pl->weaponState();
      const bool armed  = ws!=WeaponState::NoWeapon;
      const bool ranged = ws==WeaponState::Bow || ws==WeaponState::CBow;
      const bool melee  = armed && !ranged && ws!=WeaponState::Mage;

      movement = alive && active;
      if(alive)
        allowed |= keyMask(Key::Status) | keyMask(Key::Log) | keyMask(Key::Map);
      if(ground) {
        allowed |= keyMask(Key::Inventory) | keyMask(Key::Weapon) | keyMask(Key::Walk);
        if(pl->canSneak())
          allowed |= keyMask(Key::Sneak);

        const bool canAttack = armed && (!ranged || pl->hasAmmunition()) &&
                               (ws!=WeaponState::Mage || pl->attribute(ATR_MANA)>0);
        if(canAttack || (!armed && bool(player.focus())))
          allowed |= keyMask(Key::Action);
        if(melee)
          allowed |= keyMask(Key::AttackLeft) | keyMask(Key::AttackRight) | keyMask(Key::Block);

        target = armed && player.focus().npc!=nullptr;

        int spellCount = 0;
        for(int i=0; i<10; ++i) {
          if(hotbarItem(*pl,i)==nullptr)
            continue;
          allowed |= keyMask(Key(int(Key::One)+i));
          if(i>=2)
            ++spellCount;
          }
        spells = ws==WeaponState::Mage && spellCount>1;
        }
      if(free && !pl->isInAir() && !pl->isJump())
        allowed |= keyMask(Key::Jump);
      if(alive && pl->interactive()!=nullptr)
        allowed |= keyMask(Key::Action);
      }
    androidControls.availability(allowed,target,spells,movement);
    return;
    }

  if(mode==Mobile::Mode::Inventory) {
    if(inventory.hasSelectedItem()) {
      allowed |= keyMask(Key::Enter) | keyMask(Key::Weapon) |
                 keyMask(Key::Up) | keyMask(Key::Down) | keyMask(Key::Left) | keyMask(Key::Right);
      if(inventory.isOpen()!=InventoryMenu::State::Equip)
        allowed |= keyMask(Key::Jump);
      }
    androidControls.availability(allowed,false,false,false);
    }
  }

/// Prepares the touch HUD for rendering: status bars, hotbar models and the controls themselves
void MainWindow::updateMobileOverlay() {
  struct HotbarItem {
    const Item*  item;
    Mobile::Rect rect;
    };
  std::vector<HotbarItem> hotbar;
  std::string             stamp;

  auto& hud = androidControls.hud;
  hud = {};
  hud.inventoryCategory = inventory.category();
  hud.inventoryTransfer = inventory.isOpen()!=InventoryMenu::State::Equip;
  if(mobileContext()==Mobile::Mode::Inventory)
    hud.useLabel = std::string(inventory.mobileActionLabel());

  const auto mode = mobileContext();
  auto       pl   = Gothic::inst().player();
  if((mode==Mobile::Mode::Game || mode==Mobile::Mode::Inventory) && !androidControls.settingsVisible() && pl!=nullptr) {
    const auto ws = pl->weaponState();
    if(ws==WeaponState::NoWeapon) {
      const auto focus = player.focus();
      hud.actionIcon = focus.npc!=nullptr         ? AndroidInput::IconTalk    :
                       focus.interactive!=nullptr ? AndroidInput::IconContext : AndroidInput::IconInteract;
      } else
    if(ws==WeaponState::Mage)
      hud.actionIcon = AndroidInput::IconCast; else
    if(ws==WeaponState::Bow || ws==WeaponState::CBow)
      hud.actionIcon = AndroidInput::IconShoot; else
      hud.actionIcon = AndroidInput::IconAttack;

    hud.health      = float(pl->attribute(ATR_HITPOINTS))/float(std::max(1,pl->attribute(ATR_HITPOINTSMAX)));
    hud.mana        = float(pl->attribute(ATR_MANA))/float(std::max(1,pl->attribute(ATR_MANAMAX)));
    hud.hp          = pl->attribute(ATR_HITPOINTS);
    hud.hpMax       = pl->attribute(ATR_HITPOINTSMAX);
    hud.mp          = pl->attribute(ATR_MANA);
    hud.mpMax       = pl->attribute(ATR_MANAMAX);
    hud.diving      = pl->isDive();
    hud.manaVisible = ws==WeaponState::Mage || pl->attribute(ATR_MANA)<pl->attribute(ATR_MANAMAX);

    const auto diveTime = float(pl->world().script().guildVal().dive_time[pl->guild()]);
    hud.oxygen = diveTime>0 ? 1.f-float(pl->diveTime())/(1000.f*diveTime) : 0.f;

    for(const auto& b:androidControls.buttons()) {
      if(!b.visible || b.key<Mobile::Key::One || b.key>Mobile::Key::Zero)
        continue;
      const int   index = int(b.key)-int(Mobile::Key::One);
      const Item* item  = hotbarItem(*pl,index);
      if(item==nullptr)
        continue;
      if(item==pl->activeWeapon())
        hud.active = index;
      hotbar.push_back({item,b.rect});
      stamp += std::string(item->handle().visual)+":"+std::to_string(index)+":"+
               std::to_string(b.rect.x)+":"+std::to_string(b.rect.y)+":"+
               std::to_string(b.rect.w)+":"+std::to_string(b.rect.h)+";";
      }
    }

  // Models are cached, and rebuilt only when the hotbar changes
  if(stamp!=mobileItemStamp[cmdId]) {
    mobileItems[cmdId].reset();
    for(const auto& slot:hotbar) {
      const auto& r = slot.rect;
      mobileItems[cmdId].drawItem(int(r.x+r.w*0.12f),int(r.y+r.h*0.22f),int(r.w*0.76f),int(r.h*0.66f),*slot.item);
      }
    mobileItemStamp[cmdId] = std::move(stamp);
    }

  touchLayer.clear();
  {
    PaintEvent p(touchLayer,atlas,w(),h());
    androidControls.paint(p);
  }
  touchMesh[cmdId].update(device,touchLayer);
  }
#endif

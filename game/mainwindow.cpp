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
    atlas(device),renderer(swapchain),
    rootMenu(keycodec),inventory(keycodec),
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

  if(!CommandLine::inst().isWindowMode())
    setFullscreen(true);

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
      paintFocus(p,focus,vp);

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

  if(Gothic::inst().doClock() && world!=nullptr) {
    if (!Gothic::inst().isDesktop()) {
      auto hour = world->time().hour();
      auto min  = world->time().minute();
      auto& fnt = Resources::font(scale);
      string_frm clockT(int(hour),":",int(min));
      fnt.drawText(p,w()-fnt.textSize(clockT).w-5,fnt.pixelSize()+5,clockT);
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
  renderer.resetSwapchain();
  if(auto camera = Gothic::inst().camera())
    camera->setViewport(swapchain.w(),swapchain.h());

  const bool fs = SystemApi::isFullscreen(hwnd());
  auto rect = SystemApi::windowClientRect(hwnd());
  setCursorPosition(rect.w/2,rect.h/2);
  setCursorShape(fs ? CursorShape::Hidden : CursorShape::Arrow);
  dMouse = Point();
  }

void MainWindow::mouseDownEvent(MouseEvent &event) {
  if(event.button<sizeof(mouseP))
    mouseP[event.button]=true;
  player.onKeyPressed(keycodec.tr(event),KeyEvent::K_NoKey);
  }

void MainWindow::mouseUpEvent(MouseEvent &event) {
  player.onKeyReleased(keycodec.tr(event));
  if(event.button<sizeof(mouseP))
    mouseP[event.button]=false;
  }

void MainWindow::mouseDragEvent(MouseEvent &event) {
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

  auto act = keycodec.tr(event);
  auto mapping = keycodec.mapping(event);
  player.onKeyPressed(act,event.key,mapping);

  if(event.key==Event::K_F11) {
    auto tex = renderer.screenshoot(cmdId);
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
  if(!event.in)
    return;
  dMouse = Point();
  auto center = Point(w()/2,h()/2);
  setCursorPosition(center);
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

    auto b    = focus.npc->bounds();
    Vec3 bx[] = {
      {b.bbox[0].x,b.bbox[0].y,b.bbox[0].z},
      {b.bbox[1].x,b.bbox[0].y,b.bbox[0].z},
      {b.bbox[1].x,b.bbox[1].y,b.bbox[0].z},
      {b.bbox[0].x,b.bbox[1].y,b.bbox[0].z},
      {b.bbox[0].x,b.bbox[0].y,b.bbox[1].z},
      {b.bbox[1].x,b.bbox[0].y,b.bbox[1].z},
      {b.bbox[1].x,b.bbox[1].y,b.bbox[1].z},
      {b.bbox[0].x,b.bbox[1].y,b.bbox[1].z},
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
  if(auto camera = Gothic::inst().camera()) {
    if(camera->isFree())
      return Camera::Normal;
    }

  if(inventory.isOpen()==InventoryMenu::State::Equip ||
     inventory.isOpen()==InventoryMenu::State::Ransack)
    return Camera::Inventory;

  if(auto pl=Gothic::inst().player()) {
    if(pl->interactive()!=nullptr)
      return Camera::Mobsi;
    }

  if(dialogs.isActive())
    return Camera::Dialog;

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
  if(Gothic::inst().checkLoading()==Gothic::LoadState::Idle){
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
  auto tex = renderer.screenshoot(cmdId);
  auto pm  = device.readPixels(textureCast<const Texture2d&>(tex));

  if(dialogs.isActive())
    return;
  if(auto w = Gothic::inst().world(); w!=nullptr && w->currentCs()!=nullptr)
    return;

  Gothic::inst().startSave(std::move(textureCast<Texture2d&>(tex)),[slot=std::string(slot),name=std::string(name),pm](std::unique_ptr<GameSession>&& game){
    if(!game)
      return std::move(game);

    Tempest::WFile f(slot);
    Serialize      s(f);
    game->save(s,name,pm);

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
  Gothic::inst().setGame(std::move(w));
  }

void MainWindow::clearInput() {
  player.clearInput();
  std::memset(mouseP,0,sizeof(mouseP));
  }

void MainWindow::setFullscreen(bool fs) {
  SystemApi::setAsFullscreen(hwnd(),fs);
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
    renderer.draw(enc,cmdId,swapchain.currentImage(),uiMesh[cmdId],numMesh[cmdId],inventory,video);
    }
    sync = device.submit(cmd);
    device.present(swapchain);
    cmdId = (cmdId+1u)%Resources::MaxFramesInFlight;

    auto t = Application::tickCount();
    if(t-time<16 && !Gothic::inst().isInGame() && !video.isActive()) {
      uint32_t delay = uint32_t(16-(t-time));
      Application::sleep(delay);
      t += delay;
      }
    else if(maxFpsInv>0 && t-time<maxFpsInv) {
      uint32_t delay = uint32_t(maxFpsInv-(t-time));
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
    renderer.resetSwapchain();
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

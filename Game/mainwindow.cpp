#include "mainwindow.h"

#include <Tempest/Except>
#include <Tempest/Painter>

#include <Tempest/Brush>
#include <Tempest/Pen>
#include <Tempest/Layout>
#include <Tempest/Application>
#include <Tempest/Log>

#include "ui/dialogmenu.h"
#include "ui/gamemenu.h"
#include "ui/menuroot.h"
#include "ui/stacklayout.h"
#include "ui/videowidget.h"

#include "world/objects/npc.h"
#include "game/serialize.h"
#include "game/globaleffects.h"
#include "utils/crashlog.h"
#include "utils/gthfont.h"
#include "utils/dbgpainter.h"

#include "gothic.h"

using namespace Tempest;

MainWindow::MainWindow(Gothic &gothic, Device& device)
  : Window(Maximized),device(device),swapchain(device,hwnd()),
    atlas(device),renderer(swapchain,gothic),
    gothic(gothic),keycodec(gothic),
    rootMenu(gothic,keycodec),video(gothic),inventory(gothic,keycodec,renderer.storage()),
    dialogs(gothic,inventory),document(gothic,keycodec),chapter(gothic),console(gothic),
    player(gothic,dialogs,inventory) {
  CrashLog::setGpu(device.renderer());
  if(!gothic.isWindowMode())
    setFullscreen(true);

  for(uint8_t i=0;i<device.maxFramesInFlight();++i)
    fLocal.emplace_back(device);

  renderer.resetSwapchain();
  setupUi();

  barBack    = Resources::loadTexture("BAR_BACK.TGA");
  barHp      = Resources::loadTexture("BAR_HEALTH.TGA");
  barMisc    = Resources::loadTexture("BAR_MISC.TGA");
  barMana    = Resources::loadTexture("BAR_MANA.TGA");

  focusImg   = Resources::loadTexture("FOCUS_HIGHLIGHT.TGA");

  background = Resources::loadTexture("STARTSCREEN.TGA");
  loadBox    = Resources::loadTexture("PROGRESS.TGA");
  loadVal    = Resources::loadTexture("PROGRESS_BAR.TGA");

  gothic.onStartGame    .bind(this,&MainWindow::startGame);
  gothic.onLoadGame     .bind(this,&MainWindow::loadGame);
  gothic.onSaveGame     .bind(this,&MainWindow::saveGame);

  gothic.onStartLoading .bind(this,&MainWindow::onStartLoading);
  gothic.onWorldLoaded  .bind(this,&MainWindow::onWorldLoaded);
  gothic.onSessionExit  .bind(this,&MainWindow::onSessionExit);

  gothic.onVideo        .bind(this,&MainWindow::onVideo);

  if(!gothic.defaultSave().empty()){
    gothic.load(gothic.defaultSave());
    rootMenu.popMenu();
    }
  else if(!gothic.doStartMenu()) {
    startGame(gothic.defaultWorld());
    rootMenu.popMenu();
    }
  else {
    GameMusic::inst().setMusic(GameMusic::SysMenu);
    }
  }

MainWindow::~MainWindow() {
  GameMusic::inst().stopMusic();
  gothic.cancelLoading();
  device.waitIdle();
  takeWidget(&dialogs);
  takeWidget(&inventory);
  takeWidget(&chapter);
  takeWidget(&document);
  takeWidget(&video);
  takeWidget(&rootMenu);
  removeAllWidgets();
  // unload
  gothic.setGame(std::unique_ptr<GameSession>());
  }

void MainWindow::setupUi() {
  setLayout(new StackLayout());
  addWidget(&document);
  addWidget(&dialogs);
  addWidget(&inventory);
  addWidget(&chapter);
  addWidget(&video);
  addWidget(&rootMenu);

  rootMenu.setMenu("MENU_MAIN");

  gothic.onDialogPipe  .bind(&dialogs,&DialogMenu::openPipe);
  gothic.isDialogClose .bind(&dialogs,&DialogMenu::aiIsClose);

  gothic.onPrintScreen .bind(&dialogs,&DialogMenu::printScreen);
  gothic.onPrint       .bind(&dialogs,&DialogMenu::print);

  gothic.onIntroChapter.bind(&chapter, &ChapterScreen::show);
  gothic.onShowDocument.bind(&document,&DocumentMenu::show);
  }

void MainWindow::paintEvent(PaintEvent& event) {
  Painter p(event);
  auto world = gothic.world();
  auto st    = gothic.checkLoading();

  const char* info="";

  if(world==nullptr && background!=nullptr) {
    p.setBrush(Color(0.0));
    p.drawRect(0,0,w(),h());

    if(st==Gothic::LoadState::Idle) {
      p.setBrush(Brush(*background,Painter::NoBlend));
      p.drawRect(0,0,w(),h(),
                 0,0,background->w(),background->h());
      }
    }

  if(world!=nullptr) {
    world->globalFx()->scrBlend(p,Rect(0,0,w(),h()));
    }

  if(st!=Gothic::LoadState::Idle && st!=Gothic::LoadState::Finalize) {
    if(st==Gothic::LoadState::Saving) {
      drawSaving(p);
      } else {
      if(auto back = gothic.loadingBanner()) {
        p.setBrush(Brush(*back,Painter::NoBlend));
        p.drawRect(0,0,this->w(),this->h(),
                   0,0,back->w(),back->h());
        }
      if(loadBox!=nullptr)
        drawLoading(p,int(w()*0.92)-loadBox->w(), int(h()*0.12), loadBox->w(),loadBox->h());
      }
    } else {
    if(world!=nullptr && world->view()){
      auto& camera = *gothic.camera();

      auto vp = camera.viewProj();
      p.setBrush(Color(1.0));

      auto focus = world->validateFocus(player.focus());
      paintFocus(p,focus,vp);

      if(auto pl=gothic.player()){
        float hp = float(pl->attribute(Npc::ATR_HITPOINTS))/float(pl->attribute(Npc::ATR_HITPOINTSMAX));
        float mp = float(pl->attribute(Npc::ATR_MANA))     /float(pl->attribute(Npc::ATR_MANAMAX));
        drawBar(p,barHp,  10,    h()-10, hp, AlignLeft  | AlignBottom);
        drawBar(p,barMana,w()-10,h()-10, mp, AlignRight | AlignBottom);
        if(pl->isDive()) {
          int32_t gl = pl->guild();
          auto    v  = float(pl->world().script().guildVal().dive_time[gl]);
          if(v>0) {
            auto t = float(pl->diveTime())/1000.f;
            drawBar(p,barMisc,w()/2,h()-10, (v-t)/(v), AlignHCenter | AlignBottom);
            }
          }
        }
      }

    if(world && world->player()) {
      if(world->player()->hasCollision())
        info="[c]";
      else
        info = world->roomAt(world->player()->position()).c_str();
      }
    }

  if(auto c = gothic.camera()) {
    DbgPainter dbg(p,c->viewProj(),w(),h());
    c->debugDraw(dbg);
    if(world!=nullptr) {
      world->marchPoints(dbg);
      world->marchInteractives(dbg);
      }
    // world->view()->dbgLights(p);
    }

  if(gothic.doFrate()) {
    char fpsT[64]={};
    std::snprintf(fpsT,sizeof(fpsT),"fps = %.2f %s",fps.get(),info);

    auto& fnt = Resources::font();
    fnt.drawText(p,5,30,fpsT);
    }
  }

void MainWindow::resizeEvent(SizeEvent&) {
  for(auto& i:fLocal)
    i.gpuLock.wait();
  swapchain.reset();
  renderer.resetSwapchain();
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
  if(enable && event.pos()!=center) {
    dMouse += (event.pos()-center);
    SystemApi::setCursorPosition(hwnd(),center.x,center.y);
    }
  }

void MainWindow::tickMouse() {
  if(dialogs.isActive() || gothic.isPause()) {
    dMouse = Point();
    return;
    }

  const bool enableMouse = gothic.settingsGetI("GAME","enableMouse");
  if(enableMouse==0) {
    dMouse = Point();
    return;
    }

  const float mouseSensitivity = gothic.settingsGetF("GAME","mouseSensitivity");
  PointF dpScaled = PointF(float(dMouse.x)*mouseSensitivity,float(dMouse.y)*mouseSensitivity);
  dpScaled.x/=float(w());
  dpScaled.y/=float(h());

  dpScaled*=1000.f;
  dpScaled.y /= 7.f;

  if(auto camera = gothic.camera())
    camera->onRotateMouse(PointF(dpScaled.y,-dpScaled.x));
  if(!inventory.isActive()) {
    player.onRotateMouse  (-dpScaled.x);
    player.onRotateMouseDy(-dpScaled.y);
    }

  dMouse = Point();
  }

void MainWindow::mouseWheelEvent(MouseEvent &event) {
  if(auto camera = gothic.camera())
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
  player.onKeyPressed(act,event.key);

  if(event.key==Event::K_F9) {
    auto tex = renderer.screenshoot(swapchain.frameId());
    auto pm  = device.readPixels(textureCast(tex));
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
    rootMenu.keyUpEvent(event);
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

  if(event.key==KeyEvent::K_F2) {
    console.resize(w(),h());
    console.setFocus(true);
    console.exec();
    }
  else if(event.key==KeyEvent::K_F3) {
    setFullscreen(!SystemApi::isFullscreen(hwnd()));
    }
  else if(event.key==KeyEvent::K_F5){
    gothic.quickSave();
    }

  const char* menuEv=nullptr;

  auto act = keycodec.tr(event);
  if(act==KeyCodec::Escape)
    menuEv="MENU_MAIN";
  else if(act==KeyCodec::Log)
    menuEv="MENU_LOG";
  else if(act==KeyCodec::Status)
    menuEv="MENU_STATUS";

  if(menuEv!=nullptr) {
    rootMenu.setMenu(menuEv,act);
    if(auto pl = gothic.player())
      rootMenu.setPlayer(*pl);
    clearInput();
    }
  else if(act==KeyCodec::Inventory){
    if(inventory.isActive()) {
      inventory.close();
      } else {
      auto pl = gothic.player();
      if(pl!=nullptr)
        inventory.open(*pl);
      }
    clearInput();
    }
  player.onKeyReleased(act);
  }

void MainWindow::paintFocus(Painter& p, const Focus& focus, const Matrix4x4& vp) {
  if(!focus || dialogs.isActive())
    return;

  auto world = gothic.world();
  auto pl    = world==nullptr ? nullptr : world->player();

  auto pos = focus.displayPosition();
  vp.project(pos.x,pos.y,pos.z);

  int   ix = int((0.5f*pos.x+0.5f)*float(w()));
  int   iy = int((0.5f*pos.y+0.5f)*float(h()));
  auto& fnt = Resources::font();

  auto tsize = fnt.textSize(focus.displayName());
  ix-=tsize.w/2;
  if(iy<tsize.h)
    iy = tsize.h;
  if(iy>h())
    iy = h();
  fnt.drawText(p,ix,iy,focus.displayName());

  if(focus.npc!=nullptr) {
    float hp = float(focus.npc->attribute(Npc::ATR_HITPOINTS))/float(focus.npc->attribute(Npc::ATR_HITPOINTSMAX));
    drawBar(p,barHp, w()/2,10, hp, AlignHCenter|AlignTop);
    }

  const int foc = gothic.settingsGetI("GAME","highlightMeleeFocus");
  if(focus.npc!=nullptr  &&
     (foc==1 || foc==3) &&
     player.isPressed(KeyCodec::ActionGeneric) &&
     pl->weaponState()!=WeaponState::NoWeapon &&
     pl->weaponState()!=WeaponState::Fist) {
    auto b    = focus.npc->bounds();
    Vec3 bx[] = {
      {b.bboxTr[0].x,b.bboxTr[0].y,b.bboxTr[0].z},
      {b.bboxTr[1].x,b.bboxTr[0].y,b.bboxTr[0].z},
      {b.bboxTr[1].x,b.bboxTr[1].y,b.bboxTr[0].z},
      {b.bboxTr[0].x,b.bboxTr[1].y,b.bboxTr[0].z},
      {b.bboxTr[0].x,b.bboxTr[0].y,b.bboxTr[1].z},
      {b.bboxTr[1].x,b.bboxTr[0].y,b.bboxTr[1].z},
      {b.bboxTr[1].x,b.bboxTr[1].y,b.bboxTr[1].z},
      {b.bboxTr[0].x,b.bboxTr[1].y,b.bboxTr[1].z},
      };
    int min[2]={ix,iy-20}, max[2]={ix,iy-20};
    for(int i=0; i<8; ++i) {
      vp.project(bx[i]);
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
  const float destW   = 180.f*float(std::min(w(),800))/800.f;
  const float k       = float(destW)/float(std::max(barBack->w(),1));
  const float destH   = float(barBack->h())*k;
  const float destHin = float(destH)*0.95f;

  v = std::max(0.f,std::min(v,1.f));
  if(flg & AlignRight)
    x-=int(destW);
  else if(flg & AlignHCenter)
    x-=int(destW)/2;
  if(flg & AlignBottom)
    y-=int(destH);

  p.setBrush(*barBack);
  p.drawRect(x,y,int(destW),int(destH), 0,0,barBack->w(),barBack->h());

  int   dy = int(0.5f*k*(float(barBack->h())-destHin));
  float pd = 9.f*k;
  p.setBrush(*bar);
  p.drawRect(x+int(pd),y+dy,int(float(destW-pd*2)*v),int(k*destHin),
             0,0,bar->w(),bar->h());
  }

void MainWindow::drawProgress(Painter &p, int x, int y, int w, int h, float v) {
  if(v<0.1f)
    v=0.1f;
  p.setBrush(*loadBox);
  p.drawRect(x,y,w,h, 0,0,loadBox->w(),loadBox->h());

  const int paddL = int((float(w)*75.f)/float(loadBox->w()));
  const int paddT = int((float(h)*10.f)/float(loadBox->h()));

  p.setBrush(*loadVal);
  p.drawRect(x+paddL,y+paddT,int(float(w-2*paddL)*v),h-2*paddT,
             0,0,loadVal->w(),loadVal->h());
  }

void MainWindow::drawLoading(Painter &p, int x, int y, int w, int h) {
  float v = float(gothic.loadingProgress())/100.f;
  drawProgress(p,x,y,w,h,v);
  }

void MainWindow::drawSaving(Painter &p) {
  if(auto back = gothic.loadingBanner()) {
    p.setBrush(Brush(*back,Painter::NoBlend));
    p.drawRect(0,0,this->w(),this->h(),
               0,0,back->w(),back->h());
    }

  float k = 1.0f;//std::max(1.f,float(w())/800.f);
  drawSaving(p,int(460.f*k),int(300.f*k),k);
  }

void MainWindow::drawSaving(Painter& p, int sw, int sh, float scale) {
  const int x = (w()-sw)/2, y = (h()-sh)/2;

  if(saveback==nullptr)
    saveback = Resources::loadTexture("SAVING.TGA");
  if(saveback==nullptr)
    return;

  // SAVING.TGA is semi-transparent image with the idea to accomulate alpha over time
  // ... for loop for now
  p.setBrush(*saveback);
  for(int i=0;i<10;++i) {
    p.drawRect(x,y,sw,sh,
               0,0,saveback->w(),saveback->h());
    }

  float v = float(gothic.loadingProgress())/100.f;
  drawProgress(p, x+int(100.f*scale), y+sh-int(75.f*scale), sw-2*int(100.f*scale), int(40.f*scale), v);
  }

void MainWindow::isDialogClosed(bool& ret) {
  ret = !(dialogs.isActive() || document.isActive());
  }

uint64_t MainWindow::tick() {
  auto time = Application::tickCount();
  auto dt   = time-lastTick;
  lastTick  = time;

  auto st = gothic.checkLoading();
  if(st==Gothic::LoadState::Finalize || st==Gothic::LoadState::FailedLoad || st==Gothic::LoadState::FailedSave) {
    gothic.finishLoading();
    if(st==Gothic::LoadState::FailedLoad)
      rootMenu.setMenu("MENU_MAIN");
    if(st==Gothic::LoadState::FailedSave)
      gothic.onPrint("unable to write savegame file");
    return 0;
    }
  else if(st!=Gothic::LoadState::Idle) {
    if(st==Gothic::LoadState::Loading)
      GameMusic::inst().setMusic(GameMusic::SysLoading); else
      GameMusic::inst().setMusic(GameMusic::SysMenu);
    return 0;
    }

  if(gothic.isPause() || dt==0)
    return 0;

  if(dt>50)
    dt=50;
  dialogs.tick(dt);
  inventory.tick(dt);
  gothic.tick(dt);

  player.tickFocus();

  if(dialogs.isActive())
    ;//clearInput();
  if(document.isActive())
    clearInput();
  tickMouse();
  player.tickMove(dt);
  return dt;
  }

void MainWindow::tickCamera(uint64_t dt) {
  auto pcamera = gothic.camera();
  auto pl      = gothic.player();
  if(pcamera==nullptr || pl==nullptr)
    return;

  auto&      camera       = *pcamera;
  const auto ws           = player.weaponState();
  const bool followCamera = player.isInMove();
  const bool meleeFocus   = (ws==WeaponState::Fist ||
                             ws==WeaponState::W1H  ||
                             ws==WeaponState::W2H);
  auto       pos = pl->cameraBone();

  if(gothic.isPause()) {
    renderer.setCameraView(camera);
    }

  const bool fs = SystemApi::isFullscreen(hwnd());
  if(!fs && mouseP[Event::ButtonLeft]) {
    camera.setSpin(camera.destSpin());
    camera.setDestPosition(pos.x,pos.y,pos.z);
    }
  else if(dialogs.isActive() && !dialogs.isMobsiDialog()) {
    dialogs.dialogCamera(camera);
    }
  else if(inventory.isActive()) {
    camera.setDestPosition(pos.x,pos.y,pos.z);
    }
  else if(player.focus().npc!=nullptr && meleeFocus) {
    auto spin = camera.destSpin();
    spin.y = pl->rotation();
    camera.setDestSpin(spin);
    camera.setDestPosition(pos.x,pos.y,pos.z);
    }
  else {
    auto spin = camera.destSpin();
    spin.y = pl->rotation();
    if(pl->isDive())
      spin.x = -pl->rotationY();
    camera.setDestSpin(spin);
    camera.setDestPosition(pos.x,pos.y,pos.z);
    }

  if(dt==0)
    return;
  if(camera.isToogleEnabled())
    camera.setMode(solveCameraMode());
  camera.tick(*pl, dt, followCamera, (!mouseP[Event::ButtonLeft] || player.hasActionFocus() || fs));
  renderer.setCameraView(camera);
  }

Camera::Mode MainWindow::solveCameraMode() const {
  if(inventory.isOpen()==InventoryMenu::State::Equip ||
     inventory.isOpen()==InventoryMenu::State::Ransack)
    return Camera::Inventory;

  if(auto pl=gothic.player()) {
    if(pl->interactive()!=nullptr)
      return Camera::Mobsi;
    }

  if(dialogs.isActive())
    return Camera::Dialog;

  if(auto pl=gothic.player()) {
    if(pl->isDead())
      return Camera::Death;
    if(pl->isDive())
      return Camera::Dive;
    if(pl->isSwim())
      return Camera::Swim;
    switch(pl->weaponState()){
      case WeaponState::Fist:
      case WeaponState::W1H:
      case WeaponState::W2H:
        return Camera::Melee;
      case WeaponState::Bow:
      case WeaponState::CBow:
        return Camera::Ranged;
      case WeaponState::Mage:
        return Camera::Ranged;
      case WeaponState::NoWeapon:
        return Camera::Normal;
      }
    }

  return Camera::Normal;
  }

void MainWindow::startGame(const std::string &name) {
  // gothic.emitGlobalSound(gothic.loadSoundFx("NEWGAME"));

  if(gothic.checkLoading()==Gothic::LoadState::Idle){
    setGameImpl(nullptr);
    onWorldLoaded();
    }

  gothic.startLoad("LOADING.TGA",[this,name](std::unique_ptr<GameSession>&& game){
    game = nullptr; // clear world-memory now
    std::unique_ptr<GameSession> w(new GameSession(gothic,renderer.storage(),name));
    return w;
    });
  update();
  }

void MainWindow::loadGame(const std::string &name) {
  if(gothic.checkLoading()==Gothic::LoadState::Idle){
    setGameImpl(nullptr);
    onWorldLoaded();
    }

  gothic.startLoad("LOADING.TGA",[this,name](std::unique_ptr<GameSession>&& game){
    game = nullptr; // clear world-memory now
    Tempest::RFile file(name);
    Serialize      s(file);
    std::unique_ptr<GameSession> w(new GameSession(gothic,renderer.storage(),s));
    return w;
    });

  update();
  }

void MainWindow::saveGame(const std::string &name) {
  auto tex = renderer.screenshoot(swapchain.frameId());
  auto pm  = device.readPixels(textureCast(tex));

  gothic.startSave(std::move(textureCast(tex)),[name,pm](std::unique_ptr<GameSession>&& game){
    if(!game)
      return std::move(game);

    Tempest::WFile f(name);
    Serialize      s(f);
    game->save(s,name.c_str(),pm);

    // no print yet, because threading
    // gothic.print("Game saved");
    return std::move(game);
    });

  update();
  }

void MainWindow::onVideo(const Daedalus::ZString& fname) {
  video.pushVideo(fname);
  }

void MainWindow::onStartLoading() {
  player.clearInput();
  inventory.onWorldChanged();
  dialogs.onWorldChanged();
  }

void MainWindow::onWorldLoaded() {
  player   .clearInput();
  inventory.onWorldChanged();
  dialogs  .onWorldChanged();

  dMouse = Point();
  renderer.onWorldChanged();

  device.waitIdle();
  for(auto& c:commandDynamic)
    c = device.commandBuffer();

  if(auto c = gothic.camera())
    c->setViewport(w(),h());
  if(auto pl = gothic.player())
    pl->multSpeed(1.f);
  lastTick = Application::tickCount();
  player.clearFocus();
  }

void MainWindow::onSessionExit() {
  rootMenu.setMenu("MENU_MAIN");
  }

void MainWindow::setGameImpl(std::unique_ptr<GameSession> &&w) {
  gothic.setGame(std::move(w));
  }

void MainWindow::clearInput() {
  player.clearInput();
  std::memset(mouseP,0,sizeof(mouseP));
  }

void MainWindow::setFullscreen(bool fs) {
  dMouse = Point();
  SystemApi::setAsFullscreen(hwnd(),fs);
  SystemApi::setCursorPosition(hwnd(),w()/2,h()/2);
  SystemApi::showCursor(!fs);
  }

void MainWindow::render(){
  try {
    static uint64_t time=Application::tickCount();

    static bool once=true;
    if(once) {
      gothic.emitGlobalSoundWav("GAMESTART.WAV");
      once=false;
      }

    video.tick();
    uint64_t dt = 0;
    if(!video.isActive())
      dt = tick();

    auto& context = fLocal[swapchain.frameId()];
    if(!context.gpuLock.wait(0)) {
      tickCamera(dt);
      return;
      }

    if(!video.isActive()) {
      gothic.updateAnimation();
      tickCamera(dt);
      }

    if(video.isActive()) {
      video.paint(device,swapchain.frameId());
      uiLayer.clear();
      PaintEvent p(uiLayer,atlas,this->w(),this->h());
      video.paintEvent(p);
      }
    else if(needToUpdate() || gothic.checkLoading()!=Gothic::LoadState::Idle) {
      dispatchPaintEvent(uiLayer,atlas);

      numOverlay.clear();
      PaintEvent p(numOverlay,atlas,this->w(),this->h());
      inventory.paintNumOverlay(p);
      }

    const uint32_t imgId = swapchain.nextImage(context.imageAvailable);

    CommandBuffer& cmd = commandDynamic[swapchain.frameId()];
    {
    auto enc = cmd.startEncoding(device);
    renderer.draw(enc,swapchain.frameId(),uint8_t(imgId),uiLayer,numOverlay,inventory,gothic);
    }
    device.submit(cmd,context.imageAvailable,context.renderDone,context.gpuLock);
    device.present(swapchain,imgId,context.renderDone);

    auto t = Application::tickCount();
    if(t-time<15 && !gothic.isInGame() && !video.isActive()){
      Application::sleep(uint32_t(15-(t-time)));
      t = Application::tickCount();
      }
    fps.push(t-time);
    time=t;
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

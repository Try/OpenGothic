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

#include "gothic.h"
#include "game/serialize.h"
#include "utils/crashlog.h"
#include "utils/gthfont.h"

using namespace Tempest;

MainWindow::MainWindow(Gothic &gothic, Device& device)
  : Window(Maximized),device(device),swapchain(device,hwnd()),
    atlas(device),renderer(device,swapchain,gothic),
    gothic(gothic),keycodec(gothic),
    inventory(gothic,renderer.storage()),dialogs(gothic,inventory),document(gothic),chapter(gothic),
    player(gothic,dialogs,inventory) {
  CrashLog::setGpu(device.renderer());
  if(!gothic.isWindowMode())
    setFullscreen(true);

  for(uint8_t i=0;i<device.maxFramesInFlight();++i){
    fLocal.emplace_back(device);
    commandDynamic.emplace_back(device.commandBuffer());
    }

  renderer.resetSwapchain();
  setupUi();

  barBack    = Resources::loadTexture("BAR_BACK.TGA");
  barHp      = Resources::loadTexture("BAR_HEALTH.TGA");
  barMana    = Resources::loadTexture("BAR_MANA.TGA");

  background = Resources::loadTexture("STARTSCREEN.TGA");
  loadBox    = Resources::loadTexture("PROGRESS.TGA");
  loadVal    = Resources::loadTexture("PROGRESS_BAR.TGA");

  timer.timeout.bind(this,&MainWindow::tick);

  gothic.onStartGame    .bind(this,&MainWindow::startGame);
  gothic.onLoadGame     .bind(this,&MainWindow::loadGame);
  gothic.onSaveGame     .bind(this,&MainWindow::saveGame);

  gothic.onWorldLoaded  .bind(this,&MainWindow::onWorldLoaded);
  gothic.onSessionExit  .bind(this,&MainWindow::onSessionExit);

  if(!gothic.defaultSave().empty()){
    gothic.load(gothic.defaultSave());
    rootMenu->popMenu();
    }
  else if(!gothic.doStartMenu()) {
    startGame(gothic.defaultWorld());
    rootMenu->popMenu();
    }

  timer.start(10);
  }

MainWindow::~MainWindow() {
  gothic.stopMusic();
  gothic.cancelLoading();
  device.waitIdle();
  takeWidget(&dialogs);
  takeWidget(&inventory);
  takeWidget(&chapter);
  takeWidget(&document);
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
  rootMenu = &addWidget(new MenuRoot(gothic));
  rootMenu->setMenu("MENU_MAIN");

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
      p.setBrush(*background);
      p.drawRect(0,0,w(),h(),
                 0,0,background->w(),background->h());
      }
    }

  if(st!=Gothic::LoadState::Idle) {
    if(st==Gothic::LoadState::Saving) {
      drawSaving(p);
      } else {
      if(loadBox)
        drawLoading(p,int(w()*0.92)-loadBox->w(), int(h()*0.12), loadBox->w(),loadBox->h());
      }
    } else {
    if(world!=nullptr && world->view()){
      auto& camera = *gothic.gameCamera();
      world->marchPoints(p,world->view()->viewProj(camera.view()),w(),h());

      auto vp = world->view()->viewProj(camera.view());
      p.setBrush(Color(1.0));

      auto focus = world->validateFocus(currentFocus);
      if(focus && !dialogs.isActive()) {
        auto pos = focus.displayPosition();
        vp.project(pos[0],pos[1],pos[2]);

        int   ix = int((0.5f*pos[0]+0.5f)*float(w()));
        int   iy = int((0.5f*pos[1]+0.5f)*float(h()));
        auto& fnt = Resources::font();

        auto tsize = fnt.textSize(focus.displayName());
        ix-=tsize.w/2;
        if(iy<tsize.h)
          iy = tsize.h;
        if(iy>h())
          iy = h();
        fnt.drawText(p, ix,iy,focus.displayName());

        if(auto pl = focus.npc){
          float hp = float(pl->attribute(Npc::ATR_HITPOINTS))/float(pl->attribute(Npc::ATR_HITPOINTSMAX));
          drawBar(p,barHp, w()/2,10, hp, AlignHCenter|AlignTop);
          }

        /*
        if(auto pl = focus.interactive){
          pl->marchInteractives(p,vp,w(),h());
          }*/
        }

      if(auto pl=gothic.player()){
        float hp = float(pl->attribute(Npc::ATR_HITPOINTS))/float(pl->attribute(Npc::ATR_HITPOINTSMAX));
        float mp = float(pl->attribute(Npc::ATR_MANA))     /float(pl->attribute(Npc::ATR_MANAMAX));
        drawBar(p,barHp,  10,    h()-10, hp, AlignLeft |AlignBottom);
        drawBar(p,barMana,w()-10,h()-10, mp, AlignRight|AlignBottom);
        }
      }

    if(world && world->player()) {
      if(world->player()->hasCollision())
        info="[c]";
      else
        info = world->roomAt(world->player()->position()).c_str();
      }
    }

  char fpsT[64]={};
  std::snprintf(fpsT,sizeof(fpsT),"fps = %.2f %s",fps.get(),info);

  auto& fnt = Resources::font();
  fnt.drawText(p,5,30,fpsT);
  }

void MainWindow::resizeEvent(SizeEvent&) {
  swapchain.reset();
  renderer.resetSwapchain();
  }

void MainWindow::mouseDownEvent(MouseEvent &event) {
  if(event.button!=Event::ButtonLeft) {
    event.accept();
    return;
    }
  if(event.button<sizeof(mouseP))
    mouseP[event.button]=true;
  mpos = event.pos();
  if(auto camera = gothic.gameCamera())
    spin = camera->getSpin();
  player.onKeyPressed(keycodec.tr(event));
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
  if(currentFocus.npc && !fs)
    return;
  processMouse(event,fs);
  }

void MainWindow::mouseMoveEvent(MouseEvent &event) {
  const bool fs = SystemApi::isFullscreen(hwnd());
  if(fs) {
    if(auto camera = gothic.gameCamera())
      spin = camera->getSpin();
    processMouse(event,true);
    mpos = Point(w()/2,h()/2);
    SystemApi::setCursorPosition(mpos.x,mpos.y);
    }
  }

void MainWindow::processMouse(MouseEvent &event,bool fs) {
  if(dialogs.isActive() || gothic.isPause())
    return;
  if(auto camera = gothic.gameCamera()) {
    auto dp = (event.pos()-mpos);
    mpos = event.pos();
    spin += PointF(-float(dp.x),float(dp.y));
    if(spin.y>90)
      spin.y=90;
    if(spin.y<-90)
      spin.y=-90;

    if(inventory.isActive()){
      camera->setSpin(spin);
      return;
      }

    if(fs) {
      if(!(currentFocus.npc && player.weaponState()!=WeaponState::NoWeapon))
        player.onRotateMouse(-dp.x);
      spin.x = camera->getSpin().x;
      camera->setSpin(spin);
      } else {
      camera->setSpin(spin);
      }
    }
  }

void MainWindow::mouseWheelEvent(MouseEvent &event) {
  if(auto camera = gothic.gameCamera())
    camera->changeZoom(event.delta);
  }

void MainWindow::keyDownEvent(KeyEvent &event) {
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
  player.onKeyPressed(act);

  if(event.key==Event::K_F9) {
    auto tex = renderer.screenshoot(swapchain.frameId());
    auto pm  = device.readPixels(textureCast(tex));
    pm.save("dbg.png");
    }
  if(event.key==Event::K_F11) {
    gothic.enableMusic(!gothic.isMusicEnabled());
    }
  }

void MainWindow::keyUpEvent(KeyEvent &event) {
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

  if(event.key==KeyEvent::K_F3) {
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
    rootMenu->setMenu(menuEv);
    rootMenu->setFocus(true);
    if(auto pl = gothic.player())
      rootMenu->setPlayer(*pl);
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

  p.setBrush(*loadVal);
  p.drawRect(x+75,y+15,int(float(w-145)*v),35, 0,0,loadVal->w(),loadVal->h());
  }

void MainWindow::drawLoading(Painter &p, int x, int y, int w, int h) {
  if(auto back = gothic.loadingBanner()) {
    p.setBrush(*back);
    p.drawRect(0,0,this->w(),this->h(),
               0,0,back->w(),back->h());
    }

  float v = float(gothic.loadingProgress())/100.f;
  drawProgress(p,x,y,w,h,v);
  }

void MainWindow::drawSaving(Painter &p) {
  if(saveback==nullptr)
    saveback = Resources::loadTexture("SAVING.TGA");
  if(saveback==nullptr)
    return;

  if(auto back = gothic.loadingBanner()) {
    p.setBrush(*back);
    p.drawRect(0,0,this->w(),this->h(),
               0,0,back->w(),back->h());
    }

  float v = float(gothic.loadingProgress())/100.f;
  const int x = (w()-saveback->w())/2, y = (h()-saveback->h())/2;
  p.setBrush(*saveback);

  // SAVING.TGA is semi-transparent image with the idea to accomulate alpha over time
  // ... for loop for now
  for(int i=0;i<10;++i)
    p.drawRect(x,y,saveback->w(),saveback->h());

  drawProgress(p,x+30,y+330,saveback->w()-30*2,50,v);
  }

void MainWindow::tick() {
  auto st = gothic.checkLoading();
  if(st==Gothic::LoadState::Finalize || st==Gothic::LoadState::Failed) {
    gothic.finishLoading();
    if(st==Gothic::LoadState::Failed)
      rootMenu->setMenu("MENU_MAIN");
    }
  else if(st!=Gothic::LoadState::Idle) {
    gothic.setMusic(GameMusic::SysMenu);
    return;
    }

  auto time = Application::tickCount();
  auto dt   = time-lastTick;
  lastTick  = time;

  if(gothic.isPause())
    return;

  if(dt>50)
    dt=50;
  dialogs.tick(dt);
  inventory.tick(dt);
  gothic.tick(dt);

  if(dialogs.isActive()){
    clearInput();
    }

  auto pcamera = gothic.gameCamera();
  if(pcamera==nullptr)
    return;

  player.tickFocus(currentFocus);
  if(document.isActive())
    clearInput();

  const bool followCamera=player.isInMove();
  if(player.tickMove(dt)) {
    auto& camera = *pcamera;
    if(auto pl=gothic.player()) {
      camera.setMode(solveCameraMode());
      camera.follow(*pl,dt,followCamera,
                    (!mouseP[Event::ButtonLeft] || currentFocus || SystemApi::isFullscreen(hwnd())) && !inventory.isActive());
      }
    }
  }

Camera::Mode MainWindow::solveCameraMode() const {
  if(inventory.isOpen()==InventoryMenu::State::Equip ||
     inventory.isOpen()==InventoryMenu::State::Ransack)
    return Camera::Inventory;

  if(auto pl=gothic.player()){
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
  gothic.emitGlobalSound(gothic.loadSoundFx("NEWGAME"));

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

void MainWindow::onWorldLoaded() {
  player   .clearInput();
  inventory.update();
  dialogs  .onWorldChanged();

  if(auto camera = gothic.gameCamera()) {
    //camera->reset();
    spin = camera->getSpin();
    }
  mpos = Point(w()/2,h()/2);
  renderer.onWorldChanged();

  device.waitIdle();
  for(auto& c:commandDynamic)
    c = device.commandBuffer();

  if(auto pl = gothic.player())
    pl->multSpeed(1.f);
  lastTick     = Application::tickCount();
  currentFocus = Focus();
  }

void MainWindow::onSessionExit() {
  rootMenu->setMenu("MENU_MAIN");
  }

void MainWindow::setGameImpl(std::unique_ptr<GameSession> &&w) {
  gothic.setGame(std::move(w));
  }

void MainWindow::clearInput() {
  player.clearInput();
  std::memset(mouseP,0,sizeof(mouseP));
  }

void MainWindow::setFullscreen(bool fs) {
  SystemApi::setAsFullscreen(hwnd(),fs);
  mpos = Point(w()/2,h()/2);
  SystemApi::setCursorPosition(mpos.x,mpos.y);
  SystemApi::showCursor(!fs);
  }

void MainWindow::render(){
  try {
    static uint64_t time=Application::tickCount();

    if(dialogs.isActive())
      renderer.setCameraView(dialogs.dialogCamera()); else
    if(auto camera = gothic.gameCamera())
      renderer.setCameraView(*camera);

    if(!gothic.isPause())
      gothic.updateAnimation();

    auto& context = fLocal[swapchain.frameId()];

    context.gpuLock.wait();
    if(needToUpdate()) {
      dispatchPaintEvent(uiLayer,atlas);

      numOverlay.clear();
      PaintEvent p(numOverlay,atlas,this->w(),this->h());
      inventory.paintNumOverlay(p);
      }

    const uint32_t imgId = swapchain.nextImage(context.imageAvailable);

    PrimaryCommandBuffer& cmd = commandDynamic[swapchain.frameId()];
    renderer.draw(cmd.startEncoding(device),swapchain.frameId(),imgId,uiLayer,numOverlay,inventory,gothic);
    device.submit(cmd,context.imageAvailable,context.renderDone,context.gpuLock);
    device.present(swapchain,imgId,context.renderDone);

    auto t = Application::tickCount();
    if(t-time<16 && !gothic.isInGame()){
      Application::sleep(uint32_t(16-(t-time)));
      t = Application::tickCount();
      }
    fps.push(t-time);
    time=t;
    }
  catch(const Tempest::DeviceLostException&) {
    Log::e("lost device!");
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

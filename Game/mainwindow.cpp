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
#include "utils/cp1251.h"

#include "gothic.h"
#include "game/serialize.h"

using namespace Tempest;

MainWindow::MainWindow(Gothic &gothic, Tempest::VulkanApi& api)
  : Window(Maximized),device(api,hwnd()),atlas(device),resources(gothic,device),
    draw(device,gothic),gothic(gothic),inventory(gothic,draw.storage()),
    dialogs(gothic,inventory),chapter(gothic),camera(gothic),player(gothic,dialogs,inventory) {
  //SystemApi::setAsFullscreen(hwnd(),true);

  for(uint8_t i=0;i<device.maxFramesInFlight();++i){
    fLocal.emplace_back(device);
    commandBuffersSemaphores.emplace_back(device);
    }

  initSwapchain();
  setupUi();

  barBack    = Resources::loadTexture("BAR_BACK.TGA");
  barHp      = Resources::loadTexture("BAR_HEALTH.TGA");
  barMana    = Resources::loadTexture("BAR_MANA.TGA");

  background = Resources::loadTexture("STARTSCREEN.TGA");
  loadBox    = Resources::loadTexture("PROGRESS.TGA");
  loadVal    = Resources::loadTexture("PROGRESS_BAR.TGA");

  timer.timeout.bind(this,&MainWindow::tick);

  gothic.onStartGame  .bind(this,&MainWindow::startGame);
  gothic.onWorldLoaded.bind(this,&MainWindow::onWorldLoaded);

  if(!gothic.doStartMenu()) {
    startGame(gothic.defaultWorld());
    rootMenu->popMenu();
    }

  timer.start(10);
  }

MainWindow::~MainWindow() {
  gothic.cancelLoading();
  device.waitIdle();
  takeWidget(&dialogs);
  takeWidget(&inventory);
  takeWidget(&chapter);
  removeAllWidgets();
  // unload
  gothic.setGame(std::unique_ptr<GameSession>());
  }

void MainWindow::setupUi() {
  setLayout(new StackLayout());
  addWidget(&dialogs);
  addWidget(&inventory);
  addWidget(&chapter);
  rootMenu = &addWidget(new MenuRoot(gothic));
  rootMenu->setMenu(new GameMenu(*rootMenu,gothic,"MENU_MAIN"));

  gothic.onDialogProcess.bind(&dialogs,&DialogMenu::aiProcessInfos);
  gothic.onDialogOutput .bind(&dialogs,&DialogMenu::aiOutput);
  gothic.onDialogClose  .bind(&dialogs,&DialogMenu::aiClose);
  gothic.isDialogClose  .bind(&dialogs,&DialogMenu::aiIsClose);

  gothic.onDialogForwardOutput.bind(&dialogs,&DialogMenu::aiOutputForward);

  gothic.onPrintScreen  .bind(&dialogs,&DialogMenu::printScreen);
  gothic.onPrint        .bind(&dialogs,&DialogMenu::print);

  gothic.onIntroChapter .bind(&chapter,&ChapterScreen::show);
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

  if(st!=Gothic::LoadState::Idle){
    if(loadBox)
      drawLoading(p,int(w()*0.92)-loadBox->w(), int(h()*0.12), loadBox->w(),loadBox->h());
    } else {
    if(world!=nullptr && world->view()){
      world->marchPoints(p,world->view()->viewProj(camera.view()),w(),h());

      auto vp = world->view()->viewProj(camera.view());
      p.setBrush(Color(1.0));

      auto focus = world->validateFocus(currentFocus);
      if(focus && !dialogs.isActive()) {
        auto pos = focus.displayPosition();
        vp.project(pos[0],pos[1],pos[2]);

        int ix = int((0.5f*pos[0]+0.5f)*w());
        int iy = int((0.5f*pos[1]+0.5f)*h());
        p.setFont(Resources::font());
        const char* txt = cp1251::toUtf8(focus.displayName());
        auto tsize = p.font().textSize(txt);
        ix-=tsize.w/2;
        if(iy<tsize.h)
          iy = tsize.h;
        if(iy>h())
          iy = h();
        p.drawText(ix,iy,txt);

        if(auto pl = focus.npc){
          float hp = pl->attribute(Npc::ATR_HITPOINTS)/float(pl->attribute(Npc::ATR_HITPOINTSMAX));
          drawBar(p,barHp, w()/2,10, hp, AlignHCenter|AlignTop);
          }
        }

      if(auto pl=gothic.player()){
        float hp = pl->attribute(Npc::ATR_HITPOINTS)/float(pl->attribute(Npc::ATR_HITPOINTSMAX));
        float mp = pl->attribute(Npc::ATR_MANA)/float(pl->attribute(Npc::ATR_MANAMAX));
        drawBar(p,barHp,  10,    h()-10, hp, AlignLeft |AlignBottom);
        drawBar(p,barMana,w()-10,h()-10, mp, AlignRight|AlignBottom);
        }
      }

    if(world && world->player() && world->player()->hasCollision())
      info="[c]";
    }

  char fpsT[64]={};
  std::snprintf(fpsT,sizeof(fpsT),"fps = %.2f %s",fps.get(),info);

  p.setFont(Resources::font());
  p.drawText(5,30,fpsT);
  }

void MainWindow::resizeEvent(SizeEvent&) {
  device.reset();
  initSwapchain();
  }

void MainWindow::mouseDownEvent(MouseEvent &event) {
  if(event.button!=Event::ButtonLeft) {
    event.accept();
    return;
    }
  if(event.button<sizeof(mouseP))
    mouseP[event.button]=true;
  mpos = event.pos();
  spin = camera.getSpin();
  }

void MainWindow::mouseUpEvent(MouseEvent &event) {
  if(event.button<sizeof(mouseP))
    mouseP[event.button]=false;
  }

void MainWindow::mouseDragEvent(MouseEvent &event) {
  const bool fs = SystemApi::isFullscreen(hwnd());
  if(!mouseP[Event::ButtonLeft] && !fs)
    return;
  processMouse(event,false);
  }

void MainWindow::mouseMoveEvent(MouseEvent &event) {
  const bool fs = SystemApi::isFullscreen(hwnd());
  if(fs) {
    spin = camera.getSpin();
    processMouse(event,true);
    mpos = event.pos();
    }
  }

void MainWindow::processMouse(MouseEvent &event,bool fs) {
  if(dialogs.isActive() || gothic.isPause())
    return;
  auto dp = (event.pos()-mpos);
  mpos = event.pos();
  spin += PointF(fs ? 0 : -dp.x,dp.y);
  if(spin.y>90)
    spin.y=90;
  if(spin.y<-90)
    spin.y=-90;
  if(currentFocus.npc)
    return;
  camera.setSpin(spin);
  }

void MainWindow::mouseWheelEvent(MouseEvent &event) {
  camera.changeZoom(event.delta);
  }

void MainWindow::keyDownEvent(KeyEvent &event) {
  if(chapter.isActive()){
    chapter.keyDownEvent(event);
    if(event.isAccepted()){
      uiKeyUp=&chapter;
      return;
      }
    }

  if(dialogs.isActive()){
    dialogs.keyDownEvent(event);
    if(event.isAccepted()){
      uiKeyUp=&dialogs;
      return;
      }
    }

  if(inventory.isActive()){
    inventory.keyDownEvent(event);
    if(event.isAccepted()){
      uiKeyUp=&inventory;
      return;
      }
    }
  uiKeyUp=nullptr;
  pressed[event.key]=true;
  }

void MainWindow::keyUpEvent(KeyEvent &event) {
  if(uiKeyUp==&chapter){
    chapter.keyUpEvent(event);
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

  pressed[event.key]=false;

  const char* menuEv=nullptr;

  if(event.key==KeyEvent::K_F3) {
    setFullscreen(!SystemApi::isFullscreen(hwnd()));
    }
  else if(event.key==KeyEvent::K_F5){
    gothic.quickSave();
    }
  else if(event.key==KeyEvent::K_F6){
    loadGame("qsave.sav");
    }

  if(event.key==KeyEvent::K_ESCAPE)
    menuEv="MENU_MAIN";
  else if(event.key==KeyEvent::K_Back)
    menuEv="MENU_LOG";
  else if(event.key==KeyEvent::K_B)
    menuEv="MENU_STATUS";

  if(menuEv!=nullptr) {
    rootMenu->setMenu(new GameMenu(*rootMenu,gothic,menuEv));
    rootMenu->setFocus(true);
    if(auto pl = gothic.player())
      rootMenu->setPlayer(*pl);
    clearInput();
    }  
  else if(event.key==KeyEvent::K_Tab){
    if(inventory.isActive()) {
      inventory.close();
      } else {
      auto pl = gothic.player();
      if(pl!=nullptr)
        inventory.open(*pl);
      }
    clearInput();
    }
  }

void MainWindow::drawBar(Painter &p, const Tempest::Texture2d* bar, int x, int y, float v, AlignFlag flg) {
  if(barBack==nullptr || bar==nullptr)
    return;
  const int   destW   = int(180*(std::min(w(),800)/800.f));
  const float k       = destW/float(std::max(barBack->w(),1));
  const int   destH   = int(barBack->h()*k);
  const int   destHin = int(destH*0.95f);

  v = std::max(0.f,std::min(v,1.f));
  if(flg & AlignRight)
    x-=destW;
  else if(flg & AlignHCenter)
    x-=destW/2;
  if(flg & AlignBottom)
    y-=destH;

  p.setBrush(*barBack);
  p.drawRect(x,y,destW,destH, 0,0,barBack->w(),barBack->h());

  int dy = int(0.5f*k*(barBack->h()-destHin));
  int pd = int(9*k);
  p.setBrush(*bar);
  p.drawRect(x+pd,y+dy,int((destW-pd*2)*v),int(k*destHin),
             0,0,bar->w(),bar->h());
  }

void MainWindow::drawLoading(Painter &p, int x, int y, int w, int h) {
  float v = gothic.loadingProgress()/100.f;
  if(v<0.1f)
    v=0.1f;

  if(auto back = gothic.loadingBanner()) {
    p.setBrush(*back);
    p.drawRect(0,0,this->w(),this->h(),
               0,0,back->w(),back->h());
    }

  p.setBrush(*loadBox);
  p.drawRect(x,y,w,h, 0,0,loadBox->w(),loadBox->h());

  p.setBrush(*loadVal);
  p.drawRect(x+75,y+15,int((w-145)*v),35, 0,0,loadVal->w(),loadVal->h());
  }

void MainWindow::tick() {
  auto st = gothic.checkLoading();
  if(st==Gothic::LoadState::Finalize || st==Gothic::LoadState::Failed) {
    gothic.finishLoading();
    }
  else if(st!=Gothic::LoadState::Idle) {
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

  currentFocus = player.findFocus(&currentFocus,camera,w(),h());
  player.setTarget(currentFocus.npc);

  if(mouseP[Event::ButtonLeft]){
    auto focus = currentFocus;
    if(focus.interactive!=nullptr && player.interact(*focus.interactive)) {
      clearInput();
      }
    else if(focus.npc!=nullptr && player.interact(*focus.npc)) {
      clearInput();
      }
    else if(focus.item!=nullptr && player.interact(*focus.item)) {
      clearInput();
      }

    if(focus.npc)
      player.actionFocus(*focus.npc);
    }
  if(pressed[KeyEvent::K_F8])
    player.marvinF8();
  if(pressed[KeyEvent::K_Shift]){
    player.toogleWalkMode();
    pressed[KeyEvent::K_Shift]=false;
    }

  if(pressed[KeyEvent::K_1]){
    player.drawWeaponMele();
    pressed[KeyEvent::K_1]=false;
    }
  if(pressed[KeyEvent::K_2]){
    player.drawWeaponBow();
    pressed[KeyEvent::K_2]=false;
    }

  if(pressed[KeyEvent::K_3]){
    player.drawWeaponMage(3);
    pressed[KeyEvent::K_3]=false;
    }
  if(pressed[KeyEvent::K_4]){
    player.drawWeaponMage(4);
    pressed[KeyEvent::K_4]=false;
    }
  if(pressed[KeyEvent::K_5]){
    player.drawWeaponMage(5);
    pressed[KeyEvent::K_5]=false;
    }
  if(pressed[KeyEvent::K_6]){
    player.drawWeaponMage(6);
    pressed[KeyEvent::K_6]=false;
    }
  if(pressed[KeyEvent::K_7]){
    player.drawWeaponMage(7);
    pressed[KeyEvent::K_7]=false;
    }
  if(pressed[KeyEvent::K_8]){
    player.drawWeaponMage(8);
    pressed[KeyEvent::K_8]=false;
    }
  if(pressed[KeyEvent::K_9]){
    player.drawWeaponMage(9);
    pressed[KeyEvent::K_9]=false;
    }
  if(pressed[KeyEvent::K_0]){
    player.drawWeaponMage(10);
    pressed[KeyEvent::K_0]=false;
    }

  if(pressed[KeyEvent::K_Space]){
    player.jump();
    pressed[KeyEvent::K_Space]=false;
    }
  if(pressed[KeyEvent::K_Q])
    player.rotateLeft();
  if(pressed[KeyEvent::K_E])
    player.rotateRight();

  if(mouseP[Event::ButtonLeft]) {
    if(pressed[KeyEvent::K_W])
      player.actionForward();
    if(pressed[KeyEvent::K_A])
      player.actionLeft();
    if(pressed[KeyEvent::K_D])
      player.actionRight();
    if(pressed[KeyEvent::K_S])
      player.actionBack();
    } else {
    if(pressed[KeyEvent::K_W])
      player.moveForward();
    if(pressed[KeyEvent::K_A])
      player.moveLeft();
    if(pressed[KeyEvent::K_D])
      player.moveRight();
    if(pressed[KeyEvent::K_S])
      player.moveBack();
    }

  if(player.tickMove(dt)) {
    if(auto pl=gothic.player()) {
      camera.setMode(solveCameraMode());
      camera.follow(*pl,dt,!mouseP[Event::ButtonLeft] || currentFocus);
      }
    } else {
    if(pressed[KeyEvent::K_Q])
      camera.rotateLeft();
    if(pressed[KeyEvent::K_E])
      camera.rotateRight();
    if(pressed[KeyEvent::K_A])
      camera.moveLeft();
    if(pressed[KeyEvent::K_D])
      camera.moveRight();
    if(pressed[KeyEvent::K_W])
      camera.moveForward();
    if(pressed[KeyEvent::K_S])
      camera.moveBack();
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

void MainWindow::loadGame(const std::string &name) {
  if(gothic.checkLoading()==Gothic::LoadState::Idle){
    setGameImpl(nullptr);
    onWorldLoaded();
    }

  //LOADING_OLDWORLD.TGA - for world-change trigger
  gothic.startLoading("LOADING_OLDWORLD.TGA",[this,name](std::unique_ptr<GameSession>&& game){
    game = nullptr; // clear world-memory now
    Tempest::RFile file(name);
    Serialize      s(file);
    std::unique_ptr<GameSession> w(new GameSession(gothic,draw.storage(),s));
    return w;
    });

  update();
  }

void MainWindow::startGame(const std::string &name) {
  gothic.emitGlobalSound(gothic.loadSoundFx("NEWGAME"));

  if(gothic.checkLoading()==Gothic::LoadState::Idle){
    setGameImpl(nullptr);
    onWorldLoaded();
    }

  gothic.startLoading("LOADING.TGA",[this,name](std::unique_ptr<GameSession>&& game){
    game = nullptr; // clear world-memory now
    std::unique_ptr<GameSession> w(new GameSession(gothic,draw.storage(),name));
    return w;
    });
  update();
  }

void MainWindow::onWorldLoaded() {
  camera   .reset();
  player   .clearInput();
  inventory.update();
  dialogs  .clear();

  spin = camera.getSpin();
  draw.onWorldChanged();

  device.waitIdle();
  for(auto& c:commandDynamic)
    c = device.commandBuffer();

  if(auto pl = gothic.player())
    pl->multSpeed(1.f);
  lastTick     = Application::tickCount();
  currentFocus = Focus();
  }

void MainWindow::setGameImpl(std::unique_ptr<GameSession> &&w) {
  gothic.setGame(std::move(w));
  }

void MainWindow::clearInput() {
  player.clearInput();
  std::memset(pressed,0,sizeof(pressed));
  std::memset(mouseP,0,sizeof(mouseP));
  }

void MainWindow::setFullscreen(bool fs) {
  SystemApi::setAsFullscreen(hwnd(),fs);
  }

void MainWindow::initSwapchain(){
  const size_t imgC=device.swapchainImageCount();
  commandDynamic.clear();
  fboUi.clear();

  uiPass=device.pass(FboMode::Preserve|FboMode::PresentOut,FboMode::Discard,TextureFormat::Undefined);

  for(size_t i=0;i<imgC;++i) {
    Tempest::Frame frame=device.frame(i);
    fboUi.emplace_back(device.frameBuffer(frame,uiPass));
    commandDynamic.emplace_back(device.commandBuffer());
    }

  draw.initSwapchain(uint32_t(w()),uint32_t(h()));
  }

void MainWindow::render(){
  try {
    static uint64_t time=Application::tickCount();

    auto&                 context    = fLocal[device.frameId()];
    Semaphore&            renderDone = commandBuffersSemaphores[device.frameId()];
    PrimaryCommandBuffer& cmd        = commandDynamic[device.frameId()];

    if(dialogs.isActive())
      draw.setCameraView(dialogs.dialogCamera()); else
      draw.setCameraView(camera);

    if(!gothic.isPause())
      gothic.updateAnimation(); else
      Tempest::Application::sleep(5);

    context.gpuLock.wait();

    const uint32_t imgId=device.nextImage(context.imageAvailable);

    if(needToUpdate())
      dispatchPaintEvent(surface,atlas);

    if(draw.needToUpdateCmd()){
      device.waitIdle();
      for(size_t i=0;i<fLocal.size();++i){
        fLocal[i].gpuLock.wait();
        commandDynamic[i] = device.commandBuffer();
        }
      //draw.updateCmd();
      }

    cmd.begin();
    draw.draw(cmd,imgId,gothic);
    //draw.draw(cmd,imgId,inventory);

    if(1) {
      cmd.setPass(fboUi[imgId],uiPass);
      surface.draw(device,cmd);
      }
    cmd.end();

    device.draw(cmd,context.imageAvailable,renderDone,context.gpuLock);
    auto tt = Application::tickCount();
    device.present(imgId,renderDone);
    tt = Application::tickCount()-tt;

    auto t=Application::tickCount();
    fps.push(t-time);
    time=t;
    }
  catch(const Tempest::DeviceLostException&) {
    Log::e("lost device!");
    device.reset();
    initSwapchain();
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

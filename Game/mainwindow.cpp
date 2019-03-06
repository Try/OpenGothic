#include "mainwindow.h"

#include <Tempest/Except>
#include <Tempest/Painter>

#include <Tempest/Brush>
#include <Tempest/Pen>
#include <Tempest/Layout>
#include <Tempest/Application>
#include <Tempest/Log>

#include "world/focus.h"
#include "ui/dialogmenu.h"
#include "ui/gamemenu.h"
#include "ui/menuroot.h"
#include "ui/stacklayout.h"

#include "gothic.h"

using namespace Tempest;

MainWindow::MainWindow(Gothic &gothic, Tempest::VulkanApi& api)
  : Window(Maximized),device(api,hwnd()),atlas(device),resources(gothic,device),
    draw(device,gothic),gothic(gothic),dialogs(gothic),player(dialogs,inventory) {
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

  timer.timeout.bind(this,&MainWindow::tick);

  gothic.onSetWorld.bind(this,&MainWindow::setWorld);

  if(!gothic.doStartMenu()) {
    setWorld(gothic.defaultWorld());
    rootMenu->popMenu();
    }

  timer.start(10);
  }

MainWindow::~MainWindow() {
  gothic.cancelLoading();
  device.waitIdle();
  takeWidget(&dialogs);
  takeWidget(&inventory);
  removeAllWidgets();
  // unload
  gothic.setWorld(std::unique_ptr<World>());
  }

void MainWindow::setupUi() {
  setLayout(new StackLayout());
  addWidget(&dialogs);
  addWidget(&inventory);
  rootMenu = &addWidget(new MenuRoot(gothic));
  rootMenu->setMenu(new GameMenu(*rootMenu,gothic,"MENU_MAIN"));

  gothic.onDialogProcess.bind(&dialogs,&DialogMenu::aiProcessInfos);
  gothic.onDialogOutput .bind(&dialogs,&DialogMenu::aiOutput);
  gothic.onDialogClose  .bind(&dialogs,&DialogMenu::aiClose);
  gothic.isDialogClose  .bind(&dialogs,&DialogMenu::aiIsClose);

  gothic.onPrintScreen  .bind(&dialogs,&DialogMenu::printScreen);
  gothic.onPrint        .bind(&dialogs,&DialogMenu::print);
  }

Focus MainWindow::findFocus() {
  if(auto gw = gothic.world())
    return gw->findFocus(camera.view(),w(),h());
  return Focus();
  }

void MainWindow::paintEvent(PaintEvent& event) {
  Painter p(event);
  auto world = gothic.world();

  if(world==nullptr && background!=nullptr) {
    p.setBrush(Color(0.0));
    p.drawRect(0,0,w(),h());

    p.setBrush(*background);
    p.drawRect(0,0,w(),h(),
               0,0,background->w(),background->h());
    }

  if(world!=nullptr && world->view()){
    auto vp = world->view()->viewProj(camera.view());
    p.setBrush(Color(1.0));

    auto focus = findFocus();

    if(focus) {
      auto pos = focus.displayPosition();
      vp.project(pos[0],pos[1],pos[2]);

      int ix = int((0.5f*pos[0]+0.5f)*w());
      int iy = int((0.5f*pos[1]+0.5f)*h());
      p.setFont(Resources::font());
      const char* txt = focus.displayName();
      auto tsize = p.font().textSize(txt);
      ix-=tsize.w/2;
      if(iy<tsize.h)
        iy = tsize.h;
      if(iy>h())
        iy = h();
      p.drawText(ix,iy,txt);

      if(auto pl = focus.npc){
        float hp = pl->attribute(Npc::ATR_HITPOINTS)/float(pl->attribute(Npc::ATR_HITPOINTSMAX));
        drawBar(p,barHp, w()/2,10, hp, AlignHCenter|AlignTop );
        }
      }

    if(auto pl=gothic.player()){
      float hp = pl->attribute(Npc::ATR_HITPOINTS)/float(pl->attribute(Npc::ATR_HITPOINTSMAX));
      float mp = pl->attribute(Npc::ATR_MANA)/float(pl->attribute(Npc::ATR_MANAMAX));
      drawBar(p,barHp,  10,    h()-10, hp, AlignLeft |AlignBottom);
      drawBar(p,barMana,w()-10,h()-10, mp, AlignRight|AlignBottom);
      }
    }

  if(gothic.checkLoading()!=Gothic::LoadState::Idle && loadBox){
    p.setBrush(*loadBox);
    p.drawRect(w()-loadBox->w()-50, 50, loadBox->w(),loadBox->h());
    }

  char fpsT[64]={};
  const char* info="";
  if(world && world->player() && world->player()->hasCollision())
    info="[c]";
  std::snprintf(fpsT,sizeof(fpsT),"fps = %.2f %s",fps.get(),info);

  p.setFont(Resources::menuFont());
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
  if(!mouseP[Event::ButtonLeft] || dialogs.isActive())
    return;
  auto dp = (event.pos()-mpos);
  mpos = event.pos();
  spin += PointF(-dp.x,dp.y);
  if(spin.y>90)
    spin.y=90;
  if(spin.y<-90)
    spin.y=-90;
  camera.setSpin(spin);
  }

void MainWindow::mouseWheelEvent(MouseEvent &event) {
  camera.changeZoom(event.delta);
  }

void MainWindow::keyDownEvent(KeyEvent &event) {
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

void MainWindow::tick() {
  auto st = gothic.checkLoading();
  if(st==Gothic::LoadState::Finalize){
    setWorldImpl(std::move(loaderWorld));
    gothic.finishLoading();
    }
  if(st!=Gothic::LoadState::Idle) {
    return;
    }

  auto time = Application::tickCount();
  auto dt   = time-lastTick;
  lastTick  = time;

  if(gothic.isPause())
    return;

  if(dt>100)
    dt=100;
  gothic.tick(dt);
  dialogs.tick(dt);

  if(dialogs.isActive()){
    clearInput();
    inventory.close();
    }

  if(mouseP[Event::ButtonLeft]){
    auto item = findFocus();
    if(item.interactive!=nullptr && player.interact(*item.interactive)) {
      clearInput();
      }
    else if(item.npc!=nullptr && player.interact(*item.npc)) {
      clearInput();
      }
    else if(item.item!=nullptr && player.interact(*item.item)) {
      //clearInput();
      }
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
    if(auto pl=gothic.player())
      camera.follow(*pl,!mouseP[Event::ButtonLeft]);
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

void MainWindow::setWorld(const std::string &name) {
  if(gothic.checkLoading()==Gothic::LoadState::Idle){
    loaderWorld = gothic.clearWorld(); // clear world-memory later
    setWorldImpl(nullptr);
    }

  gothic.startLoading([this,name](){
    loaderWorld=nullptr; // clear world-memory now
    std::this_thread::yield();
    std::unique_ptr<World> w(new World(gothic,draw.storage(),name));
    loaderWorld = std::move(w);
    });
  update();
  }

void MainWindow::setWorldImpl(std::unique_ptr<World> &&w) {
  gothic   .setWorld(std::move(w));
  camera   .setWorld(gothic.world());
  player   .setWorld(gothic.world());
  inventory.setWorld(gothic.world());
  dialogs.clear();
  spin = camera.getSpin();
  draw.onWorldChanged();

  if(auto pl = gothic.player())
    pl->multSpeed(1.f);
  lastTick = Application::tickCount();
  }

void MainWindow::clearInput() {
  player.clearInput();
  std::memset(pressed,0,sizeof(pressed));
  std::memset(mouseP,0,sizeof(mouseP));
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

    auto& context=fLocal[device.frameId()];
    Semaphore&     renderDone=commandBuffersSemaphores[device.frameId()];
    CommandBuffer& cmd       =commandDynamic[device.frameId()];

    draw.setDebugView(camera);
    if(!gothic.isPause())
      gothic.updateAnimation();

    context.gpuLock.wait();

    const uint32_t imgId=device.nextImage(context.imageAvailable);

    if(needToUpdate())
      dispatchPaintEvent(surface,atlas);

    cmd.begin();

    if(1)
      draw.draw(cmd,imgId,gothic);

    if(1) {
      cmd.setPass(fboUi[imgId],uiPass);
      surface.draw(device,cmd,uiPass);
      }

    cmd.end();

    device.draw(cmd,context.imageAvailable,renderDone,context.gpuLock);

    device.present(imgId,renderDone);

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

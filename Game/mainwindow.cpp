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
    draw(device,gothic),gothic(gothic),dialogs(gothic),player(dialogs) {
  for(uint8_t i=0;i<device.maxFramesInFlight();++i){
    fLocal.emplace_back(device);
    commandBuffersSemaphores.emplace_back(device);
    }

  initSwapchain();
  setupUi();

  background = Resources::loadTexture("STARTSCREEN.TGA");
  timer.timeout.bind(this,&MainWindow::tick);

  gothic.onSetWorld.bind(this,&MainWindow::setWorld);

  if(!gothic.doStartMenu()) {
    setWorld(gothic.defaultWorld());
    rootMenu->popMenu();
    }

  timer.start(10);
  }

MainWindow::~MainWindow() {
  device.waitIdle();
  takeWidget(&dialogs);
  removeAllWidgets();
  // unload
  gothic.setWorld(std::make_unique<World>());
  }

void MainWindow::setupUi() {
  setLayout(new StackLayout());
  addWidget(&dialogs);
  rootMenu = &addWidget(new MenuRoot(gothic));
  rootMenu->setMenu(new GameMenu(*rootMenu,gothic,"MENU_MAIN"));

  gothic.onDialogOutput.bind(&dialogs,&DialogMenu::aiOutput);
  gothic.onDialogClose .bind(&dialogs,&DialogMenu::aiClose);
  gothic.onPrintScreen .bind(&dialogs,&DialogMenu::printScreen);
  }

Focus MainWindow::findFocus() {
  if(gothic.world().view())
    return gothic.world().findFocus(camera.view(),w(),h());
  return Focus();
  }

void MainWindow::paintEvent(PaintEvent& event) {
  Painter p(event);

  if(gothic.world().isEmpty() && background!=nullptr) {
    p.setBrush(Color(0.0));
    p.drawRect(0,0,w(),h());

    p.setBrush(*background);
    p.drawRect((w()-background->w())/2,(h()-background->h())/2,
               background->w(),background->h(),
               0,0,background->w(),background->h());
    }

  if(gothic.world().view()){
    auto vp = gothic.world().view()->viewProj(camera.view());
    //gothic.world().marchInteractives(p,vp,w(),h());
    p.setBrush(Color(1.0));

    auto item = findFocus();

    if(item) {
      auto pos = item.displayPosition();
      vp.project(pos[0],pos[1],pos[2]);

      int ix = int((0.5f*pos[0]+0.5f)*w());
      int iy = int((0.5f*pos[1]+0.5f)*h());
      p.setFont(Resources::font());
      const char* txt = item.displayName();
      auto tsize = p.font().textSize(txt);
      ix-=tsize.w/2;
      if(iy<tsize.h)
        iy = tsize.h;
      if(iy>h())
        iy = h();
      p.drawText(ix,iy,txt);
      }
    }

  char fpsT[64]={};
  const char* info="";
  if(gothic.world().player() && gothic.world().player()->hasCollision())
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
  auto dp = (event.pos()-mpos);
  mpos = event.pos();
  spin += PointF(-dp.x,dp.y);
  camera.setSpin(spin);
  }

void MainWindow::mouseWheelEvent(MouseEvent &event) {
  camera.changeZoom(event.delta);
  }

void MainWindow::keyDownEvent(KeyEvent &event) {
  pressed[event.key]=true;
  }

void MainWindow::keyUpEvent(KeyEvent &event) {
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
    if(auto pl = gothic.world().player())
      rootMenu->setPlayer(*pl);
    }
  }

void MainWindow::tick() {
  auto time = Application::tickCount();
  auto dt   = time-lastTick;
  lastTick  = time;

  if(gothic.isPause())
    return;

  if(dt>100)
    dt=100;
  gothic.tick(dt);
  dialogs.tick(dt);

  if(mouseP[Event::ButtonLeft]){
    auto item = findFocus();
    if(item.interactive!=nullptr && player.interact(*item.interactive)) {
      mouseP[Event::ButtonLeft]=false;
      }
    else if(item.npc!=nullptr && player.interact(*item.npc)) {
      mouseP[Event::ButtonLeft]=false;
      }
    }
  if(pressed[KeyEvent::K_F8])
    player.marvinF8();

  if(pressed[KeyEvent::K_0]){
    player.drawFist();
    pressed[KeyEvent::K_0]=false;
    }
  if(pressed[KeyEvent::K_1]){
    player.drawWeapon1H();
    pressed[KeyEvent::K_1]=false;
    }
  if(pressed[KeyEvent::K_2]){
    player.drawWeapon2H();
    pressed[KeyEvent::K_2]=false;
    }
  if(pressed[KeyEvent::K_3]){
    player.drawWeaponBow();
    pressed[KeyEvent::K_3]=false;
    }
  if(pressed[KeyEvent::K_4]){
    player.drawWeaponCBow();
    pressed[KeyEvent::K_4]=false;
    }

  if(pressed[KeyEvent::K_Space]){
    player.jump();
    pressed[KeyEvent::K_Space]=false;
    }
  if(pressed[KeyEvent::K_Q])
    player.rotateLeft();
  if(pressed[KeyEvent::K_E])
    player.rotateRight();
  if(pressed[KeyEvent::K_A])
    player.moveLeft();
  if(pressed[KeyEvent::K_D])
    player.moveRight();
  if(pressed[KeyEvent::K_W])
    player.moveForward();
  if(pressed[KeyEvent::K_S])
    player.moveBack();

  if(player.tickMove(dt)) {
    camera.follow(*gothic.world().player(),!mouseP[Event::ButtonLeft]);
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
  std::unique_ptr<World> w(new World(gothic,draw.storage(),name));
  gothic.setWorld(std::move(w));
  camera.setWorld(&gothic.world());
  player.setWorld(&gothic.world());
  spin = camera.getSpin();
  draw.onWorldChanged();

  if(auto pl = gothic.world().player())
    pl->multSpeed(1.f);

  lastTick = Application::tickCount();
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
    uint64_t time=Application::tickCount();

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

    fps.push(Application::tickCount()-time);
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

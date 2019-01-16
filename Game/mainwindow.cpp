#include "mainwindow.h"

#include <Tempest/Except>
#include <Tempest/Painter>

#include <Tempest/Brush>
#include <Tempest/Pen>
#include <Tempest/Layout>
#include <Tempest/Application>

#include "ui/gamemenu.h"
#include "ui/menuroot.h"

#include "gothic.h"

using namespace Tempest;

MainWindow::MainWindow(Gothic &gothic, Tempest::VulkanApi& api)
  :Window(Maximized),device(api,hwnd()),atlas(device),resources(gothic,device),draw(device),gothic(gothic) {
  for(uint8_t i=0;i<device.maxFramesInFlight();++i){
    fLocal.emplace_back(device);
    commandBuffersSemaphores.emplace_back(device);
    }

  spin.y = 240;
  initSwapchain();
  setupUi();

  background = Resources::loadTexture("STARTSCREEN.TGA");
  }

MainWindow::~MainWindow() {
  removeAllWidgets();
  // unload
  gothic.setWorld(World());
  }

void MainWindow::setupUi() {
  setLayout(Horizontal);
  rootMenu = &addWidget(new MenuRoot(gothic));
  rootMenu->setMenu(new GameMenu(*rootMenu,gothic,"MENU_MAIN"));
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

  char fpsT[64]={};
  std::snprintf(fpsT,sizeof(fpsT),"fps = %.2f",fps.get());

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
  mpos = event.pos();
  }

void MainWindow::mouseDragEvent(MouseEvent &event) {
  auto dp = (event.pos()-mpos);
  mpos = event.pos();
  spin += PointF(dp.x,dp.y);
  }

void MainWindow::mouseWheelEvent(MouseEvent &event) {
  if(event.delta>0)
    zoom *= 1.1f;
  if(event.delta<0)
    zoom /= 1.1f;
  }

void MainWindow::initSwapchain(){
  const size_t imgC=device.swapchainImageCount();
  commandDynamic.clear();
  fboUi.clear();

  uiPass=device.pass(FboMode::Preserve,FboMode::Discard,TextureFormat::Undefined);

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

    context.gpuLock.wait();

    const uint32_t imgId=device.nextImage(context.imageAvailable);

    if(needToUpdate())
      dispatchPaintEvent(surface,atlas);
    draw.setDebugView(spin,zoom);

    cmd.begin();

    if(1)
      draw.draw(cmd,imgId,gothic);

    if(1) {
      cmd.beginRenderPass(fboUi[imgId],uiPass);
      surface.draw(device,cmd,uiPass);
      cmd.endRenderPass();
      }

    cmd.end();

    device.draw(cmd,context.imageAvailable,renderDone,context.gpuLock);

    device.present(imgId,renderDone);

    fps.push(Application::tickCount()-time);
    }
  catch(const Tempest::DeviceLostException&) {
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

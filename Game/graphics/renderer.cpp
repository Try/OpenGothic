#include "renderer.h"

#include <Tempest/Color>
#include <Tempest/Fence>
#include <Tempest/Semaphore>

#include "graphics/submesh/staticmesh.h"
#include "ui/inventorymenu.h"
#include "camera.h"
#include "gothic.h"

using namespace Tempest;

Renderer::Renderer(Tempest::Device &device,Tempest::Swapchain& swapchain,Gothic& gothic)
  :device(device),swapchain(swapchain),gothic(gothic),stor(device,gothic) {
  view.identity();

  static const TextureFormat shfrm[] = {
    TextureFormat::RG16,
    TextureFormat::RGB8,
    TextureFormat::RGBA8,
    };
  static const TextureFormat zfrm[] = {
    //TextureFormat::Depth24S8,
    TextureFormat::Depth24x8,
    TextureFormat::Depth16,
    };

  for(auto& i:shfrm) {
    if(device.properties().hasAttachFormat(i) && device.properties().hasSamplerFormat(i)){
      shadowFormat = i;
      break;
      }
    }

  for(auto& i:zfrm) {
    if(device.properties().hasDepthFormat(i)){
      zBufferFormat = i;
      break;
      }
    }

  Log::i("GPU = ",device.renderer());
  Log::i("Depth format = ",int(zBufferFormat)," Shadow format = ",int(shadowFormat));
  uboShadowComp = stor.device.uniforms(stor.uboComposeLayout());
  }

void Renderer::resetSwapchain() {
  const uint32_t w      = swapchain.w();
  const uint32_t h      = swapchain.h();
  const uint32_t imgC   = swapchain.imageCount();
  const uint32_t smSize = 2048;

  zbuffer        = device.attachment(zBufferFormat,w,h);
  zbufferItem    = device.attachment(zBufferFormat,w,h);
  shadowMapFinal = device.attachment(shadowFormat,smSize,smSize);

  Sampler2d smp;
  smp.setClamping(ClampMode::ClampToBorder);
  smp.anisotropic = false;

  shadowPass = device.pass(FboMode(FboMode::PreserveOut,Color(1.0)), FboMode(FboMode::Discard,1.f));
  for(int i=0;i<2;++i){
    shadowMap[i] = device.attachment(shadowFormat, smSize,smSize);
    shadowZ[i]   = device.attachment(zBufferFormat,smSize,smSize);
    fboShadow[i] = device.frameBuffer(shadowMap[i],shadowZ[i]);
    textureCast(shadowMap[i]).setSampler(smp);
    }

  fboUi.clear();
  fbo3d.clear();
  fboItem.clear();
  for(uint32_t i=0;i<imgC;++i) {
    Tempest::Attachment& frame=swapchain.frame(i);
    fbo3d  .emplace_back(device.frameBuffer(frame,zbuffer));
    fboItem.emplace_back(device.frameBuffer(frame,zbufferItem));
    fboUi  .emplace_back(device.frameBuffer(frame));
    }

  composePass = device.pass(FboMode::PreserveOut);
  fboCompose  = device.frameBuffer(shadowMapFinal);
  textureCast(shadowMapFinal).setSampler(smp);

  if(auto wview=gothic.worldView()) {
    wview->initPipeline(w,h);
    wview->resetCmd();
    }

  uboShadowComp.set(0,shadowMap[0]);
  uboShadowComp.set(1,shadowMap[1]);

  mainPass      = device.pass(FboMode(FboMode::PreserveOut,Color(0.0)), FboMode(FboMode::Discard,1.f));
  uiPass        = device.pass(FboMode::Preserve);
  inventoryPass = device.pass(FboMode::Preserve, FboMode(FboMode::Discard,1.f));
  }

void Renderer::onWorldChanged() {
  if(auto wview=gothic.worldView()){
    if(zbuffer.w()>0 && zbuffer.h()>0)
      wview->initPipeline(uint32_t(zbuffer.w()),uint32_t(zbuffer.h()));
    }
  }

void Renderer::setCameraView(const Camera& camera) {
  view = camera.view();
  if(auto wview=gothic.worldView()){
    shadow[0] = camera.viewShadow(wview->mainLight().dir(),0);
    shadow[1] = camera.viewShadow(wview->mainLight().dir(),1);
    }
  }

void Renderer::draw(Encoder<PrimaryCommandBuffer> &&cmd, uint8_t frameId, uint8_t imgId,
                    VectorImage&   uiLayer,   VectorImage& numOverlay,
                    InventoryMenu& inventory, const Gothic& gothic) {
  draw(cmd, fbo3d  [imgId], gothic, frameId);
  draw(cmd, fboUi  [imgId], uiLayer);
  draw(cmd, fboItem[imgId], inventory);
  draw(cmd, fboUi  [imgId], numOverlay);
  }

void Renderer::draw(Encoder<PrimaryCommandBuffer> &cmd, FrameBuffer& fbo, const Gothic &gothic, uint8_t frameId) {
  auto wview = gothic.worldView();
  if(wview==nullptr) {
    cmd.setPass(fbo,mainPass);
    return;
    }

  wview->updateCmd(frameId,*gothic.world(),swapchain.frame(frameId),shadowMapFinal,fbo.layout(),fboShadow->layout());
  wview->updateUbo(frameId,view,shadow,2);

  for(uint8_t i=0;i<2;++i) {
    cmd.setPass(fboShadow[i],shadowPass);
    wview->drawShadow(cmd,swapchain.frameId(),i);
    }

  composeShadow(cmd,fboCompose);

  cmd.setPass(fbo,mainPass);
  wview->drawMain(cmd,swapchain.frameId());
  }

void Renderer::draw(Encoder<PrimaryCommandBuffer> &cmd, FrameBuffer& fbo, InventoryMenu &inventory) {
  if(inventory.isOpen()==InventoryMenu::State::Closed)
    return;
  cmd.setPass(fbo,inventoryPass);
  inventory.draw(cmd,swapchain.frameId());
  }

void Renderer::draw(Encoder<PrimaryCommandBuffer> &cmd, FrameBuffer& fbo, VectorImage& surface) {
  cmd.setPass(fbo,uiPass);
  surface.draw(device,swapchain,cmd);
  }

void Renderer::composeShadow(Encoder<PrimaryCommandBuffer> &cmd, FrameBuffer &fbo) {
  cmd.setPass(fbo,composePass);
  cmd.setUniforms(stor.pComposeShadow,uboShadowComp);
  cmd.draw(Resources::fsqVbo());
  }

Tempest::Attachment Renderer::screenshoot(uint8_t frameId) {
  device.waitIdle();

  uint32_t w    = uint32_t(zbuffer.w());
  uint32_t h    = uint32_t(zbuffer.h());

  auto        zbuf = device.attachment(zBufferFormat,w,h);
  auto        img  = device.attachment(Tempest::TextureFormat::RGBA8,w,h);
  FrameBuffer fbo  = device.frameBuffer(img,zbuf);

  PrimaryCommandBuffer cmd;
  {
  auto enc = cmd.startEncoding(device);

  draw(enc,fbo,gothic,frameId);
  }

  Fence sync = device.fence();

  const Tempest::PrimaryCommandBuffer* submit[1]={&cmd};
  device.submit(submit,1,nullptr,0,nullptr,0,&sync);
  sync.wait();

  if(auto wview = gothic.worldView())
    wview->resetCmd();

  return img;
  }

#include "renderer.h"

#include <Tempest/Color>
#include <Tempest/Fence>
#include <Tempest/Semaphore>

#include "graphics/submesh/staticmesh.h"
#include "ui/inventorymenu.h"
#include "camera.h"
#include "gothic.h"

using namespace Tempest;

Renderer::Renderer(Tempest::Device &device,Gothic& gothic)
  :device(device),gothic(gothic),stor(device) {
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
    if(device.caps().hasAttachFormat(i) && device.caps().hasSamplerFormat(i)){
      shadowFormat = i;
      break;
      }
    }

  for(auto& i:zfrm) {
    if(device.caps().hasDepthFormat(i)){
      zBufferFormat = i;
      break;
      }
    }

  Log::i("GPU = ",device.renderer());
  Log::i("Depth format = ",int(zBufferFormat)," Shadow format = ",int(shadowFormat));
  uboShadowComp = stor.device.uniforms(stor.uboComposeLayout());
  }

void Renderer::initSwapchain(uint32_t w,uint32_t h) {
  const uint32_t smSize = 2048;
  if(zbuffer.w()==int(w) && zbuffer.h()==int(h))
    return;

  const uint32_t imgC=device.swapchainImageCount();

  zbuffer        = device.texture(zBufferFormat,w,h,false);
  mainPass       = device.pass(Attachment(FboMode::PreserveOut,Color(0.0)), Attachment(FboMode::Discard,1.f));
  shadowMapFinal = device.texture(shadowFormat,smSize,smSize,false);

  Sampler2d smp;
  smp.setClamping(ClampMode::ClampToBorder);
  smp.anisotropic = false;

  shadowPass = device.pass(Attachment(FboMode::PreserveOut,Color(1.0)), Attachment(FboMode::Discard,1.f));
  for(int i=0;i<2;++i){
    shadowMap[i] = device.texture(shadowFormat, smSize,smSize,false);
    shadowZ[i]   = device.texture(zBufferFormat,smSize,smSize,false);
    fboShadow[i] = device.frameBuffer(shadowMap[i],shadowZ[i]);
    shadowMap[i].setSampler(smp);
    }

  fboUi.clear();
  fbo3d.clear();
  for(uint32_t i=0;i<imgC;++i) {
    Tempest::Frame frame=device.frame(i);
    fbo3d.emplace_back(device.frameBuffer(frame,zbuffer));
    fboUi.emplace_back(device.frameBuffer(frame));
    }

  composePass = device.pass(FboMode::PreserveOut);
  fboCompose  = device.frameBuffer(shadowMapFinal);
  shadowMapFinal.setSampler(smp);

  if(auto wview=gothic.worldView())
    wview->initPipeline(w,h);

  uboShadowComp.set(0,shadowMap[0]);
  uboShadowComp.set(1,shadowMap[1]);

  uiPass        = device.pass(FboMode::Preserve);
  inventoryPass = device.pass(FboMode::Submit|FboMode::PreserveIn, Attachment(FboMode::Discard,1.f));
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

bool Renderer::needToUpdateCmd() {
  if(auto wview=gothic.worldView()) {
    return wview->needToUpdateCmd();
    }
  return false;
  }

void Renderer::draw(Tempest::Encoder<Tempest::PrimaryCommandBuffer> &&cmd, uint32_t imgId,
                    VectorImage &surface, InventoryMenu &inventory, const Gothic &gothic) {
  FrameBuffer& fbo3d = this->fbo3d[imgId];
  FrameBuffer& fboUi = this->fboUi[imgId];

  draw(cmd,fbo3d,gothic);
  cmd.setPass(fboUi,uiPass);
  surface.draw(device,cmd);
  draw(cmd,fbo3d,inventory);
  }

void Renderer::draw(Tempest::Encoder<Tempest::PrimaryCommandBuffer> &cmd, FrameBuffer& fbo, const Gothic &gothic) {
  auto wview = gothic.worldView();
  if(wview==nullptr) {
    // just clear
    cmd.setPass(fbo,mainPass);
    return;
    }

  wview->updateCmd(*gothic.world(),shadowMapFinal,fbo.layout(),fboShadow->layout());
  wview->updateUbo(view,shadow,2);

  for(uint8_t i=0;i<2;++i) {
    cmd.setLayout(shadowMap[i],TextureLayout::ColorAttach);
    cmd.setLayout(shadowZ[i],  TextureLayout::DepthAttach);
    cmd.setPass(fboShadow[i],shadowPass);
    wview->drawShadow(fboShadow[i],shadowPass,cmd,i);
    }

  for(uint8_t i=0;i<2;++i)
    cmd.setLayout(shadowMap[i],TextureLayout::Sampler);

  composeShadow(cmd,fboCompose);
  cmd.setLayout(zbuffer,TextureLayout::DepthAttach);
  wview->drawMain(fbo,mainPass,cmd);
  }

void Renderer::draw(Tempest::Encoder<Tempest::PrimaryCommandBuffer> &cmd, FrameBuffer& fbo, InventoryMenu &inventory) {
  cmd.setPass(fbo,inventoryPass);
  inventory.draw(cmd,device.frameId());
  }

void Renderer::composeShadow(Tempest::Encoder<PrimaryCommandBuffer> &cmd, FrameBuffer &fbo) {
  cmd.setLayout(shadowMapFinal,TextureLayout::ColorAttach);
  cmd.setPass(fbo,composePass);
  cmd.setUniforms(stor.pComposeShadow,uboShadowComp);
  cmd.draw(Resources::fsqVbo());
  cmd.setLayout(shadowMapFinal,TextureLayout::Sampler);
  }

Tempest::Pixmap Renderer::screenshoot() {
  device.waitIdle();

  uint32_t w    = uint32_t(zbuffer.w());
  uint32_t h    = uint32_t(zbuffer.h());

  auto        zbuf = device.texture(zBufferFormat,w,h,false);
  auto        img  = device.texture(Tempest::TextureFormat::RGBA8,w,h,false);
  FrameBuffer fbo  = device.frameBuffer(img,zbuffer);

  auto cmd = device.commandBuffer();
  {
  auto enc = cmd.startEncoding(device);
  draw(enc,fbo,gothic);
  }

  Fence sync(device);

  const Tempest::PrimaryCommandBuffer* submit[1]={&cmd};
  device.draw(submit,1,nullptr,0,nullptr,0,&sync);
  sync.wait();

  return device.readPixels(img);
  }

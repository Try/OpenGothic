#include "renderer.h"

#include <Tempest/Color>

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

  Log::i("Depth format = ",int(zBufferFormat)," Shadow format = ",int(shadowFormat));
  uboShadowComp = stor.device.uniforms(stor.uboComposeLayout());
  }

void Renderer::initSwapchain(uint32_t w,uint32_t h) {
  const uint32_t smSize = 2048;
  if(zbuffer.w()==int(w) && zbuffer.h()==int(h))
    return;

  const size_t imgC=device.swapchainImageCount();
  fbo3d.clear();

  zbuffer        = device.createTexture(zBufferFormat,w,h,false);
  mainPass       = device.pass(Color(0.0),1.f,zbuffer.format());
  shadowMapFinal = device.createTexture(shadowFormat,smSize,smSize,false);

  Sampler2d smp;
  smp.setClamping(ClampMode::ClampToBorder);
  smp.anisotropic = false;

  shadowPass = device.pass(Color(1.0),1.f,shadowMapFinal.format(),zbuffer.format());
  for(int i=0;i<2;++i){
    shadowMap[i] = device.createTexture(shadowFormat,smSize,smSize,false);
    shadowZ[i]   = device.createTexture(zBufferFormat,smSize,smSize,false);
    fboShadow[i] = device.frameBuffer(shadowMap[i],shadowZ[i],shadowPass);
    shadowMap[i].setSampler(smp);
    }

  for(size_t i=0;i<imgC;++i) {
    Tempest::Frame frame=device.frame(i);
    fbo3d.emplace_back(device.frameBuffer(frame,zbuffer,mainPass));
    }

  composePass    = device.pass(Color(0.0),shadowMapFinal.format());
  fboCompose     = device.frameBuffer(shadowMapFinal,composePass);
  shadowMapFinal.setSampler(smp);

  stor.initPipeline(mainPass);
  if(auto wview=gothic.worldView())
    wview->initPipeline(w,h);

  uboShadowComp.set(0,shadowMap[0]);
  uboShadowComp.set(1,shadowMap[1]);
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

void Renderer::updateCmd() {
  if(auto wview=gothic.worldView()) {
    //wview->updateCmd(*gothic.world(),Resources::fallbackTexture());
    wview->updateCmd(*gothic.world(),shadowMapFinal,shadowPass);
    }
  }

void Renderer::draw(PrimaryCommandBuffer &cmd, uint32_t imgId, const Gothic &gothic) {
  FrameBuffer& fboFr = fbo3d[imgId];

  auto wview = gothic.worldView();
  if(wview!=nullptr) {
    wview->updateCmd(*gothic.world(),shadowMapFinal,shadowPass);
    wview->updateUbo(view,shadow,2,device.frameId());

    cmd.exchangeLayout(shadowMap[0],TextureLayout::Undefined,TextureLayout::ColorAttach);
    cmd.exchangeLayout(shadowMap[1],TextureLayout::Undefined,TextureLayout::ColorAttach);
    wview->drawShadow(cmd,fboShadow[0],shadowPass,imgId,0);
    wview->drawShadow(cmd,fboShadow[1],shadowPass,imgId,1);

    //cmd.barrier      (shadowMap[0],Stage::Fragment,Stage::Fragment);
    //cmd.barrier      (shadowMap[1],Stage::Fragment,Stage::Fragment);

    //composeShadow(cmd,fboFr);
    composeShadow(cmd,fboCompose);
    //cmd.barrier      (shadowMapFinal,Stage::Fragment,Stage::Fragment);
    wview->draw      (cmd,fboFr,storage().pass(),imgId);
    } else {
    cmd.setPass(fboFr,storage().pass());
    }
  }

void Renderer::draw(PrimaryCommandBuffer &cmd, uint32_t imgId, InventoryMenu &inventory) {
  FrameBuffer& fbo = fbo3d[imgId];

  cmd.setPass(fbo,storage().pass());
  inventory.draw(cmd,device.frameId());
  }

void Renderer::composeShadow(PrimaryCommandBuffer &cmd, FrameBuffer &fbo) {
  cmd.exchangeLayout(shadowMap[0],TextureLayout::ColorAttach,TextureLayout::Sampler);
  cmd.exchangeLayout(shadowMap[1],TextureLayout::ColorAttach,TextureLayout::Sampler);

  cmd.setPass(fbo,composePass);
  cmd.setUniforms(stor.pComposeShadow,uboShadowComp); // FIXME: vulkan image-layouts
  cmd.draw(Resources::fsqVbo());
  }

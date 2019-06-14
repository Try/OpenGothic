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

  const TextureFormat zfrm[] = {
    TextureFormat::RG16,
    TextureFormat::RGB8,
    TextureFormat::RGBA8,
    };
  for(auto& i:zfrm) {
    if(device.caps().hasAttachFormat(i) && device.caps().hasSamplerFormat(i)){
      shadowFormat = i;
      break;
      }
    }
  }

void Renderer::initSwapchain(uint32_t w,uint32_t h) {
  const uint32_t smSize = 2048;
  if(zbuffer.w()==int(w) && zbuffer.h()==int(h))
    return;

  const size_t imgC=device.swapchainImageCount();
  fbo3d.clear();

  zbuffer    = device.createTexture(TextureFormat::Depth24x8,w,h,false);
  mainPass   = device.pass(Color(0.0),1.f,zbuffer.format());

  shadowMap  = device.createTexture(shadowFormat,smSize,smSize,false);
  shadowZ    = device.createTexture(TextureFormat::Depth24x8,smSize,smSize,false);
  shadowPass = device.pass(Color(1.0),1.f,shadowMap.format(),zbuffer.format());
  fboShadow  = device.frameBuffer(shadowMap,shadowZ,shadowPass);

  Sampler2d smp;
  smp.setClamping(ClampMode::ClampToBorder);
  smp.anisotropic = false;
  shadowMap.setSampler(smp);

  for(size_t i=0;i<imgC;++i) {
    Tempest::Frame frame=device.frame(i);
    fbo3d.emplace_back(device.frameBuffer(frame,zbuffer,mainPass));
    }

  stor.initPipeline(mainPass);
  if(auto wview=gothic.worldView())
    wview->initPipeline(w,h);
  }

void Renderer::onWorldChanged() {
  if(auto wview=gothic.worldView()){
    if(zbuffer.w()>0 && zbuffer.h()>0)
      wview->initPipeline(uint32_t(zbuffer.w()),uint32_t(zbuffer.h()));
    }
  }

void Renderer::setCameraView(const Camera& camera) {
  view = camera.view();
  if(auto wview=gothic.worldView())
    shadow = camera.viewShadow(wview->mainLight().dir());
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
    wview->updateCmd(*gothic.world(),shadowMap,shadowPass);
    }
  }

void Renderer::draw(PrimaryCommandBuffer &cmd, uint32_t imgId, const Gothic &gothic) {
  FrameBuffer& fboFr = fbo3d[imgId];

  auto wview = gothic.worldView();
  if(wview!=nullptr) {
    wview->updateCmd(*gothic.world(),shadowMap,shadowPass);
    wview->updateUbo(view,shadow,device.frameId());

    wview->drawShadow(cmd,fboShadow,shadowPass,imgId);
    //cmd.changeLayout (shadowMap,TextureLayout::Sampler,TextureLayout::ColorAttach);
    wview->draw      (cmd,fboFr,storage().pass(),imgId);

    //wview->drawShadow(cmd,fboFr,storage().pass(),imgId);
    } else {
    cmd.setPass(fboFr,storage().pass());
    }
  }

void Renderer::draw(PrimaryCommandBuffer &cmd, uint32_t imgId, InventoryMenu &inventory) {
  FrameBuffer& fbo = fbo3d[imgId];

  cmd.setPass(fbo,storage().pass());
  inventory.draw(cmd,device.frameId());
  }

#include "renderer.h"

#include <Tempest/Color>
#include <Tempest/Fence>
#include <Tempest/Semaphore>
#include <Tempest/Log>

#include "graphics/mesh/submesh/staticmesh.h"
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
  uboShadowComp = stor.device.uniforms(stor.pComposeShadow.layout());
  uboCopy       = stor.device.uniforms(stor.pCopy.layout());
  }

void Renderer::resetSwapchain() {
  const uint32_t w      = swapchain.w();
  const uint32_t h      = swapchain.h();
  const uint32_t imgC   = swapchain.imageCount();
  const uint32_t smSize = 2048;

  zbuffer        = device.zbuffer   (zBufferFormat,w,h);
  zbufferItem    = device.zbuffer   (zBufferFormat,w,h);
  shadowMapFinal = device.attachment(shadowFormat,smSize,smSize);
  shadowPass     = device.pass(FboMode(FboMode::PreserveOut,Color(1.0)), FboMode(FboMode::Discard,1.f));

  for(int i=0; i<2; ++i){
    shadowMap[i] = device.attachment (shadowFormat, smSize,smSize);
    shadowZ[i]   = device.zbuffer    (zBufferFormat,smSize,smSize);
    fboShadow[i] = device.frameBuffer(shadowMap[i],shadowZ[i]);
    }

  lightingBuf = device.attachment(TextureFormat::RGBA8,swapchain.w(),swapchain.h());
  gbufDiffuse = device.attachment(TextureFormat::RGBA8,swapchain.w(),swapchain.h());
  gbufNormal  = device.attachment(TextureFormat::RGBA8,swapchain.w(),swapchain.h());
  gbufDepth   = device.attachment(TextureFormat::R32F, swapchain.w(),swapchain.h());

  fboUi.clear();
  fbo3d.clear();
  fboCpy.clear();
  fboItem.clear();
  for(uint32_t i=0;i<imgC;++i) {
    Tempest::Attachment& frame=swapchain.frame(i);
    fbo3d  .emplace_back(device.frameBuffer(frame,zbuffer));
    fboCpy .emplace_back(device.frameBuffer(frame));
    fboItem.emplace_back(device.frameBuffer(frame,zbufferItem));
    fboUi  .emplace_back(device.frameBuffer(frame));
    }
  fboGBuf     = device.frameBuffer(lightingBuf,gbufDiffuse,gbufNormal,gbufDepth,zbuffer);
  copyPass    = device.pass(FboMode::PreserveOut);
  fboCompose  = device.frameBuffer(shadowMapFinal);

  if(auto wview=gothic.worldView()) {
    wview->setFrameGlobals(Resources::fallbackBlack(),0,0);
    wview->setGbuffer(Resources::fallbackBlack(),Resources::fallbackBlack(),Resources::fallbackBlack(),Resources::fallbackBlack());
    }

  Sampler2d smp = Sampler2d::nearest();
  smp.setFiltration(Filter::Linear);
  smp.setClamping(ClampMode::ClampToBorder);
  uboShadowComp.set(0,shadowMap[0],smp);
  smp = Sampler2d::nearest();
  smp.setClamping(ClampMode::ClampToBorder);
  uboShadowComp.set(1,shadowMap[1],smp);

  uboCopy.set(0,lightingBuf,Sampler2d::nearest());

  gbufPass       = device.pass(FboMode(FboMode::PreserveOut,Color(0.0)),
                               FboMode(FboMode::PreserveOut),
                               FboMode(FboMode::PreserveOut),
                               FboMode(FboMode::PreserveOut,Color(1.0)),
                               FboMode(FboMode::PreserveOut,1.f));
  mainPass       = device.pass(FboMode::Preserve, FboMode::PreserveIn);
  mainPassNoGbuf = device.pass(FboMode(FboMode::PreserveOut,Color(0.0)), FboMode(FboMode::Discard,1.f));
  uiPass         = device.pass(FboMode::Preserve);
  inventoryPass  = device.pass(FboMode::Preserve, FboMode(FboMode::Discard,1.f));
  }

void Renderer::onWorldChanged() {
  }

void Renderer::setCameraView(const Camera& camera) {
  view     = camera.view();
  viewProj = camera.viewProj();
  if(auto wview=gothic.worldView()){
    shadow[0] = camera.viewShadow(wview->mainLight().dir(),0);
    shadow[1] = camera.viewShadow(wview->mainLight().dir(),1);
    }
  }

void Renderer::draw(Encoder<CommandBuffer>& cmd, uint8_t frameId, uint8_t imgId,
                    VectorImage&   uiLayer,   VectorImage& numOverlay,
                    InventoryMenu& inventory, const Gothic& gothic) {
  draw(cmd, fbo3d  [imgId], fboCpy[imgId], gothic, frameId);
  draw(cmd, fboUi  [imgId], uiLayer);
  draw(cmd, fboItem[imgId], inventory);
  draw(cmd, fboUi  [imgId], numOverlay);
  }

void Renderer::draw(Tempest::Encoder<CommandBuffer>& cmd,
                    FrameBuffer& fbo, FrameBuffer& fboCpy, const Gothic &gothic, uint8_t frameId) {
  auto wview = gothic.worldView();
  if(wview==nullptr) {
    cmd.setFramebuffer(fbo,mainPassNoGbuf);
    return;
    }

  Painter3d painter(cmd);
  wview->setModelView(viewProj,shadow,2);
  wview->setFrameGlobals(textureCast(shadowMapFinal),gothic.world()->tickCount(),frameId);
  wview->setGbuffer(textureCast(lightingBuf),textureCast(gbufDiffuse),textureCast(gbufNormal),textureCast(gbufDepth));

  for(uint8_t i=2;i>0;) {
    --i;
    cmd.setFramebuffer(fboShadow[i],shadowPass);
    painter.setFrustrum(shadow[i]);
    wview->drawShadow(cmd,painter,frameId,i);
    }

  cmd.setFramebuffer(fboCompose,copyPass);
  cmd.setUniforms(stor.pComposeShadow,uboShadowComp);
  cmd.draw(Resources::fsqVbo());

  painter.setFrustrum(viewProj);
  cmd.setFramebuffer(fboGBuf,gbufPass);
  wview->drawGBuffer(cmd,painter,frameId);

  cmd.setFramebuffer(fboCpy,copyPass);
  cmd.setUniforms(stor.pCopy,uboCopy);
  cmd.draw(Resources::fsqVbo());

  cmd.setFramebuffer(fbo,mainPass);
  wview->drawLights (cmd,painter,frameId);
  wview->drawMain   (cmd,painter,frameId);
  }

void Renderer::draw(Tempest::Encoder<CommandBuffer>& cmd, FrameBuffer& fbo, InventoryMenu &inventory) {
  if(inventory.isOpen()==InventoryMenu::State::Closed)
    return;
  cmd.setFramebuffer(fbo,inventoryPass);
  inventory.draw(fbo,cmd,swapchain.frameId());
  }

void Renderer::draw(Tempest::Encoder<CommandBuffer>& cmd, FrameBuffer& fbo, VectorImage& surface) {
  cmd.setFramebuffer(fbo,uiPass);
  surface.draw(device,swapchain,cmd);
  }

Tempest::Attachment Renderer::screenshoot(uint8_t frameId) {
  device.waitIdle();

  uint32_t w = uint32_t(zbuffer.w());
  uint32_t h = uint32_t(zbuffer.h());

  auto        img  = device.attachment(Tempest::TextureFormat::RGBA8,w,h);
  FrameBuffer fbo  = device.frameBuffer(img,zbuffer);
  FrameBuffer fboC = device.frameBuffer(img);

  if(auto wview = gothic.worldView())
    wview->setupUbo();

  CommandBuffer cmd;
  {
  auto enc = cmd.startEncoding(device);
  draw(enc,fbo,fboC,gothic,frameId);
  }

  Fence sync = device.fence();

  const Tempest::CommandBuffer* submit[1]={&cmd};
  device.submit(submit,1,nullptr,0,nullptr,0,&sync);
  sync.wait();

  if(auto wview = gothic.worldView())
    wview->setupUbo();

  return img;
  }

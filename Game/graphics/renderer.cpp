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

Renderer::Renderer(Tempest::Swapchain& swapchain, Gothic& gothic)
  : swapchain(swapchain),gothic(gothic),stor(gothic) {
  auto& device = Resources::device();
  view.identity();

  static const TextureFormat shfrm[] = {
    TextureFormat::R16,
    TextureFormat::RG16,
    TextureFormat::R32F,
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
  uboCopy = device.uniforms(stor.pCopy.layout());
  }

void Renderer::resetSwapchain() {
  auto& device = Resources::device();

  const uint32_t w      = swapchain.w();
  const uint32_t h      = swapchain.h();
  const uint32_t imgC   = swapchain.imageCount();
  const uint32_t smSize = 2048;

  zbuffer        = device.zbuffer(zBufferFormat,w,h);
  zbufferItem    = device.zbuffer(zBufferFormat,w,h);
  shadowPass     = device.pass(FboMode(FboMode::PreserveOut,Color(0.0)), FboMode(FboMode::Discard,0.f));

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

  if(auto wview=gothic.worldView()) {
    wview->setFrameGlobals(nullptr,0,0);
    wview->setGbuffer(Resources::fallbackBlack(),Resources::fallbackBlack(),Resources::fallbackBlack(),Resources::fallbackBlack());
    }

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
  if(auto wview=gothic.worldView()) {
    for(size_t i=0; i<Resources::ShadowLayers; ++i)
      shadow[i] = camera.viewShadow(wview->mainLight().dir(),i);
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

  wview->setModelView(viewProj,shadow,Resources::ShadowLayers);
  const Texture2d* sh[Resources::ShadowLayers];
  for(size_t i=0; i<Resources::ShadowLayers; ++i)
    sh[i] = &textureCast(shadowMap[i]);
  wview->setFrameGlobals(sh,gothic.world()->tickCount(),frameId);
  wview->setGbuffer(textureCast(lightingBuf),textureCast(gbufDiffuse),textureCast(gbufNormal),textureCast(gbufDepth));

  wview->visibilityPass(viewProj,shadow,Resources::ShadowLayers);
  for(uint8_t i=0; i<Resources::ShadowLayers; ++i) {
    cmd.setFramebuffer(fboShadow[i],shadowPass);
    wview->drawShadow(cmd,frameId,i);
    }

  cmd.setFramebuffer(fboGBuf,gbufPass);
  wview->drawGBuffer(cmd,frameId);

  cmd.setFramebuffer(fboCpy,copyPass);
  cmd.setUniforms(stor.pCopy,uboCopy);
  cmd.draw(Resources::fsqVbo());

  cmd.setFramebuffer(fbo,mainPass);
  wview->drawLights (cmd,frameId);
  wview->drawMain   (cmd,frameId);
  }

void Renderer::draw(Tempest::Encoder<CommandBuffer>& cmd, FrameBuffer& fbo, InventoryMenu &inventory) {
  if(inventory.isOpen()==InventoryMenu::State::Closed)
    return;
  cmd.setFramebuffer(fbo,inventoryPass);
  inventory.draw(fbo,cmd,swapchain.frameId());
  }

void Renderer::draw(Tempest::Encoder<CommandBuffer>& cmd, FrameBuffer& fbo, VectorImage& surface) {
  auto& device = Resources::device();
  cmd.setFramebuffer(fbo,uiPass);
  surface.draw(device,swapchain,cmd);
  }

Tempest::Attachment Renderer::screenshoot(uint8_t frameId) {
  auto& device = Resources::device();
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

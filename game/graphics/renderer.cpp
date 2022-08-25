#include "renderer.h"

#include <Tempest/Color>
#include <Tempest/Fence>
#include <Tempest/Log>

#include "graphics/mesh/submesh/staticmesh.h"
#include "ui/inventorymenu.h"
#include "camera.h"
#include "gothic.h"

using namespace Tempest;

static uint32_t nearestPot(uint32_t x) {
  x = x | (x >> 1);
  x = x | (x >> 2);
  x = x | (x >> 4);
  x = x | (x >> 8);
  x = x | (x >> 16);
  return x - (x >> 1);
  }

Renderer::Renderer(Tempest::Swapchain& swapchain)
  : swapchain(swapchain) {
  auto& device = Resources::device();
  view.identity();
  /*
  static const TextureFormat shfrm[] = {
    TextureFormat::R16,
    TextureFormat::RG16,
    TextureFormat::R32F,
    TextureFormat::RGBA8,
    };*/
  static const TextureFormat zfrm[] = {
    //TextureFormat::Depth24S8,
    TextureFormat::Depth24x8,
    TextureFormat::Depth16,
    };

  for(auto& i:zfrm) {
    if(device.properties().hasDepthFormat(i) && device.properties().hasSamplerFormat(i)){
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

  Log::i("GPU = ",device.properties().name);
  Log::i("Depth format = ",int(zBufferFormat)," Shadow format = ",int(shadowFormat));

  Gothic::inst().onSettingsChanged.bind(this,&Renderer::initSettings);
  initSettings();
  }

Renderer::~Renderer() {
  Gothic::inst().onSettingsChanged.ubind(this,&Renderer::initSettings);
  }

void Renderer::resetSwapchain() {
  auto& device = Resources::device();
  device.waitIdle();

  const uint32_t w      = swapchain.w();
  const uint32_t h      = swapchain.h();
  const uint32_t smSize = settings.shadowResolution;

  auto smpN = Sampler2d::nearest();
  smpN.setClamping(ClampMode::ClampToEdge);

  auto smpB = Sampler2d::bilinear();
  smpB.setClamping(ClampMode::ClampToEdge);

  zbuffer        = device.zbuffer(zBufferFormat,w,h);
  zbufferItem    = device.zbuffer(zBufferFormat,w,h);

  if(Gothic::inst().doMeshShading()) {
    uint32_t hw = nearestPot(w);
    uint32_t hh = nearestPot(h);

    hiZPot  = device.image2d(TextureFormat::R16, hw, hh, true);
    hiZ     = StorageImage();

    uboHiZPot = device.descriptors(Shaders::inst().hiZPot);
    uboHiZPot.set(0, zbuffer, smpN);
    uboHiZPot.set(1, hiZPot);

    uboZMip.clear();
    for(uint32_t i=0; (hw>1 && hh>1); ++i) {
      hw = std::max(1u, hw/2u);
      hh = std::max(1u, hh/2u);
      auto& ubo = uboZMip.emplace_back(device.descriptors(Shaders::inst().hiZMip));

      if(hiZ.isEmpty()) {
        ubo.set(0, hiZPot, smpN, i);
        ubo.set(1, hiZPot, smpN, i+1);
        } else {
        ubo.set(0, hiZ, smpN, i);
        ubo.set(1, hiZ, smpN, i+1);
        }

      if(hiZ.isEmpty() && (hw<=64 && hh<=64)) {
        hiZ = device.image2d(TextureFormat::R16, hw, hh, true);
        ubo.set(1, hiZ, smpN, 0);
        i = uint32_t(-1);
        }
      }
    }

  if(smSize>0) {
    for(int i=0; i<Resources::ShadowLayers; ++i)
      shadowMap[i] = device.zbuffer(shadowFormat,smSize,smSize);
    }

  lightingBuf = device.attachment(TextureFormat::RGBA8,swapchain.w(),swapchain.h());
  gbufDiffuse = device.attachment(TextureFormat::RGBA8,swapchain.w(),swapchain.h());
  gbufNormal  = device.attachment(TextureFormat::RGBA8,swapchain.w(),swapchain.h());
  gbufDepth   = device.attachment(TextureFormat::R32F, swapchain.w(),swapchain.h());

  uboCopy = device.descriptors(Shaders::inst().copy);
  uboCopy.set(0,lightingBuf,Sampler2d::nearest());

  ssao.ssaoBuf = device.attachment(ssao.aoFormat, (swapchain.w()+1)/2,(swapchain.h()+1)/2);
  ssao.blurBuf = device.attachment(ssao.aoFormat, (swapchain.w()+1)/2,(swapchain.h()+1)/2);

  if(Gothic::inst().doRayQuery()) {
    ssao.ssaoPso        = &Shaders::inst().ssaoRq;
    //ssao.ssaoComposePso = &Shaders::inst().ssaoComposeRq;
    ssao.ssaoComposePso = &Shaders::inst().ssaoCompose;
    } else {
    ssao.ssaoPso        = &Shaders::inst().ssao;
    ssao.ssaoComposePso = &Shaders::inst().ssaoCompose;
    }
  ssao.uboSsao = device.descriptors(*ssao.ssaoPso);
  ssao.uboSsao.set(0,lightingBuf,smpN);
  ssao.uboSsao.set(1,gbufDiffuse,smpN);
  ssao.uboSsao.set(2,gbufNormal, smpN);
  ssao.uboSsao.set(3,zbuffer,    smpB);

  ssao.uboCompose = device.descriptors(*ssao.ssaoComposePso);
  ssao.uboCompose.set(0,lightingBuf, smpB);
  ssao.uboCompose.set(1,gbufDiffuse, smpB);
  ssao.uboCompose.set(2,ssao.ssaoBuf,smpB);
  ssao.uboCompose.set(3,zbuffer,     smpB);

  ssao.uboBlur[0] = device.descriptors(Shaders::inst().bilateralBlur);
  ssao.uboBlur[0].set(0,ssao.ssaoBuf,smpN);
  ssao.uboBlur[0].set(1,zbuffer,     smpB);

  ssao.uboBlur[1] = device.descriptors(Shaders::inst().bilateralBlur);
  ssao.uboBlur[1].set(0,ssao.blurBuf,smpN);
  ssao.uboBlur[1].set(1,zbuffer,     smpB);

  prepareUniforms();
  }

void Renderer::initSettings() {
  settings.zEnvMappingEnabled = Gothic::inst().settingsGetI("ENGINE","zEnvMappingEnabled")!=0;
  settings.zCloudShadowScale  = Gothic::inst().settingsGetI("ENGINE","zCloudShadowScale") !=0;
  }

void Renderer::onWorldChanged() {
  prepareUniforms();
  }

void Renderer::setCameraView(const Camera& camera) {
  view     = camera.view();
  proj     = camera.projective();
  viewProj = camera.viewProj();
  if(auto wview=Gothic::inst().worldView()) {
    for(size_t i=0; i<Resources::ShadowLayers; ++i)
      shadow[i] = camera.viewShadow(wview->mainLight().dir(),i);
    }

  zNear      = camera.zNear();
  zFar       = camera.zFar();
  clipInfo.x = zNear*zFar;
  clipInfo.y = zNear-zFar;
  clipInfo.z = zFar;
  }

void Renderer::prepareUniforms() {
  auto wview = Gothic::inst().worldView();
  if(wview==nullptr)
    return;

  if(ssao.ssaoPso==&Shaders::inst().ssaoRq)
    ssao.uboSsao   .set(5,wview->landscapeTlas());
  if(ssao.ssaoComposePso==&Shaders::inst().ssaoComposeRq)
    ssao.uboCompose.set(5,wview->landscapeTlas());


  const Texture2d* sh[Resources::ShadowLayers] = {};
  for(size_t i=0; i<Resources::ShadowLayers; ++i)
    if(!shadowMap[i].isEmpty()) {
      sh[i] = &textureCast(shadowMap[i]);
      }
  wview->setGbuffer(textureCast(lightingBuf),textureCast(gbufDiffuse),
                    textureCast(gbufNormal), textureCast(gbufDepth),
                    sh, textureCast(hiZ));
  }

void Renderer::draw(Encoder<CommandBuffer>& cmd, uint8_t cmdId, size_t imgId,
                    VectorImage::Mesh& uiLayer, VectorImage::Mesh& numOverlay,
                    InventoryMenu& inventory) {
  auto& result = swapchain[imgId];

  draw(result, cmd, cmdId);
  cmd.setFramebuffer({{result, Tempest::Preserve, Tempest::Preserve}});
  uiLayer.draw(cmd);

  if(inventory.isOpen()!=InventoryMenu::State::Closed) {
    cmd.setFramebuffer({{result, Tempest::Preserve, Tempest::Preserve}},{zbufferItem, 1.f, Tempest::Preserve});
    inventory.draw(cmd,cmdId);

    cmd.setFramebuffer({{result, Tempest::Preserve, Tempest::Preserve}});
    numOverlay.draw(cmd);
    }
  }

void Renderer::dbgDraw(Tempest::Painter& p) {
  static bool dbg = false;
  if(!dbg)
    return;

  auto& tex = hiZ;
  //auto& tex = shadowMap[1];

  p.setBrush(textureCast(tex));
  auto sz = Size(p.brush().w(),p.brush().h());
  if(sz.isEmpty())
    return;

  while(sz.w<256 && sz.h<256) {
    sz.w *= 2;
    sz.h *= 2;
    }
  while(sz.w>512 || sz.h>512) {
    sz.w = (sz.w+1)/2;
    sz.h = (sz.h+1)/2;
    }
  p.drawRect(10,50,sz.w,sz.h,
             0,0,p.brush().w(),p.brush().h());
  }

void Renderer::draw(Tempest::Attachment& result, Tempest::Encoder<CommandBuffer>& cmd, uint8_t cmdId) {
  auto wview = Gothic::inst().worldView();
  if(wview==nullptr) {
    cmd.setFramebuffer({{result, Vec4(), Tempest::Preserve}});
    return;
    }

  static bool updFr = true;
  if(updFr){
    if(wview->mainLight().dir().y>0) {
      frustrum[SceneGlobals::V_Shadow0].make(shadow[0],shadowMap[0].w(),shadowMap[0].h());
      frustrum[SceneGlobals::V_Shadow1].make(shadow[1],shadowMap[1].w(),shadowMap[1].h());
      } else {
      frustrum[SceneGlobals::V_Shadow0].clear();
      frustrum[SceneGlobals::V_Shadow1].clear();
      }
    frustrum[SceneGlobals::V_Main].make(viewProj,zbuffer.w(),zbuffer.h());
    wview->visibilityPass(frustrum);
    }

  wview->preFrameUpdate(view,proj,zNear,zFar,shadow,Gothic::inst().world()->tickCount(),cmdId);

  drawHiZ(cmd,*wview,cmdId);

  for(uint8_t i=0; i<Resources::ShadowLayers; ++i) {
    if(shadowMap[i].isEmpty())
      continue;
    cmd.setFramebuffer({}, {shadowMap[i], 0.f, Tempest::Preserve});
    if(wview->mainLight().dir().y>0)
      wview->drawShadow(cmd,cmdId,i);
    }

  wview->prepareSky(cmd,cmdId);

  if(Gothic::inst().doMeshShading()) {
    cmd.setFramebuffer({{lightingBuf, Vec4(),           Tempest::Preserve},
                        {gbufDiffuse, Tempest::Discard, Tempest::Preserve},
                        {gbufNormal,  Tempest::Discard, Tempest::Preserve},
                        {gbufDepth,   1.f,              Tempest::Preserve}},
                       {zbuffer, Tempest::Preserve, Tempest::Preserve});
    } else {
    cmd.setFramebuffer({{lightingBuf, Vec4(),           Tempest::Preserve},
                        {gbufDiffuse, Tempest::Discard, Tempest::Preserve},
                        {gbufNormal,  Tempest::Discard, Tempest::Preserve},
                        {gbufDepth,   1.f,              Tempest::Preserve}},
                       {zbuffer, 1.f, Tempest::Preserve});
    }
  wview->drawGBuffer(cmd,cmdId);

  drawSSAO(result,cmd,*wview);

  cmd.setFramebuffer({{result, Tempest::Preserve, Tempest::Preserve}}, {zbuffer, Tempest::Preserve, Tempest::Discard});
  wview->drawLights (cmd,cmdId);
  wview->drawSky    (cmd,cmdId);
  wview->drawMain   (cmd,cmdId);
  wview->drawFog    (cmd,cmdId);
  }

void Renderer::drawHiZ(Tempest::Encoder<Tempest::CommandBuffer>& cmd, WorldView& wview, uint8_t cmdId) {
  if(!Gothic::inst().doMeshShading())
    return;

  cmd.setFramebuffer({}, {zbuffer, 1.f, Tempest::Preserve});
  wview.drawHiZ(cmd,cmdId);

  uint32_t w = uint32_t(hiZPot.w()), h = uint32_t(hiZPot.h());
  cmd.setFramebuffer({});
  cmd.setUniforms(Shaders::inst().hiZPot, uboHiZPot);
  cmd.dispatchThreads(w,h);
  for(uint32_t i=0; i<uboZMip.size(); ++i) {
    w = w/2;
    h = h/2;
    cmd.setUniforms(Shaders::inst().hiZMip, uboZMip[i]);
    cmd.dispatchThreads(std::max<size_t>(w,1),std::max<size_t>(h,1));
    }
  }

void Renderer::drawDSM(Tempest::Attachment& result, Tempest::Encoder<Tempest::CommandBuffer>& cmd, const WorldView& view) {
  static bool useDsm = true;

  if(!useDsm || !Gothic::inst().doRayQuery()) {
    cmd.setFramebuffer({{result, Tempest::Discard, Tempest::Preserve}});
    cmd.setUniforms(Shaders::inst().copy,uboCopy);
    cmd.draw(Resources::fsqVbo());
    return;
    }
  }

void Renderer::drawSSAO(Tempest::Attachment& result, Encoder<CommandBuffer>& cmd, const WorldView& view) {
  static bool useSsao = true;

  if(!useSsao || !settings.zCloudShadowScale) {
    cmd.setFramebuffer({{result, Tempest::Discard, Tempest::Preserve}});
    cmd.setUniforms(Shaders::inst().copy,uboCopy);
    cmd.draw(Resources::fsqVbo());
    return;
    }

  {
  // ssao
  struct PushSsao {
    Matrix4x4 mvp;
  } push;
  push.mvp = viewProj;

  cmd.setFramebuffer({{ssao.ssaoBuf, Tempest::Discard, Tempest::Preserve}});
  cmd.setUniforms(*ssao.ssaoPso,ssao.uboSsao,&push,sizeof(push));
  cmd.draw(Resources::fsqVbo());
  }

  for(int i=0; i<2; ++i) {
    struct PushSsao {
      Vec3  clipInfo;
      float sharpness;
      Vec2  invResolutionDirection;
    } push;
    push.clipInfo               = clipInfo;
    push.sharpness              = 1.f;
    push.invResolutionDirection = Vec2(1.f/float(ssao.ssaoBuf.w()), 1.f/float(ssao.ssaoBuf.h()));
    if(i==0)
      push.invResolutionDirection.y = 0; else
      push.invResolutionDirection.x = 0;

    if(i==0)
      cmd.setFramebuffer({{ssao.blurBuf, Tempest::Discard, Tempest::Preserve}}); else
      cmd.setFramebuffer({{ssao.ssaoBuf, Tempest::Discard, Tempest::Preserve}});
    cmd.setUniforms(Shaders::inst().bilateralBlur,ssao.uboBlur[i],&push,sizeof(push));
    cmd.draw(Resources::fsqVbo());
    }

  {
  struct PushSsao {
    Matrix4x4 mvpInv;
    Vec3      ambient;
    float     padd0 = 0;
    Vec3      ldir;
  } push;
  push.mvpInv  = viewProj;
  push.mvpInv.inverse();
  push.ambient = view.ambientLight();
  push.ldir    = view.mainLight().dir();

  cmd.setFramebuffer({{result, Tempest::Discard, Tempest::Preserve}});
  cmd.setUniforms(*ssao.ssaoComposePso,ssao.uboCompose,&push,sizeof(push));
  cmd.draw(Resources::fsqVbo());
  }
  }

Tempest::Attachment Renderer::screenshoot(uint8_t frameId) {
  auto& device = Resources::device();
  device.waitIdle();

  uint32_t w    = uint32_t(zbuffer.w());
  uint32_t h    = uint32_t(zbuffer.h());
  auto     img  = device.attachment(Tempest::TextureFormat::RGBA8,w,h);

  CommandBuffer cmd;
  {
  auto enc = cmd.startEncoding(device);
  draw(img,enc,frameId);
  }

  Fence sync = device.fence();
  device.submit(cmd,sync);
  sync.wait();

  return img;
  }

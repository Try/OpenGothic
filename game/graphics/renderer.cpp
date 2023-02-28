#include "renderer.h"

#include <Tempest/Color>
#include <Tempest/Fence>
#include <Tempest/Log>

#include "ui/inventorymenu.h"
#include "camera.h"
#include "gothic.h"

using namespace Tempest;

static uint32_t nextPot(uint32_t x) {
  x--;
  x |= x >> 1;
  x |= x >> 2;
  x |= x >> 4;
  x |= x >> 8;
  x |= x >> 16;
  x++;
  return x;
  }

static std::string_view toStr(TextureFormat f) {
  switch (f) {
    case TextureFormat::Depth16:   return "Depth16";
    case TextureFormat::Depth24x8: return "Depth24x8";
    case TextureFormat::Depth24S8: return "Depth24S8";
    case TextureFormat::Depth32F:  return "Depth32F";
    default: return "?";
    }
  }

Renderer::Renderer(Tempest::Swapchain& swapchain)
  : swapchain(swapchain) {
  auto& device = Resources::device();
  view.identity();

  static const TextureFormat sfrm[] = {
    TextureFormat::Depth16,
    TextureFormat::Depth24x8,
    TextureFormat::Depth32F,
    };

  for(auto& i:sfrm) {
    if(device.properties().hasDepthFormat(i) && device.properties().hasSamplerFormat(i)) {
      shadowFormat = i;
      break;
      }
    }

  static const TextureFormat zfrm[] = {
    TextureFormat::Depth32F,
    TextureFormat::Depth24x8,
    TextureFormat::Depth16,
    };
  for(auto& i:zfrm) {
    if(device.properties().hasDepthFormat(i) && device.properties().hasSamplerFormat(i)){
      zBufferFormat = i;
      break;
      }
    }

  Log::i("GPU = ",device.properties().name);
  Log::i("Depth format = ",toStr(zBufferFormat)," Shadow format = ",toStr(shadowFormat));

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

  auto smpN = Sampler::nearest();
  smpN.setClamping(ClampMode::ClampToEdge);

  zbuffer        = device.zbuffer(zBufferFormat,w,h);
  zbufferItem    = device.zbuffer(zBufferFormat,w,h);

  if(Gothic::inst().doMeshShading()) {
    uint32_t pw = nextPot(w);
    uint32_t ph = nextPot(h);

    uint32_t hw = pw;
    uint32_t hh = ph;
    while(hw>64 || hh>64) {
      hw = std::max(1u, (hw+1)/2u);
      hh = std::max(1u, (hh+1)/2u);
      }

    std::vector<uint32_t> zb(hw*hh, 0);
    hiZRaw = device.ssbo(zb);
    hiZ    = device.image2d(TextureFormat::R16, hw, hh, true);

    uboHiZRaw = device.descriptors(Shaders::inst().hiZRaw);
    uboHiZRaw.set(0, zbuffer, smpN);
    uboHiZRaw.set(1, hiZRaw);
    uboHiZRaw.set(2, hiZ);

    uboHiZPot = device.descriptors(Shaders::inst().hiZPot);
    uboHiZPot.set(0, zbuffer, smpN);
    uboHiZPot.set(1, hiZRaw);
    uboHiZPot.set(2, hiZ);

    uboZMip.clear();
    for(uint32_t i=0; (hw>1 && hh>1); ++i) {
      hw = std::max(1u, hw/2u);
      hh = std::max(1u, hh/2u);
      auto& ubo = uboZMip.emplace_back(device.descriptors(Shaders::inst().hiZMip));
      ubo.set(0, hiZ, smpN, i);
      ubo.set(1, hiZ, smpN, i+1);
      }
    }

  if(smSize>0) {
    for(int i=0; i<Resources::ShadowLayers; ++i)
      shadowMap[i] = device.zbuffer(shadowFormat,smSize,smSize);
    }

  sceneOpaque  = device.attachment(TextureFormat::R11G11B10UF,swapchain.w(),swapchain.h());
  sceneLinear  = device.attachment(TextureFormat::R11G11B10UF,swapchain.w(),swapchain.h());
  sceneDepth   = device.attachment(TextureFormat::R32F,       swapchain.w(),swapchain.h());

  gbufDiffuse  = device.attachment(TextureFormat::RGBA8,      swapchain.w(),swapchain.h());
  gbufNormal   = device.attachment(TextureFormat::R11G11B10UF,swapchain.w(),swapchain.h());

  uboCopy = device.descriptors(Shaders::inst().copy);
  uboCopy.set(0,sceneOpaque,Sampler::nearest());

  uboCopyDepth = device.descriptors(Shaders::inst().copy);
  uboCopyDepth.set(0, zbuffer);

  if(Gothic::inst().doRayQuery() && Resources::device().properties().bindless.nonUniformIndexing &&
     settings.shadowResolution>0)
    shadow.composePso = &Shaders::inst().shadowResolveRq;
  else if(settings.shadowResolution>0)
    shadow.composePso = &Shaders::inst().shadowResolveSh;
  else
    shadow.composePso = &Shaders::inst().shadowResolve;

  for(size_t i=0; i<Resources::MaxFramesInFlight; ++i)
    shadow.ubo[i] = device.descriptors(*shadow.composePso);

  //ssao.ssaoBuf = device.attachment(ssao.aoFormat, swapchain.w(),swapchain.h());
  ssao.ssaoBuf = device.image2d(ssao.aoFormat, swapchain.w(),swapchain.h());
  if(Gothic::inst().doRayQuery() && false) {
    // disabled
    //ssao.ssaoPso        = &Shaders::inst().ssaoRq;
    ssao.ssaoComposePso = &Shaders::inst().ssaoCompose;
    } else {
    ssao.ssaoPso        = &Shaders::inst().ssao;
    ssao.ssaoComposePso = &Shaders::inst().ssaoCompose;
    }
  ssao.uboSsao = device.descriptors(*ssao.ssaoPso);
  ssao.uboSsao.set(0, ssao.ssaoBuf);
  ssao.uboSsao.set(1, sceneOpaque, smpN);
  ssao.uboSsao.set(2, gbufDiffuse, smpN);
  ssao.uboSsao.set(3, gbufNormal,  smpN);
  ssao.uboSsao.set(4, zbuffer,     smpN);

  ssao.uboCompose = device.descriptors(*ssao.ssaoComposePso);
  ssao.uboCompose.set(0, sceneOpaque,  smpN);
  ssao.uboCompose.set(1, gbufDiffuse,  smpN);
  ssao.uboCompose.set(2, gbufNormal,   smpN);
  ssao.uboCompose.set(3, zbuffer,      smpN);
  ssao.uboCompose.set(4, ssao.ssaoBuf, smpN);

  tonemapping.pso     = &Shaders::inst().tonemapping;
  tonemapping.uboTone = device.descriptors(*tonemapping.pso);

  prepareUniforms();
  }

void Renderer::initSettings() {
  settings.zEnvMappingEnabled = Gothic::inst().settingsGetI("ENGINE","zEnvMappingEnabled")!=0;
  settings.zCloudShadowScale  = Gothic::inst().settingsGetI("ENGINE","zCloudShadowScale") !=0;
  }

void Renderer::onWorldChanged() {
  auto wview = Gothic::inst().worldView();
  if(wview!=nullptr) {
    wview->onTlasChanged.bind(this,&Renderer::setupTlas);
    }
  prepareUniforms();
  }

void Renderer::setCameraView(const Camera& camera) {
  view     = camera.view();
  proj     = camera.projective();
  viewProj = camera.viewProj();
  if(auto wview=Gothic::inst().worldView()) {
    for(size_t i=0; i<Resources::ShadowLayers; ++i)
      shadowMatrix[i] = camera.viewShadow(wview->mainLight().dir(),i);
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

  tonemapping.uboTone.set(0, sceneLinear);

  for(size_t i=0; i<Resources::MaxFramesInFlight; ++i) {
    auto& u = shadow.ubo[i];
    u.set(0, wview->sceneGlobals().uboGlobalPf[i][SceneGlobals::V_Main]);
    u.set(1, gbufDiffuse);
    u.set(2, gbufNormal);
    u.set(3, zbuffer);

    for(size_t r=0; r<Resources::ShadowLayers; ++r) {
      if(shadowMap[r].isEmpty())
        continue;
      u.set(4+r, shadowMap[r]);
      }
    }
  setupTlas(nullptr);

  const Texture2d* sh[Resources::ShadowLayers] = {};
  for(size_t i=0; i<Resources::ShadowLayers; ++i)
    if(!shadowMap[i].isEmpty()) {
      sh[i] = &textureCast(shadowMap[i]);
      }
  wview->setShadowMaps(sh);

  wview->setHiZ(textureCast(hiZ));
  wview->setGbuffer(textureCast(gbufDiffuse), textureCast(gbufNormal));
  wview->setSceneImages(textureCast(sceneOpaque), textureCast(sceneDepth), zbuffer);
  wview->setupUbo();
  }

void Renderer::setupTlas(const Tempest::AccelerationStructure* tlas) {
  auto wview = Gothic::inst().worldView();
  if(wview==nullptr)
    return;
  auto& scene = wview->sceneGlobals();
  if(scene.tlas==nullptr)
    return;

  if(false /*ssao.ssaoPso==&Shaders::inst().ssaoRq*/) {
    ssao.uboSsao.set(5,wview->landscapeTlas());
    }

  if(shadow.composePso==&Shaders::inst().shadowResolveRq) {
    for(size_t i=0; i<Resources::MaxFramesInFlight; ++i) {
      auto& u = shadow.ubo[i];
      u.set(7, scene.bindless.tex);
      u.set(8, scene.bindless.vbo);
      u.set(9, scene.bindless.ibo);
      u.set(10,scene.bindless.iboOffset);

      u.set(6, *scene.tlas);
      }
    }
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
  // auto& tex = shadowMap[1];
  // auto& tex = shadowMap[0];

  p.setBrush(Brush(textureCast(tex),Painter::Alpha,ClampMode::ClampToBorder));
  auto sz = Size(p.brush().w(),p.brush().h());
  if(sz.isEmpty())
    return;

  const int size = 200;
  while(sz.w<size && sz.h<size) {
    sz.w *= 2;
    sz.h *= 2;
    }
  while(sz.w>size*2 || sz.h>size*2) {
    sz.w = (sz.w+1)/2;
    sz.h = (sz.h+1)/2;
    }
  p.drawRect(10,50,sz.w,sz.h,
             0,0,p.brush().w(),p.brush().h());
  }

void Renderer::draw(Tempest::Attachment& result, Tempest::Encoder<CommandBuffer>& cmd, uint8_t fId) {
  auto wview = Gothic::inst().worldView();
  if(wview==nullptr) {
    cmd.setFramebuffer({{result, Vec4(), Tempest::Preserve}});
    return;
    }

  static bool updFr = true;
  if(updFr){
    if(wview->mainLight().dir().y>Camera::minShadowY) {
      frustrum[SceneGlobals::V_Shadow0].make(shadowMatrix[0],shadowMap[0].w(),shadowMap[0].h());
      frustrum[SceneGlobals::V_Shadow1].make(shadowMatrix[1],shadowMap[1].w(),shadowMap[1].h());
      } else {
      frustrum[SceneGlobals::V_Shadow0].clear();
      frustrum[SceneGlobals::V_Shadow1].clear();
      }
    frustrum[SceneGlobals::V_Main].make(viewProj,zbuffer.w(),zbuffer.h());
    wview->visibilityPass(frustrum);
    }

  wview->preFrameUpdate(view,proj,zNear,zFar,shadowMatrix,Gothic::inst().world()->tickCount(),fId);

  drawHiZ(cmd,*wview,fId);
  wview->prepareSky(cmd,fId);

  drawGBuffer      (cmd,fId,*wview);
  drawShadowMap    (cmd,fId,*wview);
  drawShadowResolve(sceneOpaque,cmd,fId,*wview);
  stashDepthAux(cmd,fId);

  wview->prepareFog(cmd,fId);

  drawSSAO(sceneLinear,cmd,*wview);
  cmd.setFramebuffer({{sceneLinear, Tempest::Preserve, Tempest::Preserve}}, {zbuffer, Tempest::Preserve, Tempest::Preserve});
  wview->drawLights     (cmd,fId);
  wview->drawWater      (cmd,fId);

  wview->drawSky        (cmd,fId);
  wview->drawTranslucent(cmd,fId);

  cmd.setFramebuffer({{sceneLinear, Tempest::Preserve, Tempest::Preserve}});
  wview->drawFog    (cmd,fId);

  cmd.setFramebuffer({{result, Tempest::Discard, Tempest::Preserve}});
  drawTonemapping(cmd);
  }

void Renderer::drawTonemapping(Tempest::Encoder<Tempest::CommandBuffer>& cmd) {
  struct Push {
    float exposureInv = 1.0;
    };
  Push p;
  if(auto wview = Gothic::inst().worldView()) {
    p.exposureInv = wview->sky().autoExposure();
    }

  static float dbgExposure = -1;
  if(dbgExposure>0)
    p.exposureInv = dbgExposure;

  cmd.setUniforms(*tonemapping.pso, tonemapping.uboTone, &p, sizeof(p));
  cmd.draw(Resources::fsqVbo());
  }

void Renderer::stashDepthAux(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId) {
  auto& device = Resources::device();
  if(!device.properties().hasSamplerFormat(zBufferFormat))
    return;
  cmd.setFramebuffer({{sceneDepth, Tempest::Discard, Tempest::Preserve}});
  cmd.setUniforms(Shaders::inst().copy, uboCopyDepth);
  cmd.draw(Resources::fsqVbo());
  }

void Renderer::drawHiZ(Tempest::Encoder<Tempest::CommandBuffer>& cmd, WorldView& wview, uint8_t fId) {
  if(!Gothic::inst().doMeshShading())
    return;

  cmd.setFramebuffer({}, {zbuffer, 1.f, Tempest::Preserve});
  wview.drawHiZ(cmd,fId);

  cmd.setFramebuffer({});
  cmd.setUniforms(Shaders::inst().hiZRaw, uboHiZRaw);
  cmd.dispatchThreads(uint32_t(zbuffer.w()),uint32_t(zbuffer.h()));

  cmd.setFramebuffer({});
  cmd.setUniforms(Shaders::inst().hiZPot, uboHiZPot);
  cmd.dispatchThreads(uint32_t(hiZ.w()),uint32_t(hiZ.h()));

  uint32_t w = uint32_t(hiZ.w()), h = uint32_t(hiZ.h());
  for(uint32_t i=0; i<uboZMip.size(); ++i) {
    w = w/2;
    h = h/2;
    cmd.setUniforms(Shaders::inst().hiZMip, uboZMip[i]);
    cmd.dispatchThreads(std::max<size_t>(w,1),std::max<size_t>(h,1));
    }
  }

void Renderer::drawGBuffer(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId, WorldView& view) {
  if(Gothic::inst().doMeshShading()) {
    cmd.setFramebuffer({{gbufDiffuse, Tempest::Vec4(),  Tempest::Preserve},
                        {gbufNormal,  Tempest::Discard, Tempest::Preserve}},
                       {zbuffer, Tempest::Preserve, Tempest::Preserve});
    } else {
    cmd.setFramebuffer({{gbufDiffuse, Tempest::Discard, Tempest::Preserve},
                        {gbufNormal,  Tempest::Discard, Tempest::Preserve}},
                       {zbuffer, 1.f, Tempest::Preserve});
    }
  view.drawGBuffer(cmd,fId);
  }

void Renderer::drawShadowMap(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId, WorldView& view) {
  if(settings.shadowResolution<=0)
    return;

  for(uint8_t i=0; i<Resources::ShadowLayers; ++i) {
    cmd.setFramebuffer({}, {shadowMap[i], 0.f, Tempest::Preserve});
    if(view.mainLight().dir().y > Camera::minShadowY)
      view.drawShadow(cmd,fId,i);
    }
  }

void Renderer::drawShadowResolve(Tempest::Attachment& result, Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId, const WorldView& view) {
  static bool useDsm = true;
  if(!useDsm)
    return;

  cmd.setFramebuffer({{result, Tempest::Discard, Tempest::Preserve}});
  cmd.setUniforms(*shadow.composePso,shadow.ubo[fId]);
  cmd.draw(Resources::fsqVbo());
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
    Matrix4x4 mvpInv;
  } push;
  push.mvp    = viewProj;
  push.mvpInv = viewProj;
  push.mvpInv.inverse();

  //cmd.setFramebuffer({{ssao.ssaoBuf, Tempest::Discard, Tempest::Preserve}});
  //cmd.setUniforms(*ssao.ssaoPso,ssao.uboSsao,&push,sizeof(push));
  //cmd.draw(Resources::fsqVbo());
  cmd.setFramebuffer({});
  cmd.setUniforms(*ssao.ssaoPso, ssao.uboSsao, &push, sizeof(push));
  cmd.dispatchThreads(size_t(result.w()), size_t(result.h()));
  }

  {
  struct PushSsao {
    Vec3      ambient;
    float     padd0 = 0;
    Vec3      ldir;
    float     padd1 = 0;
    Vec3      clipInfo;
  } push;
  push.ambient  = view.ambientLight();
  push.ldir     = view.mainLight().dir();
  push.clipInfo = clipInfo;

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

  // debug
  auto d16     = device.attachment(TextureFormat::R16,    swapchain.w(),swapchain.h());
  auto normals = device.attachment(TextureFormat::RGBA16, swapchain.w(),swapchain.h());

  auto ubo = device.descriptors(Shaders::inst().copy);
  ubo.set(0,gbufNormal,Sampler::nearest());
  {
  auto enc = cmd.startEncoding(device);
  enc.setFramebuffer({{normals,Tempest::Discard,Tempest::Preserve}});
  enc.setUniforms(Shaders::inst().copy,ubo);
  enc.draw(Resources::fsqVbo());
  }
  device.submit(cmd,sync);
  sync.wait();

  ubo.set(0,zbuffer,Sampler::nearest());
  {
  auto enc = cmd.startEncoding(device);
  enc.setFramebuffer({{d16,Tempest::Discard,Tempest::Preserve}});
  enc.setUniforms(Shaders::inst().copy,ubo);
  enc.draw(Resources::fsqVbo());
  }
  device.submit(cmd,sync);
  sync.wait();

  auto pm  = device.readPixels(textureCast(normals));
  pm.save("gbufNormal.png");

  pm  = device.readPixels(textureCast(d16));
  pm.save("zbuffer.hdr");

  return img;
  }

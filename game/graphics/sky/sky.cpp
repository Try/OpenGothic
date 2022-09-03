#include "sky.h"

#include <Tempest/Application>
#include <Tempest/CommandBuffer>
#include <Tempest/Fence>

#include <cctype>

#include "graphics/shaders.h"
#include "world/world.h"
#include "gothic.h"
#include "resources.h"

using namespace Tempest;

Sky::Sky(const SceneGlobals& scene)
  :scene(scene) {
  auto& device = Resources::device();

  transLut     = device.attachment(lutFormat,256, 64);
  multiScatLut = device.attachment(lutFormat, 32, 32);
  viewLut      = device.attachment(lutFormat,128, 64);
  fogLut       = device.attachment(lutFormat,256,128);  // TODO: use propper 3d texture

  /* https://bartwronski.files.wordpress.com/2014/08/bwronski_volumetric_fog_siggraph2014.pdf
   * page 25 160*90*64 = ~720p
   */
  if(use3dFog)
    fogLut3D = device.image3d(lutFormat,160,90,64);
  }

void Sky::setWorld(const World& world, const std::pair<Vec3, Vec3>& bbox) {
  auto& wname  = world.name();
  auto  dot    = wname.rfind('.');
  auto  name   = dot==std::string::npos ? wname : wname.substr(0,dot);

  for(size_t i=0;i<2;++i) {
    day  .lay[i].texture = skyTexture(name,true, i);
    night.lay[i].texture = skyTexture(name,false,i);
    }

  /*
    zSunName=unsun5.tga
    zSunSize=200
    zSunAlpha=230
    zMoonName=moon.tga
    zMoonSize=400
    zMoonAlpha=255
    */
  sun          = Resources::loadTexture(Gothic::settingsGetS("SKY_OUTDOOR","zSunName"));
  // auto& moon   = gothic.settingsGetS("SKY_OUTDOOR","zMoonName");

  minZ = bbox.first.z;
  setupUbo();
  }

void Sky::setupUbo() {
  auto& device = Resources::device();
  auto  smp    = Sampler::trillinear();
  auto  smpB   = Sampler::bilinear();
  smpB.setClamping(ClampMode::ClampToEdge);

  uboMultiScatLut = device.descriptors(Shaders::inst().skyMultiScattering);
  uboMultiScatLut.set(0, transLut, smpB);

  uboSkyViewLut = device.descriptors(Shaders::inst().skyViewLut);
  uboSkyViewLut.set(0, transLut,     smpB);
  uboSkyViewLut.set(1, multiScatLut, smpB);

  uboFogViewLut = device.descriptors(Shaders::inst().fogViewLut);
  uboFogViewLut.set(0, transLut,       smpB);
  uboFogViewLut.set(1, multiScatLut,   smpB);
  //uboFogViewLut.set(2, *scene.shadowMap[1], smpB);

  if(use3dFog) {
    uboFogViewLut3d = device.descriptors(Shaders::inst().fogViewLut3D);
    uboFogViewLut3d.set(0, transLut,     smpB);
    uboFogViewLut3d.set(1, multiScatLut, smpB);
    uboFogViewLut3d.set(3, fogLut3D);
    }

  uboFinal = device.descriptors(Shaders::inst().sky);
  uboFinal.set(0, transLut,     smpB);
  uboFinal.set(1, multiScatLut, smpB);
  uboFinal.set(2, viewLut,      smpB);
  uboFinal.set(3,*day  .lay[0].texture,smp);
  uboFinal.set(4,*day  .lay[1].texture,smp);
  uboFinal.set(5,*night.lay[0].texture,smp);
  uboFinal.set(6,*night.lay[1].texture,smp);

  uboFog = device.descriptors(Shaders::inst().fog);
  uboFog.set(0, transLut,     smpB);
  uboFog.set(1, multiScatLut, smpB);
  uboFog.set(2, fogLut,       smpB);
  //uboFog.set(3, fogLut3D,     smpB);
  uboFog.set(4, *scene.gbufDepth, Sampler::nearest());

  if(use3dFog) {
    uboFog3d = device.descriptors(Shaders::inst().fog3d);
    uboFog3d.set(0, transLut,     smpB);
    uboFog3d.set(1, multiScatLut, smpB);
    uboFog3d.set(2, fogLut,       smpB);
    uboFog3d.set(3, fogLut3D,     smpB);
    uboFog3d.set(4, *scene.gbufDepth, Sampler::nearest());
    }
  }

void Sky::prepareSky(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint32_t frameId) {
  UboSky ubo = mkPush();

  if(!lutIsInitialized) {
    cmd.setFramebuffer({{transLut, Tempest::Discard, Tempest::Preserve}});
    cmd.setUniforms(Shaders::inst().skyTransmittance, &ubo, sizeof(ubo));
    cmd.draw(Resources::fsqVbo());

    cmd.setFramebuffer({{multiScatLut, Tempest::Discard, Tempest::Preserve}});
    cmd.setUniforms(Shaders::inst().skyMultiScattering, uboMultiScatLut, &ubo, sizeof(ubo));
    cmd.draw(Resources::fsqVbo());

    lutIsInitialized = true;
    }

  if(use3dFog) {
    cmd.setFramebuffer({});
    cmd.setUniforms(Shaders::inst().fogViewLut3D, uboFogViewLut3d, &ubo, sizeof(ubo));
    cmd.dispatchThreads(uint32_t(fogLut3D.w()),uint32_t(fogLut3D.h()));
    }

  cmd.setFramebuffer({{viewLut, Tempest::Discard, Tempest::Preserve}});
  cmd.setUniforms(Shaders::inst().skyViewLut, uboSkyViewLut, &ubo, sizeof(ubo));
  cmd.draw(Resources::fsqVbo());

  cmd.setFramebuffer({{fogLut, Tempest::Discard, Tempest::Preserve}});
  cmd.setUniforms(Shaders::inst().fogViewLut, uboFogViewLut, &ubo, sizeof(ubo));
  cmd.draw(Resources::fsqVbo());
  }

void Sky::drawSky(Tempest::Encoder<CommandBuffer>& cmd, uint32_t fId) {
  UboSky ubo = mkPush();

  Vec3 a(0,0,0), b(0,0,1);
  ubo.viewProjectInv.project(a);
  ubo.viewProjectInv.project(b);
  auto dv = b-a;
  auto len = dv.length();
  (void)len;

  cmd.setUniforms(Shaders::inst().sky, uboFinal, &ubo, sizeof(ubo));
  cmd.draw(Resources::fsqVbo());
  }

void Sky::drawFog(Tempest::Encoder<CommandBuffer>& cmd, uint32_t fId) {
  UboSky ubo = mkPush();
  if(use3dFog) {
    cmd.setUniforms(Shaders::inst().fog3d, uboFog3d, &ubo, sizeof(ubo));
    cmd.draw(Resources::fsqVbo());
    } else {
    cmd.setUniforms(Shaders::inst().fog,   uboFog,   &ubo, sizeof(ubo));
    cmd.draw(Resources::fsqVbo());
    }
  }

const Texture2d& Sky::skyLut() const {
  return textureCast(viewLut);
  }

Sky::UboSky Sky::mkPush() {
  UboSky ubo;
  auto v = scene.view;
  Vec3 plPos = Vec3(0,0,0);
  scene.viewProjectInv().project(plPos);
  ubo.plPosY = plPos.y/100.f; //meters
  v.translate(Vec3(plPos.x,0,plPos.z));

  ubo.plPosY += (-minZ)/100.f;

  ubo.viewProjectInv = scene.proj;
  ubo.viewProjectInv.mul(v);
  ubo.viewProjectInv.inverse();

  //ubo.shadow1 = scene.shadowView(1);

  auto ticks = scene.tickCount;
  auto t0 = float(ticks%90000 )/90000.f;
  auto t1 = float(ticks%270000)/270000.f;
  ubo.dxy0[0] = t0;
  ubo.dxy1[0] = t1;
  ubo.sunDir  = scene.sun.dir();
  ubo.night   = 1.f-std::min(std::max(3.f*ubo.sunDir.y,0.f),1.f);

  return ubo;
  }

std::array<float,3> Sky::mkColor(uint8_t r, uint8_t g, uint8_t b) {
  return {{r/255.f,g/255.f,b/255.f}};
  }

const Texture2d* Sky::skyTexture(std::string_view name, bool day, size_t id) {
  if(auto t = implSkyTexture(name,day,id))
    return t;
  if(auto t = implSkyTexture("",day,id))
    return t;
  return &Resources::fallbackBlack();
  }

const Texture2d* Sky::implSkyTexture(std::string_view name, bool day, size_t id) {
  char tex[256]={};
  if(!name.empty()){
    const char* format=day ? "SKYDAY_%.*s_LAYER%d_A0.TGA" : "SKYNIGHT_%.*s_LAYER%d_A0.TGA";
    std::snprintf(tex,sizeof(tex),format,int(name.size()),name.data(),int(id));
    } else {
    const char* format=day ? "SKYDAY_LAYER%d_A0.TGA" : "SKYNIGHT_LAYER%d_A0.TGA";
    std::snprintf(tex,sizeof(tex),format,int(id));
    }
  for(size_t r=0;tex[r];++r)
    tex[r]=char(std::toupper(tex[r]));
  auto r = Resources::loadTexture(tex);
  if(r==&Resources::fallbackTexture())
    return &Resources::fallbackBlack(); //format error
  return r;
  }

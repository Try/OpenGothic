#include "sky.h"

#include <Tempest/Application>
#include <Tempest/CommandBuffer>
#include <Tempest/Fence>

#include <cctype>

#include "game/gamesession.h"
#include "graphics/shaders.h"
#include "world/world.h"
#include "utils/versioninfo.h"
#include "gothic.h"
#include "resources.h"

using namespace Tempest;

Sky::Sky(const SceneGlobals& scene)
  :scene(scene) {
  auto& device = Resources::device();
  algo = EGSR;
  if(!device.properties().hasAttachFormat(Tempest::TextureFormat::RGBA32F)) {
    algo = Nishita;
    }

  if(algo==EGSR) {
    egsr.transLut     = device.attachment(egsr.lutFormat,256, 64);
    egsr.multiScatLut = device.attachment(egsr.lutFormat, 32, 32);
    egsr.viewLut      = device.attachment(egsr.lutFormat,128, 64);
    egsr.fogLut       = device.attachment(egsr.lutFormat,256,128);  // TODO: use propper 3d texture
    }
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
  auto  smp    = Sampler2d::trillinear();
  auto  smpB   = Sampler2d::bilinear();
  smpB.setClamping(ClampMode::ClampToEdge);

  if(algo==Nishita) {
    nishita.uboSky = device.descriptors(Shaders::inst().sky);
    nishita.uboSky.set(1,*day  .lay[0].texture,smp);
    nishita.uboSky.set(2,*day  .lay[1].texture,smp);
    nishita.uboSky.set(3,*night.lay[0].texture,smp);
    nishita.uboSky.set(4,*night.lay[1].texture,smp);

    nishita.uboFog = device.descriptors(Shaders::inst().fog);
    nishita.uboFog.set(1,*scene.gbufDepth,Sampler2d::nearest());
    } else {
    egsr.uboMultiScatLut = device.descriptors(Shaders::inst().skyMultiScattering);
    egsr.uboMultiScatLut.set(0, egsr.transLut, smpB);

    egsr.uboSkyViewLut = device.descriptors(Shaders::inst().skyViewLut);
    egsr.uboSkyViewLut.set(0, egsr.transLut,     smpB);
    egsr.uboSkyViewLut.set(1, egsr.multiScatLut, smpB);

    egsr.uboFogViewLut = device.descriptors(Shaders::inst().fogViewLut);
    egsr.uboFogViewLut.set(0, egsr.transLut,       smpB);
    egsr.uboFogViewLut.set(1, egsr.multiScatLut,   smpB);
    //egsr.uboFogViewLut.set(2, *scene.shadowMap[1], smpB);

    egsr.uboFinal = device.descriptors(Shaders::inst().skyEGSR);
    egsr.uboFinal.set(0, egsr.transLut,     smpB);
    egsr.uboFinal.set(1, egsr.multiScatLut, smpB);
    egsr.uboFinal.set(2, egsr.viewLut,      smpB);
    egsr.uboFinal.set(3,*day  .lay[0].texture,smp);
    egsr.uboFinal.set(4,*day  .lay[1].texture,smp);
    egsr.uboFinal.set(5,*night.lay[0].texture,smp);
    egsr.uboFinal.set(6,*night.lay[1].texture,smp);

    egsr.uboFog = device.descriptors(Shaders::inst().fogEGSR);
    egsr.uboFog.set(0, egsr.transLut,     smpB);
    egsr.uboFog.set(1, egsr.multiScatLut, smpB);
    egsr.uboFog.set(2, egsr.fogLut,       smpB);
    egsr.uboFog.set(3, *scene.gbufDepth,  Sampler2d::nearest());
    }
  }

void Sky::prepareSky(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint32_t frameId) {
  if(algo==EGSR) {
    UboSky ubo = mkPush();

    if(!egsr.lutIsInitialized) {
      cmd.setFramebuffer({{egsr.transLut, Tempest::Discard, Tempest::Preserve}});
      cmd.setUniforms(Shaders::inst().skyTransmittance, &ubo, sizeof(ubo));
      cmd.draw(Resources::fsqVbo());

      cmd.setFramebuffer({{egsr.multiScatLut, Tempest::Discard, Tempest::Preserve}});
      cmd.setUniforms(Shaders::inst().skyMultiScattering, egsr.uboMultiScatLut, &ubo, sizeof(ubo));
      cmd.draw(Resources::fsqVbo());

      egsr.lutIsInitialized = true;
      }

    cmd.setFramebuffer({{egsr.viewLut, Tempest::Discard, Tempest::Preserve}});
    cmd.setUniforms(Shaders::inst().skyViewLut, egsr.uboSkyViewLut, &ubo, sizeof(ubo));
    cmd.draw(Resources::fsqVbo());

    cmd.setFramebuffer({{egsr.fogLut, Tempest::Discard, Tempest::Preserve}});
    cmd.setUniforms(Shaders::inst().fogViewLut, egsr.uboFogViewLut, &ubo, sizeof(ubo));
    cmd.draw(Resources::fsqVbo());
    }
  }

void Sky::drawSky(Tempest::Encoder<CommandBuffer>& cmd, uint32_t fId) {
  UboSky ubo = mkPush();

  Vec3 a(0,0,0), b(0,0,1);
  ubo.viewProjectInv.project(a);
  ubo.viewProjectInv.project(b);
  auto dv = b-a;
  auto len = dv.length();
  (void)len;

  if(algo==EGSR) {
    cmd.setUniforms(Shaders::inst().skyEGSR, egsr.uboFinal, &ubo, sizeof(ubo));
    cmd.draw(Resources::fsqVbo());
    } else {
    cmd.setUniforms(Shaders::inst().sky, nishita.uboSky, &ubo, sizeof(ubo));
    cmd.draw(Resources::fsqVbo());
    }
  }

void Sky::drawFog(Tempest::Encoder<CommandBuffer>& cmd, uint32_t fId) {
  UboSky ubo = mkPush();
  if(algo==EGSR) {
    cmd.setUniforms(Shaders::inst().fogEGSR, egsr.uboFog, &ubo, sizeof(ubo));
    cmd.draw(Resources::fsqVbo());
    } else {
    cmd.setUniforms(Shaders::inst().fog, nishita.uboFog, &ubo, sizeof(ubo));
    cmd.draw(Resources::fsqVbo());
    }
  }

const Texture2d& Sky::skyLut() const {
  return textureCast(egsr.viewLut);
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

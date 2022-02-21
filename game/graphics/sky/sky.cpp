#include "sky.h"

#include <Tempest/Application>
#include <Tempest/CommandBuffer>

#include <cctype>

#include "game/gamesession.h"
#include "graphics/shaders.h"
#include "gothic.h"
#include "world/world.h"
#include "utils/versioninfo.h"
#include "resources.h"

using namespace Tempest;

Sky::Sky(const SceneGlobals& scene)
  :scene(scene) {
  }

void Sky::setWorld(const World &world) {
  auto& wname  = world.name();
  auto  dot    = wname.rfind('.');
  auto  name   = dot==std::string::npos ? wname : wname.substr(0,dot);

  for(size_t i=0;i<2;++i) {
    day  .lay[i].texture = skyTexture(name.c_str(),true, i);
    night.lay[i].texture = skyTexture(name.c_str(),false,i);
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

  setupUbo();
  }

void Sky::setupUbo() {
  auto& device = Resources::device();
  auto  smp    = Sampler2d::trillinear();
  auto  smpB   = Sampler2d::bilinear();
  smpB.setClamping(ClampMode::ClampToEdge);

  for(auto& i:perFrame) {
    i.uboSky    = device.descriptors(Shaders::inst().sky);

    i.uboSky.set(1,*day  .lay[0].texture,smp);
    i.uboSky.set(2,*day  .lay[1].texture,smp);
    i.uboSky.set(3,*night.lay[0].texture,smp);
    i.uboSky.set(4,*night.lay[1].texture,smp);

    i.uboFog = device.descriptors(Shaders::inst().fog);
    i.uboFog.set(1,*scene.gbufDepth,Sampler2d::nearest());
    }

  sky.transLut        = device.attachment(sky.lutFormat,256, 64);
  sky.multiScatLut    = device.attachment(sky.lutFormat, 32, 32);
  sky.viewLut         = device.attachment(sky.lutFormat,256,128);
  sky.fogLut          = device.attachment(sky.lutFormat,128,64);

  sky.uboMultiScatLut = device.descriptors(Shaders::inst().skyMultiScattering);
  sky.uboMultiScatLut.set(0, sky.transLut, smpB);

  sky.uboSkyViewLut   = device.descriptors(Shaders::inst().skyViewLut);
  sky.uboSkyViewLut.set(0, sky.transLut,     smpB);
  sky.uboSkyViewLut.set(1, sky.multiScatLut, smpB);

  sky.uboFogViewLut   = device.descriptors(Shaders::inst().fogViewLut);
  sky.uboFogViewLut.set(0, sky.transLut,     smpB);
  sky.uboFogViewLut.set(1, sky.multiScatLut, smpB);

  sky.uboFinal        = device.descriptors(Shaders::inst().skyPrsr);
  sky.uboFinal.set(0, sky.transLut,     smpB);
  sky.uboFinal.set(1, sky.multiScatLut, smpB);
  sky.uboFinal.set(2, sky.viewLut,      smpB);
  sky.uboFinal.set(3,*day  .lay[0].texture,smp);
  sky.uboFinal.set(4,*day  .lay[1].texture,smp);
  sky.uboFinal.set(5,*night.lay[0].texture,smp);
  sky.uboFinal.set(6,*night.lay[1].texture,smp);

  sky.uboFog          = device.descriptors(Shaders::inst().fogPrsr);
  sky.uboFog.set(0, sky.transLut,     smpB);
  sky.uboFog.set(1, sky.multiScatLut, smpB);
  sky.uboFog.set(2, sky.fogLut,       smpB);
  sky.uboFog.set(3, *scene.gbufDepth, Sampler2d::nearest());
  }

void Sky::prepareSky(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint32_t frameId) {
  UboSky ubo = mkPush();

  cmd.setFramebuffer({{sky.transLut, Tempest::Discard, Tempest::Preserve}});
  cmd.setUniforms(Shaders::inst().skyTransmittance, &ubo, sizeof(ubo));
  cmd.draw(Resources::fsqVbo());

  cmd.setFramebuffer({{sky.multiScatLut, Tempest::Discard, Tempest::Preserve}});
  cmd.setUniforms(Shaders::inst().skyMultiScattering, sky.uboMultiScatLut, &ubo, sizeof(ubo));
  cmd.draw(Resources::fsqVbo());

  cmd.setFramebuffer({{sky.viewLut, Tempest::Discard, Tempest::Preserve}});
  cmd.setUniforms(Shaders::inst().skyViewLut, sky.uboSkyViewLut, &ubo, sizeof(ubo));
  cmd.draw(Resources::fsqVbo());

  cmd.setFramebuffer({{sky.fogLut, Tempest::Discard, Tempest::Preserve}});
  cmd.setUniforms(Shaders::inst().fogViewLut, sky.uboFogViewLut, &ubo, sizeof(ubo));
  cmd.draw(Resources::fsqVbo());
  }

void Sky::drawSky(Tempest::Encoder<CommandBuffer>& cmd, uint32_t fId) {
  UboSky ubo = mkPush();

  if(ver==2) {
    cmd.setUniforms(Shaders::inst().skyPrsr, sky.uboFinal, &ubo, sizeof(ubo));
    cmd.draw(Resources::fsqVbo());
    } else {
    auto& pf = perFrame[fId];
    cmd.setUniforms(Shaders::inst().sky, pf.uboSky, &ubo, sizeof(ubo));
    cmd.draw(Resources::fsqVbo());
    }
  }

void Sky::drawFog(Tempest::Encoder<CommandBuffer>& cmd, uint32_t fId) {
  if(ver==2) {
    UboSky ubo = mkPush();
    cmd.setUniforms(Shaders::inst().fogPrsr, sky.uboFog, &ubo, sizeof(ubo));
    cmd.draw(Resources::fsqVbo());
    } else {
    UboFog ubo;
    ubo.mvp    = scene.viewProject();
    ubo.mvpInv = ubo.mvp;
    ubo.mvpInv.inverse();

    auto& pf = perFrame[fId];
    cmd.setUniforms(Shaders::inst().fog, pf.uboFog);
    //p.setUniforms(scene.storage.fog, &ubo, sizeof(ubo));
    cmd.draw(Resources::fsqVbo());
    }
  }

Sky::UboSky Sky::mkPush() {
  UboSky ubo;
  auto v = scene.view;
  Vec3 plPos = Vec3(0,0,0);
  scene.viewProjectInv().project(plPos);
  ubo.plPosY = plPos.y/100.f; //meters
  v.translate(Vec3(plPos.x,0,plPos.z));

  ubo.plPosY += 1000; // TODO: need to add minZ of map-mesh

  ubo.viewProjectInv = scene.proj;
  ubo.viewProjectInv.mul(v);
  ubo.viewProjectInv.inverse();

  auto ticks = scene.tickCount;
  auto t0 = float(ticks%90000 )/90000.f;
  auto t1 = float(ticks%270000)/270000.f;
  ubo.dxy0[0] = t0;
  ubo.dxy1[0] = t1;
  ubo.sunDir  = scene.sun.dir();
  ubo.night   = std::min(std::max(-ubo.sunDir.y*3.f,0.f),1.f);//nightFlt;

  return ubo;
  }

void Sky::setDayNight(float dayF) {
  nightFlt = 1.f-dayF;
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

#include "sky.h"

#include <Tempest/Application>
#include <Tempest/CommandBuffer>
#include <Tempest/Platform>
#include <Tempest/Fence>

#include <cctype>

#include "graphics/shaders.h"
#include "utils/string_frm.h"
#include "world/world.h"
#include "gothic.h"
#include "resources.h"

using namespace Tempest;

// https://www.slideshare.net/LukasLang/physically-based-lighting-in-unreal-engine-4
// https://www.slideshare.net/DICEStudio/moving-frostbite-to-physically-based-rendering
static const float DirectSunLux  = 64'000.f;
static const float DirectMoonLux = 0.27f;
static const float StreetLight   = 10.f;

static float smoothstep(float edge0, float edge1, float x) {
  float t = std::min(std::max((x - edge0) / (edge1 - edge0), 0.f), 1.f);
  return t * t * (3.f - 2.f * t);
  };

static float linearstep(float edge0, float edge1, float x) {
  float t = std::min(std::max((x - edge0) / (edge1 - edge0), 0.f), 1.f);
  return t;
  };

Sky::Sky(const SceneGlobals& scene, const World& world, const std::pair<Tempest::Vec3, Tempest::Vec3>& bbox)
  :scene(scene) {
  auto wname  = world.name();
  auto dot    = wname.rfind('.');
  auto name   = dot==std::string::npos ? wname : wname.substr(0,dot);
  for(size_t i=0;i<2;++i) {
    clouds[0].lay[i] = skyTexture(name,true, i);
    clouds[1].lay[i] = skyTexture(name,false,i);
    }
  minZ = bbox.first.z;

  lumScale       = 5.f / DirectSunLux;
  GSunIntensity  = DirectSunLux  * lumScale;
  GMoonIntensity = DirectMoonLux * lumScale;

  /*
    zSunName=unsun5.tga
    zSunSize=200
    zSunAlpha=230
    zMoonName=moon.tga
    zMoonSize=400
    zMoonAlpha=255
    */
  sunImg       = Resources::loadTexture(Gothic::settingsGetS("SKY_OUTDOOR","zSunName"));
  sunSize      = Gothic::settingsGetF("SKY_OUTDOOR","zSunSize");
  if(sunSize<=1)
    sunSize = 200;
  moonImg      = Resources::loadTexture(Gothic::settingsGetS("SKY_OUTDOOR","zMoonName"));
  moonSize     = Gothic::settingsGetF("SKY_OUTDOOR","zMoonSize");
  if(moonSize<=1)
    moonSize = 400;

  auto& device = Resources::device();
  cloudsLut    = device.image2d   (lutRGBAFormat,  2,  1);
  transLut     = device.attachment(lutRGBFormat, 256, 64);
  multiScatLut = device.attachment(lutRGBFormat,  32, 32);
  viewLut      = device.attachment(lutRGBFormat, 128, 64);
  fogLut       = device.attachment(lutRGBFormat, 256,128);
  Gothic::inst().onSettingsChanged.bind(this,&Sky::setupSettings);
  setupSettings();
  }

Sky::~Sky() {
  Gothic::inst().onSettingsChanged.ubind(this,&Sky::setupSettings);
  }

void Sky::setupSettings() {
  auto& device = Resources::device();
  bool  fog    = Gothic::inst().settingsGetI("RENDERER_D3D","zFogRadial")!=0;

  auto q = Quality::Exponential;
  if(fog) {
    if(device.properties().hasStorageFormat(TextureFormat::R32U))
      q = Quality::VolumetricHQ; else
      q = Quality::VolumetricLQ;
    }

  if(quality==q)
    return;
  quality = q;

  lutIsInitialized = false;

  device.waitIdle();
  switch(quality) {
    case Exponential:
      fogLut3D     = StorageImage();
      shadowDw     = StorageImage();
      occlusionLut = StorageImage();
      break;
    case VolumetricLQ:
      /* https://bartwronski.files.wordpress.com/2014/08/bwronski_volumetric_fog_siggraph2014.pdf
       * page 25 160*90*64 = ~720p
       */
      fogLut3D     = device.image3d(lutRGBAFormat, 320, 176, 64);
      shadowDw     = device.image2d(TextureFormat::R16, 512, 256);
      occlusionLut = StorageImage();
      break;
    case VolumetricHQ:
      // fogLut and oclussion are decupled
      fogLut3D = device.image3d(lutRGBAFormat,128,64,32);
      shadowDw = StorageImage();
      break;
    }
  //fogLut3D = device.image3d(lutRGBAFormat,1,1,1);
  setupUbo();
  }

void Sky::drawSunMoon(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint32_t frameId, bool sun) {
  auto  m = scene.viewProject();
  auto  d = sunLight().dir();

  if(!sun) {
    // fixed pos for now
    d = Vec3::normalize({-1,1,0});
    }

  float w = 0;
  m.project(d.x, d.y, d.z, w);

  if(d.z<0)
    return;

  struct Push {
    Tempest::Vec2      pos;
    Tempest::Vec2      size;
    Tempest::Vec3      sunDir;
    float              GSunIntensity = 0;
    Tempest::Matrix4x4 viewProjectInv;
    uint32_t           isSun = 0;
    } push;
  push.pos  = Vec2(d.x,d.y)/d.z;
  if(scene.zbuffer!=nullptr) {
    push.size.x  = 2.f/float(scene.zbuffer->w());
    push.size.y  = 2.f/float(scene.zbuffer->h());
    }
  push.size          *= sun ? sunSize : (moonSize*0.25f);
  push.GSunIntensity  = sun ? GSunIntensity : (GMoonIntensity*10.f);
  push.isSun          = sun ? 1 : 0;
  push.sunDir         = d;
  push.viewProjectInv = scene.viewProjectInv();

  // HACK
  if(sun) {
    float day = sunLight().dir().y;
    float stp = linearstep(-0.07f, 0.03f, day);
    push.GSunIntensity *= stp*stp*4.f;
    } else {
    push.GSunIntensity *= isNight();
    }

  cmd.setUniforms(Shaders::inst().sun, sun ? uboSun : uboMoon, &push, sizeof(push));
  cmd.draw(6);
  }

float Sky::isNight() const {
  return 1.f - linearstep(-0.15f, 0.f, sun.dir().y);
  }

void Sky::setWorld(const World& world, const std::pair<Vec3, Vec3>& bbox) {
  setupSettings();
  }

void Sky::updateLight(const int64_t now) {
  // https://www.suncalc.org/#/52.4561,13.4033,5/2020.06.28/13:09/1/3
  const int64_t rise         = gtime( 4,45).toInt();
  const int64_t meridian     = gtime(13, 9).toInt();
  const int64_t set          = gtime(21,33).toInt();
  const int64_t midnight     = gtime(1,0,0).toInt();
  //const int64_t now          = owner.time().timeInDay().toInt();
  const float   shadowLength = 0.56f;

  float pulse = 0.f;
  if(rise<=now && now<meridian){
    pulse =  0.f + float(now-rise)/float(meridian-rise);
    }
  else if(meridian<=now && now<set){
    pulse =  1.f - float(now-meridian)/float(set-meridian);
    }
  else if(set<=now){
    pulse =  0.f - float(now-set)/float(midnight-set);
    }
  else if(now<rise){
    pulse = -1.f + (float(now)/float(rise));
    }

  {
    float k   = float(now)/float(midnight);
    float ax  = 360-360*std::fmod(k+0.25f,1.f);
    ax = ax*float(M_PI/180.0);
    sun.setDir(-std::sin(ax)*shadowLength, pulse, std::cos(ax)*shadowLength);
  }

  static float sunMul = 1;
  static float ambMul = 1;

  // irradince
  // const auto skyDay       = Vec3(0.01f, 0.18f, 0.33f)*0.2f;
  // const auto skyNight     = Vec3(0, 0, 0.000001f)*0.2f;

  float dayTint = std::max(sun.dir().y, 0.f);
  dayTint = 0.5f - std::pow(1.f - dayTint,3.f)*0.4f;

  const auto  groundAlbedo = Vec3(0.4f);
  const auto  ambientNight = groundAlbedo*StreetLight*lumScale;
  const auto  ambientDay   = groundAlbedo*(GSunIntensity*dayTint + StreetLight*lumScale);

  const auto directDay    = Vec3(0.94f, 0.87f, 0.76f); //TODO: use tLUT to guide sky color in shader
  const auto directNight  = Vec3(0.27f, 0.05f, 0.01f);

  float aDirect  = std::max(0.f,std::min(pulse*3.f,1.f));
  float aAmbient = smoothstep(-0.1f, 0.5f, pulse);

  auto clr = directNight *(1.f-aDirect ) + directDay *aDirect;
  ambient  = ambientNight*(1.f-aAmbient) + ambientDay*aAmbient;

  const float sunOcclude = smoothstep(0.0f, 0.01f, sun.dir().y);
  clr = clr*sunOcclude;

  sun.setColor(clr*sunMul);
  ambient = ambient*ambMul;

  const  float base    = smoothstep(-0.2f, 0.2f, sun.dir().y);
  static float exp     = 2.0f;
  static float moonExp = 0.001f;

  float exposure  = std::pow(base,exp);
  exposure = exposure*(1.f-moonExp) + moonExp;

  static float dbgExposure = -1;
  if(dbgExposure>0)
    exposure = dbgExposure;

  exposureInv = 1.f/exposure;
  }

void Sky::setupUbo() {
  auto& device = Resources::device();
  auto  smp    = Sampler::trillinear();
  auto  smpB   = Sampler::bilinear();
  smpB.setClamping(ClampMode::ClampToEdge);

  if(quality==VolumetricHQ)
    occlusionLut = device.image2d(TextureFormat::R32U, uint32_t(scene.zbuffer->w()), uint32_t(scene.zbuffer->h()));

  uboClouds = device.descriptors(Shaders::inst().cloudsLut);
  uboClouds.set(0, cloudsLut);
  uboClouds.set(5, *cloudsDay()  .lay[0],smp);
  uboClouds.set(6, *cloudsDay()  .lay[1],smp);
  uboClouds.set(7, *cloudsNight().lay[0],smp);
  uboClouds.set(8, *cloudsNight().lay[1],smp);

  uboTransmittance = device.descriptors(Shaders::inst().skyTransmittance);
  uboTransmittance.set(5,*cloudsDay()  .lay[0],smp);
  uboTransmittance.set(6,*cloudsDay()  .lay[1],smp);
  uboTransmittance.set(7,*cloudsNight().lay[0],smp);
  uboTransmittance.set(8,*cloudsNight().lay[1],smp);

  uboMultiScatLut = device.descriptors(Shaders::inst().skyMultiScattering);
  uboMultiScatLut.set(0, transLut, smpB);

  uboSkyViewLut = device.descriptors(Shaders::inst().skyViewLut);
  uboSkyViewLut.set(0, transLut,     smpB);
  uboSkyViewLut.set(1, multiScatLut, smpB);
  uboSkyViewLut.set(2, cloudsLut,    smpB);

  uboFogViewLut = device.descriptors(Shaders::inst().fogViewLut);
  uboFogViewLut.set(0, transLut,       smpB);
  uboFogViewLut.set(1, multiScatLut,   smpB);
  uboFogViewLut.set(2, cloudsLut,      smpB);

  uboSky = device.descriptors(Shaders::inst().sky);
  uboSky.set(0, transLut,       smpB);
  uboSky.set(1, multiScatLut,   smpB);
  uboSky.set(2, viewLut,        smpB);
  //uboSky.set(3, fogLut3D,       smpB);
  uboSky.set(4,*cloudsDay()  .lay[0],smp);
  uboSky.set(5,*cloudsDay()  .lay[1],smp);
  uboSky.set(6,*cloudsNight().lay[0],smp);
  uboSky.set(7,*cloudsNight().lay[1],smp);

  for(size_t i=0; i<Resources::MaxFramesInFlight; ++i) {
    if(quality==Exponential) {
      uboFog[i] = device.descriptors(Shaders::inst().fog);
      uboFog[i].set(0, fogLut,         smpB);
      uboFog[i].set(1, *scene.zbuffer, Sampler::nearest()); // NOTE: wanna here depthFetch from gles2
      uboFog[i].set(2, scene.uboGlobalPf[i][SceneGlobals::V_Main]);
      }

    if(quality==VolumetricLQ) {
      uboShadowDw = device.descriptors(Shaders::inst().shadowDownsample);
      uboShadowDw.set(0, shadowDw);
      uboShadowDw.set(1, *scene.shadowMap[1],Resources::shadowSampler());

      uboFogViewLut3d[i] = device.descriptors(Shaders::inst().fogViewLut3dHQ);
      uboFogViewLut3d[i].set(0, transLut,     smpB);
      uboFogViewLut3d[i].set(1, multiScatLut, smpB);
      uboFogViewLut3d[i].set(2, cloudsLut,    smpB);
      uboFogViewLut3d[i].set(3, shadowDw,     Resources::shadowSampler());
      uboFogViewLut3d[i].set(4, fogLut3D);
      uboFogViewLut3d[i].set(5, scene.uboGlobalPf[i][SceneGlobals::V_Main]);

      uboFog3d[i] = device.descriptors(Shaders::inst().fog3dLQ);
      uboFog3d[i].set(0, fogLut3D,       smpB);
      uboFog3d[i].set(1, *scene.zbuffer, Sampler::nearest());
      uboFog3d[i].set(2, scene.uboGlobalPf[i][SceneGlobals::V_Main]);
      }

    if(quality==VolumetricHQ) {
      auto smpLut3d = Sampler::bilinear();
      smpLut3d.setClamping(ClampMode::ClampToEdge);

      uboOcclusion[i] = device.descriptors(Shaders::inst().fogOcclusion);
      // uboOcclusion[i].set(0, fogLut3D,       smpB);
      uboOcclusion[i].set(1, *scene.zbuffer, Sampler::nearest());
      uboOcclusion[i].set(2, scene.uboGlobalPf[i][SceneGlobals::V_Main]);
      uboOcclusion[i].set(3, *scene.shadowMap[1], Resources::shadowSampler());
      uboOcclusion[i].set(4, occlusionLut);

      uboFogViewLut3d[i] = device.descriptors(Shaders::inst().fogViewLut3dHQ);
      uboFogViewLut3d[i].set(0, transLut,     smpB);
      uboFogViewLut3d[i].set(1, multiScatLut, smpB);
      uboFogViewLut3d[i].set(2, cloudsLut,    smpB);
      uboFogViewLut3d[i].set(3, *scene.shadowMap[1], Resources::shadowSampler());
      uboFogViewLut3d[i].set(4, fogLut3D);
      uboFogViewLut3d[i].set(5, scene.uboGlobalPf[i][SceneGlobals::V_Main]);

      uboFog3d[i] = device.descriptors(Shaders::inst().fog3dHQ);
      uboFog3d[i].set(0, fogLut3D,       smpB);
      uboFog3d[i].set(1, *scene.zbuffer, Sampler::nearest());
      uboFog3d[i].set(2, scene.uboGlobalPf[i][SceneGlobals::V_Main]);
      uboFog3d[i].set(3, *scene.shadowMap[1], Resources::shadowSampler());
      uboFog3d[i].set(4, occlusionLut);
      }
    }

  uboSun = device.descriptors(Shaders::inst().sun);
  uboSun.set(0, *sunImg);
  uboSun.set(1, transLut, smpB);

  uboMoon = device.descriptors(Shaders::inst().sun);
  uboMoon.set(0, *moonImg);
  uboMoon.set(1, transLut, smpB);
  }

void Sky::prepareSky(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint32_t frameId) {
  UboSky ubo = mkPush();

  if(!lutIsInitialized) {
    cmd.setFramebuffer({});
    cmd.setUniforms(Shaders::inst().cloudsLut, uboClouds, &ubo, sizeof(ubo));
    cmd.dispatchThreads(size_t(cloudsLut.w()),size_t(cloudsLut.h()));
    }

  if(!lutIsInitialized) {
    cmd.setFramebuffer({{transLut, Tempest::Discard, Tempest::Preserve}});
    cmd.setUniforms(Shaders::inst().skyTransmittance, uboTransmittance, &ubo, sizeof(ubo));
    cmd.draw(Resources::fsqVbo());

    cmd.setFramebuffer({{multiScatLut, Tempest::Discard, Tempest::Preserve}});
    cmd.setUniforms(Shaders::inst().skyMultiScattering, uboMultiScatLut, &ubo, sizeof(ubo));
    cmd.draw(Resources::fsqVbo());
    }

  lutIsInitialized = true;

  cmd.setFramebuffer({{viewLut, Tempest::Discard, Tempest::Preserve}});
  cmd.setUniforms(Shaders::inst().skyViewLut, uboSkyViewLut, &ubo, sizeof(ubo));
  cmd.draw(Resources::fsqVbo());
  }

void Sky::prepareFog(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint32_t frameId) {
  UboSky ubo = mkPush();

  switch(quality) {
    case Exponential: {
      cmd.setFramebuffer({{fogLut, Tempest::Discard, Tempest::Preserve}});
      cmd.setUniforms(Shaders::inst().fogViewLut, uboFogViewLut, &ubo, sizeof(ubo));
      cmd.draw(Resources::fsqVbo());
      break;
      }
    case VolumetricLQ: {
      cmd.setFramebuffer({});
      cmd.setUniforms(Shaders::inst().shadowDownsample, uboShadowDw);
      cmd.dispatchThreads(uint32_t(shadowDw.w()),uint32_t(shadowDw.h()));

      cmd.setUniforms(Shaders::inst().fogViewLut3dLQ, uboFogViewLut3d[frameId], &ubo, sizeof(ubo));
      cmd.dispatchThreads(uint32_t(fogLut3D.w()),uint32_t(fogLut3D.h()));
      break;
      }
    case VolumetricHQ: {
      const bool persistent = true;
      cmd.setFramebuffer({});
      cmd.setUniforms(Shaders::inst().fogOcclusion, uboOcclusion[frameId], &ubo, sizeof(ubo));
      if(persistent) {
        auto sz = Shaders::inst().fogOcclusion.workGroupSize();
        int  w  = (occlusionLut.w()+sz.x-1)/sz.x;
        int  h  = (occlusionLut.h()+sz.y-1)/sz.y;
        cmd.dispatch(uint32_t((w+1-1)/1), uint32_t((h+1-1)/1));
        } else{
        cmd.dispatchThreads(uint32_t(occlusionLut.w()),uint32_t(occlusionLut.h()));
        }

      cmd.setUniforms(Shaders::inst().fogViewLut3dHQ, uboFogViewLut3d[frameId], &ubo, sizeof(ubo));
      cmd.dispatchThreads(uint32_t(fogLut3D.w()),uint32_t(fogLut3D.h()));
      break;
      }
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

  auto len2 = (ubo.clipInfo.x / (ubo.clipInfo.y * 1.0 + ubo.clipInfo.z));
  (void)len2;

  cmd.setUniforms(Shaders::inst().sky, uboSky, &ubo, sizeof(ubo));
  cmd.draw(Resources::fsqVbo());

  drawSunMoon(cmd, fId, false);
  drawSunMoon(cmd, fId, true);
  }

void Sky::drawFog(Tempest::Encoder<CommandBuffer>& cmd, uint32_t fId) {
  UboSky ubo = mkPush();
  switch(quality) {
    case Exponential:
      cmd.setUniforms(Shaders::inst().fog,     uboFog[fId],   &ubo, sizeof(ubo));
      break;
    case VolumetricLQ:
      cmd.setUniforms(Shaders::inst().fog3dLQ, uboFog3d[fId], &ubo, sizeof(ubo));
      break;
    case VolumetricHQ:
      cmd.setUniforms(Shaders::inst().fog3dHQ, uboFog3d[fId], &ubo, sizeof(ubo));
      break;
    }
  cmd.draw(Resources::fsqVbo());
  }

const Texture2d& Sky::skyLut() const {
  return textureCast(viewLut);
  }

Vec2 Sky::cloudsOffset(int layer) const {
  auto ticks = scene.tickCount;
  auto t0    = float(ticks%90000 )/90000.f;
  auto t1    = float(ticks%270000)/270000.f;

  Vec2 ret = {};
  if(layer==0)
    ret.x = t0; else
    ret.x = t1;
  return ret;
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

  ubo.dxy0    = cloudsOffset(0);
  ubo.dxy1    = cloudsOffset(1);
  ubo.sunDir  = sun.dir();
  ubo.night   = isNight();

  static float rayleighScatteringScale = 33.1f;

  ubo.rayleighScatteringScale = rayleighScatteringScale;
  ubo.clipInfo                = Vec3(scene.clipInfo().x,scene.clipInfo().y,scene.clipInfo().z);
  ubo.GSunIntensity           = GSunIntensity;
  ubo.exposureInv             = exposureInv;

  return ubo;
  }

const Texture2d* Sky::skyTexture(std::string_view name, bool day, size_t id) {
  if(auto t = implSkyTexture(name,day,id))
    return t;
  if(auto t = implSkyTexture("",day,id))
    return t;
  return &Resources::fallbackBlack();
  }

const Texture2d* Sky::implSkyTexture(std::string_view name, bool day, size_t id) {
  string_frm tex("");
  if(name.empty()) {
    if(day)
      tex = string_frm("SKYDAY_LAYER",  int(id),"_A0.TGA"); else
      tex = string_frm("SKYNIGHT_LAYER",int(id),"_A0.TGA");
    } else {
    if(day)
      tex = string_frm("SKYDAY_",  name,"_LAYER",int(id),"_A0.TGA"); else
      tex = string_frm("SKYNIGHT_",name,"_LAYER",int(id),"_A0.TGA");
    }
  for(auto& i:tex)
    i = char(std::toupper(i));

  auto r = Resources::loadTexture(tex);
  if(r==&Resources::fallbackTexture())
    return &Resources::fallbackBlack(); //format error
  return r;
  }

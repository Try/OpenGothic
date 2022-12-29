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

Sky::Sky(const SceneGlobals& scene, const World& world, const std::pair<Tempest::Vec3, Tempest::Vec3>& bbox)
  :scene(scene) {
  auto wname  = world.name();
  auto dot    = wname.rfind('.');
  auto name   = dot==std::string::npos ? wname : wname.substr(0,dot);
  for(size_t i=0;i<2;++i) {
    day  .lay[i].texture = skyTexture(name,true, i);
    night.lay[i].texture = skyTexture(name,false,i);
    }
  minZ = bbox.first.z;
  GSunIntensity = 20.f;

  /*
    zSunName=unsun5.tga
    zSunSize=200
    zSunAlpha=230
    zMoonName=moon.tga
    zMoonSize=400
    zMoonAlpha=255
    */
  sunImg       = Resources::loadTexture(Gothic::settingsGetS("SKY_OUTDOOR","zSunName"));
  // auto& moon   = gothic.settingsGetS("SKY_OUTDOOR","zMoonName");

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
  bool fog = Gothic::inst().settingsGetI("RENDERER_D3D","zFogRadial")!=0;
  if(fog==zFogRadial)
    return;

  zFogRadial = fog;
  if(!zFogRadial)
    return;

  auto& device = Resources::device();
  device.waitIdle();

  lutIsInitialized = false;

  /* https://bartwronski.files.wordpress.com/2014/08/bwronski_volumetric_fog_siggraph2014.pdf
   * page 25 160*90*64 = ~720p
   */
  //fogLut3D = device.image3d(lutFormat,160,90,64);
  //fogLut3D = device.image3d(lutFormat,160,90,128);
  //fogLut3D = device.image3d(lutFormat,160,90,256);
  //fogLut3D = device.image3d(lutFormat,160,90,512);

  //fogLut3D = device.image3d(lutFormat,320,176,32);
  fogLut3D    = device.image3d(lutRGBAFormat,320,176,64);

  //shadowDw = device.image2d(TextureFormat::R32F,320, 32*16);
  //shadowDw = device.image2d(TextureFormat::R16, 256, 256);
  shadowDw = device.image2d(TextureFormat::R16, 512, 256);
  setupUbo();
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

  const auto ambientDay   = Vec3(0.25f,0.25f,0.25f);
  const auto ambientNight = Vec3(0.16f,0.16f,0.50f);

  const auto directDay    = Vec3(0.75f,0.75f,0.75f);
  const auto directNight  = Vec3(0,0,0);

  float k = float(now)/float(midnight);
  float a  = std::max(0.f,std::min(pulse*3.f,1.f));

  auto clr = directNight *(1.f-a) + directDay *a;
  ambient  = ambientNight*(1.f-a) + ambientDay*a;

  float ax  = 360-360*std::fmod(k+0.25f,1.f);
  ax = ax*float(M_PI/180.0);

  sun.setDir(-std::sin(ax)*shadowLength, pulse, std::cos(ax)*shadowLength);
  sun.setColor(clr);
  }

void Sky::setupUbo() {
  auto& device = Resources::device();
  auto  smp    = Sampler::trillinear();
  auto  smpB   = Sampler::bilinear();
  smpB.setClamping(ClampMode::ClampToEdge);

  uboClouds = device.descriptors(Shaders::inst().cloudsLut);
  uboClouds.set(0, cloudsLut);
  uboClouds.set(5,*day  .lay[0].texture,smp);
  uboClouds.set(6,*day  .lay[1].texture,smp);
  uboClouds.set(7,*night.lay[0].texture,smp);
  uboClouds.set(8,*night.lay[1].texture,smp);

  uboTransmittance = device.descriptors(Shaders::inst().skyTransmittance);
  uboTransmittance.set(5,*day  .lay[0].texture,smp);
  uboTransmittance.set(6,*day  .lay[1].texture,smp);
  uboTransmittance.set(7,*night.lay[0].texture,smp);
  uboTransmittance.set(8,*night.lay[1].texture,smp);

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

  if(zFogRadial) {
    uboShadowDw = device.descriptors(Shaders::inst().shadowDownsample);
    uboShadowDw.set(0, shadowDw);
    uboShadowDw.set(1, *scene.shadowMap[1],Resources::shadowSampler());

    for(uint32_t i=0; i<Resources::MaxFramesInFlight; ++i) {
      uboFogViewLut3d[i] = device.descriptors(Shaders::inst().fogViewLut3D);
      uboFogViewLut3d[i].set(0, transLut,     smpB);
      uboFogViewLut3d[i].set(1, multiScatLut, smpB);
      uboFogViewLut3d[i].set(2, cloudsLut,    smpB);

      uboFogViewLut3d[i].set(3, shadowDw, Resources::shadowSampler());
      uboFogViewLut3d[i].set(4, fogLut3D);
      uboFogViewLut3d[i].set(5, scene.uboGlobalPf[i][SceneGlobals::V_Main]);
      }
    }

  uboSky = device.descriptors(Shaders::inst().sky);
  uboSky.set(0, transLut,       smpB);
  uboSky.set(1, multiScatLut,   smpB);
  uboSky.set(2, viewLut,        smpB);
  //uboSky.set(3, fogLut3D,       smpB);
  uboSky.set(4,*day  .lay[0].texture,smp);
  uboSky.set(5,*day  .lay[1].texture,smp);
  uboSky.set(6,*night.lay[0].texture,smp);
  uboSky.set(7,*night.lay[1].texture,smp);

  uboFog = device.descriptors(Shaders::inst().fog);
  uboFog.set(0, transLut,       smpB);
  uboFog.set(1, multiScatLut,   smpB);
  uboFog.set(2, fogLut,         smpB);
  //uboFog.set(3, fogLut3D,       smpB);
  uboFog.set(4, *scene.zbuffer, Sampler::nearest()); // NOTE: wanna here depthFetch from gles2

  if(zFogRadial) {
    auto smpLut3d = Sampler::bilinear();
    smpLut3d.setClamping(ClampMode::ClampToEdge);

    uboSky3d = device.descriptors(Shaders::inst().sky3d);
    uboSky3d.set(0, transLut,       smpB);
    uboSky3d.set(1, multiScatLut,   smpB);
    uboSky3d.set(2, viewLut,        smpB);
    uboSky3d.set(3, fogLut3D,       smpLut3d);
    uboSky3d.set(4,*day  .lay[0].texture,smp);
    uboSky3d.set(5,*day  .lay[1].texture,smp);
    uboSky3d.set(6,*night.lay[0].texture,smp);
    uboSky3d.set(7,*night.lay[1].texture,smp);

    uboFog3d = device.descriptors(Shaders::inst().fog3d);
    uboFog3d.set(0, transLut,       smpB);
    uboFog3d.set(1, multiScatLut,   smpB);
    uboFog3d.set(2, fogLut,         smpB);
    uboFog3d.set(3, fogLut3D,       smpB);
    uboFog3d.set(4, *scene.zbuffer, Sampler::nearest());
    }
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

    lutIsInitialized = true;
    }

  cmd.setFramebuffer({{viewLut, Tempest::Discard, Tempest::Preserve}});
  cmd.setUniforms(Shaders::inst().skyViewLut, uboSkyViewLut, &ubo, sizeof(ubo));
  cmd.draw(Resources::fsqVbo());
  }

void Sky::prepareFog(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint32_t frameId) {
  UboSky ubo = mkPush();

  if(zFogRadial) {
    cmd.setFramebuffer({});
    cmd.setUniforms(Shaders::inst().shadowDownsample, uboShadowDw);
    cmd.dispatchThreads(uint32_t(shadowDw.w()),uint32_t(shadowDw.h()));

    cmd.setUniforms(Shaders::inst().fogViewLut3D,    uboFogViewLut3d[frameId], &ubo, sizeof(ubo));
    cmd.dispatchThreads(uint32_t(fogLut3D.w()),uint32_t(fogLut3D.h()));
    } else {
    cmd.setFramebuffer({{fogLut, Tempest::Discard, Tempest::Preserve}});
    cmd.setUniforms(Shaders::inst().fogViewLut, uboFogViewLut, &ubo, sizeof(ubo));
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

  auto len2 = (ubo.clipInfo.x / (ubo.clipInfo.y * 1.0 + ubo.clipInfo.z));
  (void)len2;

  if(zFogRadial)
    cmd.setUniforms(Shaders::inst().sky3d, uboSky3d, &ubo, sizeof(ubo)); else
    cmd.setUniforms(Shaders::inst().sky,   uboSky,   &ubo, sizeof(ubo));
  cmd.draw(Resources::fsqVbo());
  }

void Sky::drawFog(Tempest::Encoder<CommandBuffer>& cmd, uint32_t fId) {
  UboSky ubo = mkPush();
  if(zFogRadial)
    cmd.setUniforms(Shaders::inst().fog3d, uboFog3d, &ubo, sizeof(ubo));  else
    cmd.setUniforms(Shaders::inst().fog,   uboFog,   &ubo, sizeof(ubo));
  cmd.draw(Resources::fsqVbo());
  }

const Texture2d& Sky::skyLut() const {
  return textureCast(viewLut);
  }

const Texture2d& Sky::shadowLq() const {
  return textureCast(shadowDw);
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
  ubo.sunDir  = sun.dir();
  ubo.night   = 1.f-std::min(std::max(3.f*ubo.sunDir.y,0.f),1.f);

  static float rayleighScatteringScale = 33.1f;
  static float GSunIntensity           = 20.f;

  ubo.rayleighScatteringScale = rayleighScatteringScale;
  ubo.clipInfo                = scene.clipInfo();
  ubo.GSunIntensity           = GSunIntensity;

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

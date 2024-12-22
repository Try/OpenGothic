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

/* https://physicallybased.info
 * The changing color of the Sun over the course of the day is mainly a result of the scattering of sunlight
 * and is not due to changes in black-body radiation. The solar illuminance constant is equal to 128 000 lux,
 * but atmospheric extinction brings the number of lux down to around 100 000 lux.
 *
 * clear sky    = 6000 lx
 * sky overcast = 2000 lx
 *
 * opaque       = (100'000/pi)*NdotL = 31'800*NdotL
 *
 */
static const float DirectSunLux  = 128'000.f;
static const float DirectMoonLux = 1.f;

// static const float NightLight    = 0.36f;
// static const float ShadowSunLux  = 10'000.f;
// static const float StreetLight   = 10.f;

static float smoothstep(float edge0, float edge1, float x) {
  float t = std::min(std::max((x - edge0) / (edge1 - edge0), 0.f), 1.f);
  return t * t * (3.f - 2.f * t);
  };

static float linearstep(float edge0, float edge1, float x) {
  float t = std::min(std::max((x - edge0) / (edge1 - edge0), 0.f), 1.f);
  return t;
  };

Sky::Sky(const SceneGlobals& scene, const World& world)
  :scene(scene) {
  auto wname  = world.name();
  auto dot    = wname.rfind('.');
  auto name   = dot==std::string::npos ? wname : wname.substr(0,dot);
  for(size_t i=0; i<2; ++i) {
    clouds[0].lay[i] = skyTexture(name,true, i);
    clouds[1].lay[i] = skyTexture(name,false,i);
    }

  GSunIntensity  = DirectSunLux;
  GMoonIntensity = DirectMoonLux*4;

  /*
    zSunName=unsun5.tga
    zSunSize=200
    zSunAlpha=230
    zMoonName=moon.tga
    zMoonSize=400
    zMoonAlpha=255
    */
  sunImg       = Resources::loadTexture(Gothic::settingsGetS("SKY_OUTDOOR","zSunName"));
  moonImg      = Resources::loadTexture(Gothic::settingsGetS("SKY_OUTDOOR","zMoonName"));
  if(sunImg==nullptr)
    sunImg = &Resources::fallbackBlack();
  if(moonImg==nullptr)
    moonImg = &Resources::fallbackBlack();
  }

Sky::~Sky() {
  }

float Sky::isNight() const {
  return 1.f - linearstep(-0.18f, 0.f, sun.dir().y);
  }

void Sky::updateLight(const int64_t now) {
  // https://www.suncalc.org/#/52.4561,13.4033,5/2020.06.28/13:09/1/3
  const int64_t rise         = gtime( 4,45).toInt();
  const int64_t meridian     = gtime(13, 9).toInt();
  const int64_t set          = gtime(21,33).toInt();
  const int64_t midnight     = gtime(1,0,0).toInt();
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

  /**
   * central EU, mid day, sun at 60deg:
   * sun = 70'000 lux
   * sky = 20'000 lux
   * all = 90'000 lux
   * expected DirectSunLux = all/(cos(60)*sunClr) = 185'000
   * */
  //pulse = 0.5;

  {
    float k   = float(now)/float(midnight);
    float ax  = 360-360*std::fmod(k+0.25f,1.f);
    ax = ax*float(M_PI/180.0);
    sun.setDir(-std::sin(ax)*shadowLength, pulse, std::cos(ax)*shadowLength);
    //sun.setDir(0, 1, 0); //debug
  }

  static float sunMul = 1;
  static float ambMul = 1;
  // static auto  groundAlbedo = Vec3(0.34f, 0.42f, 0.26f); // Foliage(MacBeth)
  // static auto  groundAlbedo = Vec3(0.39f, 0.40f, 0.33f);
  static auto  groundAlbedo = Vec3(0.3f); // aligned to sky-shading

  // const float dirY = sun.dir().y;
  // float dayTint = std::max(dirY+0.01f, 0.f);

  // const  float aDirect    = linearstep(-0.0f, 0.8f, dirY);
  const  float sunOcclude = smoothstep(0.0f, 0.01f, sun.dir().y);

  Vec3 direct;
  direct  = DirectSunLux * Vec3(1.0f);

  ambient  = DirectSunLux  * float(1.0/M_PI) * groundAlbedo * sunOcclude;
  ambient += DirectMoonLux * float(1.0/M_PI) * groundAlbedo;
  // ambient *= 0.78f; // atmosphere transmission is in shader
  ambient *= 0.68f;           // NdoL prediction
  ambient *= 0.5;             // maybe in shadow or maybe not
  ambient *= 2.0*float(M_PI); // 2*pi (hermisphere integral)
  // ambient *= 0.8f;            // tuneup
  ambient *= float(1.0/M_PI); // predivide by lambertian lobe

  //ambient += Vec3(0.001f);     // should avoid zeros

  sun.setColor(direct*sunMul);
  ambient = ambient*ambMul;
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

  auto r = Resources::loadTexture(tex, true);
  if(r==&Resources::fallbackTexture())
    return &Resources::fallbackBlack(); //format error
  return r;
  }

#include "sky.h"

#include <Tempest/Application>
#include <Tempest/CommandBuffer>

#include "graphics/rendererstorage.h"
#include "world/world.h"
#include "resources.h"

using namespace Tempest;

std::array<float,3> Sky::color = {{0.47f,0.55f,0.70f}};

Sky::Sky(const RendererStorage &storage)
  :storage(storage),uboGpu(storage.device) {
  for(uint32_t i=0;i<storage.device.maxFramesInFlight();++i)
    uboGpu.desc(i) = storage.device.uniforms(storage.uboSkyLayout());
  }

void Sky::setWorld(const World &world) {
  this->world = &world;

  auto& wname = world.name();
  auto dot    = wname.rfind('.');
  auto name   = dot==std::string::npos ? wname : wname.substr(0,dot);

  //day.colorA = mkColor(255,250,235);
  //day.colorB = mkColor(255,255,255);
  day.fog    = color;
  day.day    = true;
  for(size_t i=0;i<2;++i)
    day.lay[i].texture = skyTexture(name.c_str(),day.day,i);
  }

Sky::State Sky::interpolateState(float /*time*/) {
  return day;
  }

void Sky::setMatrix(uint32_t frameId, const Tempest::Matrix4x4 &mat) {
  uboCpu.mvp = mat;
  uboCpu.mvp.inverse();

  auto ticks = world==nullptr ? Application::tickCount() : world->tickCount();
  auto t0 = (ticks%90000)/90000.f;
  auto t1 = (ticks%270000)/270000.f;
  uboCpu.dxy0[0] = t0;
  uboCpu.dxy1[0] = t1;

  uboCpu.color[0]=Sky::color[0];
  uboCpu.color[1]=Sky::color[1];
  uboCpu.color[2]=Sky::color[2];
  uboCpu.color[3]=1.f;

  uboGpu.update(uboCpu,frameId);
  }

void Sky::commitUbo(uint32_t frameId) {
  State st = interpolateState(0.f);

  uboGpu.desc(frameId).set(0,uboGpu[frameId]);
  uboGpu.desc(frameId).set(1,*st.lay[0].texture);
  uboGpu.desc(frameId).set(2,*st.lay[1].texture);
  }

void Sky::draw(Tempest::CommandBuffer &cmd, uint32_t frameId, const World&) {
  size_t offset=0;
  cmd.setUniforms(storage.pSky,uboGpu.desc(frameId),1,&offset);
  cmd.draw(Resources::fsqVbo());
  }

void Sky::setLight(const std::array<float,3> &l) {
  uboCpu.sky[0] = -l[2];
  uboCpu.sky[1] = -l[1];
  uboCpu.sky[2] =  l[0];
  }

std::array<float,3> Sky::mkColor(uint8_t r, uint8_t g, uint8_t b) {
  return {{r/255.f,g/255.f,b/255.f}};
  }

const Texture2d *Sky::skyTexture(const char *name,bool day,size_t id) {
  if(auto t = implSkyTexture(name,day,id))
    return t;
  if(auto t = implSkyTexture(nullptr,day,id))
    return t;
  return &Resources::fallbackTexture();
  }

const Texture2d *Sky::implSkyTexture(const char *name,bool day,size_t id) {
  char tex[256]={};
  if(name && name[0]!='\0'){
    const char* format=day ? "SKYDAY_%s_LAYER%d_A0.TGA" : "SKYNIGHT_%s_LAYER%d_A0.TGA";
    std::snprintf(tex,sizeof(tex),format,name,int(id));
    } else {
    const char* format=day ? "SKYDAY_LAYER%d_A0.TGA" : "SKYNIGHT_LAYER%d_A0.TGA";
    std::snprintf(tex,sizeof(tex),format,int(id));
    }
  for(size_t r=0;tex[r];++r)
    tex[r]=char(std::toupper(tex[r]));
  return Resources::loadTexture(tex);
  }

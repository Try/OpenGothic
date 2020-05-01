#include "sky.h"

#include <Tempest/Application>
#include <Tempest/CommandBuffer>

#include <cctype>

#include "graphics/rendererstorage.h"
#include "world/world.h"
#include "utils/versioninfo.h"
#include "resources.h"

using namespace Tempest;

//std::array<float,3> Sky::color = {{0.47f,0.55f,0.70f}};
std::array<float,3> Sky::color = {{0.2f, 0.5f, 0.66f}};

Sky::Sky(const RendererStorage &storage)
  :storage(storage),uboGpu(storage.device) {
  uint32_t sz = storage.device.maxFramesInFlight();
  for(uint32_t i=0;i<sz;++i)
    uboGpu.desc(i) = storage.device.uniforms(storage.uboSkyLayout());
  nToUpdate.resize(sz,true);
  }

void Sky::setWorld(const World &world) {
  this->world = &world;

  auto& wname = world.name();
  auto dot    = wname.rfind('.');
  auto name   = dot==std::string::npos ? wname : wname.substr(0,dot);

  for(size_t i=0;i<2;++i) {
    day  .lay[i].texture = skyTexture(name.c_str(),true, i);
    night.lay[i].texture = skyTexture(name.c_str(),false,i);
    }

  if(world.version().game==2) {
    //skymesh = world.getStaticView("SKYDOME_COLORLAYER.MRM");
    }
  }

void Sky::invalidateCmd() {
  for(size_t i=0;i<storage.device.maxFramesInFlight();++i)
    nToUpdate[i] = true;
  }

bool Sky::needToUpdateCommands(uint8_t frameId) const {
  return nToUpdate[frameId];
  }

void Sky::setAsUpdated(uint8_t frameId) {
  nToUpdate[frameId] = false;
  }

void Sky::setMatrix(uint8_t frameId, const Tempest::Matrix4x4 &mat) {
  uboCpu.mvp = mat;
  uboCpu.mvp.inverse();

  auto ticks = world==nullptr ? Application::tickCount() : world->tickCount();
  auto t0 = float(ticks%90000 )/90000.f;
  auto t1 = float(ticks%270000)/270000.f;
  uboCpu.dxy0[0] = t0;
  uboCpu.dxy1[0] = t1;

  uboCpu.color[0]=Sky::color[0];
  uboCpu.color[1]=Sky::color[1];
  uboCpu.color[2]=Sky::color[2];
  uboCpu.color[3]=1.f;

  uboGpu.update(uboCpu,frameId);
  }

void Sky::commitUbo(uint8_t frameId) {
  if(!nToUpdate[frameId])
    return;
  uboGpu.desc(frameId).set(0,uboGpu[frameId]);
  uboGpu.desc(frameId).set(1,*day.lay[0].texture);
  uboGpu.desc(frameId).set(2,*day.lay[1].texture);

  uboGpu.desc(frameId).set(3,*night.lay[0].texture);
  uboGpu.desc(frameId).set(4,*night.lay[1].texture);
  }

void Sky::draw(Tempest::Encoder<Tempest::CommandBuffer> &cmd, uint32_t frameId, const World&) {
  uint32_t offset=0;
  cmd.setUniforms(storage.pSky,uboGpu.desc(frameId),1,&offset);
  cmd.draw(Resources::fsqVbo());
  }

void Sky::setLight(const std::array<float,3> &l) {
  uboCpu.sky[0] = -l[2];
  uboCpu.sky[1] = -l[1];
  uboCpu.sky[2] =  l[0];
  }

void Sky::setDayNight(float dayF) {
  uboCpu.night = 1.f-dayF;
  }

std::array<float,3> Sky::mkColor(uint8_t r, uint8_t g, uint8_t b) {
  return {{r/255.f,g/255.f,b/255.f}};
  }

const Texture2d* Sky::skyTexture(const char *name,bool day,size_t id) {
  if(auto t = implSkyTexture(name,day,id))
    return t;
  if(auto t = implSkyTexture(nullptr,day,id))
    return t;
  return &Resources::fallbackBlack();
  }

const Texture2d* Sky::implSkyTexture(const char *name,bool day,size_t id) {
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
  auto r = Resources::loadTexture(tex);
  if(r==&Resources::fallbackTexture())
    return &Resources::fallbackBlack(); //format error
  return r;
  }

#include "sky.h"

#include <Tempest/Application>
#include <Tempest/CommandBuffer>

#include "graphics/rendererstorage.h"
#include "resources.h"

using namespace Tempest;

std::array<Sky::Vertex,6> Sky::fsq =
 {{
    {-1,-1},{ 1,1},{1,-1},
    {-1,-1},{-1,1},{1, 1}
 }};

std::array<float,3> Sky::color = {{0.47f,0.55f,0.70f}};

Sky::Sky(const RendererStorage &storage)
  :storage(storage),uboGpu(storage.device) {
  vbo = Resources::loadVbo(fsq.data(),fsq.size());
  for(uint32_t i=0;i<storage.device.maxFramesInFlight();++i)
    uboGpu.desc(i) = storage.device.uniforms(storage.uboSkyLayout());
  }

void Sky::setWorld(const std::string &wname) {
  auto dot  = wname.rfind('.');
  auto name = dot==std::string::npos ? wname : wname.substr(0,dot);

  day.colorA = mkColor(255,250,235);
  day.colorB = mkColor(255,255,255);
  day.fog    = color;

  for(size_t i=0;i<2;++i) {
    char tex[256]={};
    const char* format=day.day ? "SKYDAY_%s_LAYER%d_A0.TGA" : "SKYNIGHT_%s_LAYER%d_A0.TGA";
    std::snprintf(tex,sizeof(tex),format,name.c_str(),i);
    for(size_t r=0;tex[r];++r)
      tex[r]=char(std::toupper(tex[r]));
    day.lay[i].texture = Resources::loadTexture(tex);
    }
  }

void Sky::setMatrix(uint32_t frameId, const Tempest::Matrix4x4 &mat) {
  uboCpu.mvp = mat;
  uboCpu.mvp.inverse();

  auto t = (Application::tickCount()%80000)/80000.f;
  uboCpu.dxy[0] = t;

  uboGpu.update(uboCpu,frameId);
  }

void Sky::commitUbo(uint32_t frameId) {
  uboGpu.desc(frameId).set(0,uboGpu[frameId]);
  uboGpu.desc(frameId).set(1,*day.lay[0].texture);
  }

void Sky::draw(Tempest::CommandBuffer &cmd, uint32_t frameId, const World&) {
  size_t offset=0;
  cmd.setUniforms(storage.pSky,uboGpu.desc(frameId),1,&offset);
  cmd.draw(vbo);
  }

std::array<float,3> Sky::mkColor(uint8_t r, uint8_t g, uint8_t b) {
  return {{r/255.f,g/255.f,b/255.f}};
  }

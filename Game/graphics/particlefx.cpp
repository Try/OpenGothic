#include "particlefx.h"

#include "resources.h"

using namespace Tempest;

ParticleFx::ParticleFx(const Daedalus::GEngineClasses::C_ParticleFX &src)
  :src(&src) {
  view = Resources::loadTexture(src.visName_S);
  if(view->w()>0){
    frameCount = std::max(1,view->h()/view->w());
    }

  colorS     = loadVec3(src.visTexColorStart_S);
  colorE     = loadVec3(src.visTexColorEnd_S);

  emitPerSec = int32_t(src.ppsValue);
  }

Vec3 ParticleFx::loadVec3(const std::string &src) {
  if(src=="=")
    return Vec3();

  float       v[3] = {};
  const char* str  = src.c_str();
  for(int i=0;i<3;++i) {
    char* next=nullptr;
    v[i] = std::strtof(str,&next);
    str  = next;
    }
  return Vec3(v[0],v[1],v[2]);
  }

#include "light.h"

#include <cmath>
#include <cstring>

using namespace Tempest;

static Vec3 toVec3(uint32_t v) {
  uint8_t cl[4];
  std::memcpy(cl,&v,4);
  return Vec3(cl[0]/255.f,cl[1]/255.f,cl[2]/255.f);
  }

Light::Light() {
  }

void Light::setDir(const Tempest::Vec3& d) {
  float l = d.manhattanLength();
  if(l>0){
    ldir = d/l;
    } else {
    ldir = Vec3();
    }
  }

void Light::setDir(float x, float y, float z) {
  setDir({x,y,z});
  }

void Light::setColor(const Vec3& cl) {
  clr                = cl;
  colorAniListFpsInv = 0;
  }

void Light::setColor(uint32_t cl) {
  setColor(toVec3(cl));
  }

void Light::setColor(const std::vector<uint32_t>& arr, float fps, bool smooth) {
  colorSmooth = smooth;
  if(arr.size()==1) {
    setColor(toVec3(arr[0]));
    colorAniListFpsInv = 0;
    return;
    }

  colorAniList.resize(arr.size());
  for(size_t i=0; i<arr.size(); ++i)
    colorAniList[i] = toVec3(arr[i]);
  colorAniListFpsInv = arr.size()>0 ? uint64_t(1000.0/fps) : 0;
  }

void Light::setRange(float r) {
  rgn = r;
  rangeAniFPSInv = 0;
  }

void Light::setRange(const std::vector<float>& arr, float base, float fps, bool smooth) {
  rangeSmooth = smooth;
  if(arr.size()==1) {
    setRange(arr[0]*base);
    return;
    }
  rangeAniScale  = arr;
  for(auto& i:rangeAniScale)
    i*=base;

  rangeAniFPSInv = arr.size() > 0 ? uint64_t(1000.0/fps) : 0;
  if(rangeAniScale.size()>0) {
    rgn = rangeAniScale[0];
    for(auto i:rangeAniScale)
      rgn = std::max(rgn,i);
    }
  }

void Light::update(uint64_t time) {
  if(rangeAniFPSInv==0) {
    curRgn = rgn;
    } else {
    size_t   frame = size_t(time/rangeAniFPSInv), mod = size_t(time%rangeAniFPSInv);
    float    a     = float(mod)/float(rangeAniFPSInv);
    if(!rangeSmooth)
      a = 0;

    float    r0    = rangeAniScale[(frame  )%rangeAniScale.size()];
    float    r1    = rangeAniScale[(frame+1)%rangeAniScale.size()];
    curRgn = r0+(r1-r0)*a;
    }

  if(colorAniListFpsInv==0) {
    curClr = clr;
    } else {
    size_t   frame = size_t(time/colorAniListFpsInv), mod = size_t(time%colorAniListFpsInv);
    float    a     = float(mod)/float(colorAniListFpsInv);
    if(!colorSmooth)
      a = 0;

    Vec3 cl0 = colorAniList[(frame  )%colorAniList.size()];
    Vec3 cl1 = colorAniList[(frame+1)%colorAniList.size()];
    curClr = cl0+(cl1-cl0)*a;
    }
  }

bool Light::isDynamic() const {
  return rangeAniFPSInv!=0 || colorAniListFpsInv!=0;
  }

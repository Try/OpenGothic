#include "light.h"

#include <cmath>

using namespace Tempest;

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

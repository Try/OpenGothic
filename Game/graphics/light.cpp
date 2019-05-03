#include "light.h"

#include <cmath>

Light::Light() {
  }

void Light::setDir(const std::array<float,3>& d) {
  float l = std::sqrt(d[0]*d[0]+d[1]*d[1]+d[2]*d[2]);
  if(l>0){
    ldir[0] = d[0]/l;
    ldir[1] = d[1]/l;
    ldir[2] = d[2]/l;
    } else {
    ldir[0] = 0;
    ldir[1] = 0;
    ldir[2] = 0;
    }
  }

void Light::setDir(float x, float y, float z) {
  setDir({x,y,z});
  }

#include "animmath.h"

#include <cmath>

static float mix(float x,float y,float a){
  return x+(y-x)*a;
  }

phoenix::animation_sample mix(const phoenix::animation_sample& x,const phoenix::animation_sample& y,float a) {
  phoenix::animation_sample r {};

  r.rotation   = glm::slerp(x.rotation,y.rotation,a);

  r.position.x = mix(x.position.x,y.position.x,a);
  r.position.y = mix(x.position.y,y.position.y,a);
  r.position.z = mix(x.position.z,y.position.z,a);

  return r;
}

static Tempest::Matrix4x4 mkMatrix(float x,float y,float z,float w,
                                   float px,float py,float pz) {
  float m[4][4]={};

  m[0][0] = w * w + x * x - y * y - z * z;
  m[0][1] = 2.0f * (x * y - w * z);
  m[0][2] = 2.0f * (x * z + w * y);
  m[1][0] = 2.0f * (x * y + w * z);
  m[1][1] = w * w - x * x + y * y - z * z;
  m[1][2] = 2.0f * (y * z - w * x);
  m[2][0] = 2.0f * (x * z - w * y);
  m[2][1] = 2.0f * (y * z + w * x);
  m[2][2] = w * w - x * x - y * y + z * z;
  m[3][0] = px;
  m[3][1] = py;
  m[3][2] = pz;
  m[3][3] = 1;

  return Tempest::Matrix4x4(reinterpret_cast<float*>(m));
  }

Tempest::Matrix4x4 mkMatrix(const phoenix::animation_sample& s) {
  return mkMatrix(s.rotation.x,s.rotation.y,s.rotation.z,s.rotation.w,
                  s.position.x,s.position.y,s.position.z);
  }

#include "animmath.h"

#include <cmath>

static float mix(float x,float y,float a){
  return x+(y-x)*a;
  }

zenkit::Quat slerp(const zenkit::Quat& x, const zenkit::Quat& y, float a) {
  zenkit::Quat z = y;

  float cosTheta = x.x*y.x +x.y*y.y + x.z*y.z + x.w*y.w;
  // If cosTheta < 0, the interpolation will take the long way around the sphere.
  // To fix this, one quat must be negated.
  if(cosTheta < 0.f) {
    z.x = -y.x;
    z.y = -y.y;
    z.z = -y.z;
    z.w = -y.w;
    cosTheta = -cosTheta;
    }

  // Perform a linear interpolation when cosTheta is close to 1 to avoid side effect of sin(angle) becoming a zero denominator
  if(cosTheta > 1.f - std::numeric_limits<float>::epsilon()) {
    // Linear interpolation
    return zenkit::Quat(mix(x.w, z.w, a),
                        mix(x.x, z.x, a),
                        mix(x.y, z.y, a),
                        mix(x.z, z.z, a));
    }

  // Essential Mathematics, page 467
  float angle = std::acos(cosTheta);
  float kx  = std::sin((1.f - a) * angle);
  float kz  = std::sin(a * angle);
  float div = std::sin(angle);
  return zenkit::Quat((x.w*kx + z.w*kz)/div,
                      (x.x*kx + z.x*kz)/div,
                      (x.y*kx + z.y*kz)/div,
                      (x.z*kx + z.z*kz)/div);
  // return (sin((1.f - a) * angle) * x + sin(a * angle) * z) / sin(angle);
  }

zenkit::AnimationSample mix(const zenkit::AnimationSample& x, const zenkit::AnimationSample& y, float a) {
  zenkit::AnimationSample r {};

  r.rotation   = slerp(x.rotation,y.rotation,a);
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

Tempest::Matrix4x4 mkMatrix(const zenkit::AnimationSample& s) {
  return mkMatrix(s.rotation.x,s.rotation.y,s.rotation.z,s.rotation.w,
                  s.position.x,s.position.y,s.position.z);
  }

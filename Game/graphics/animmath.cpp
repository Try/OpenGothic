#include "animmath.h"

#include <cmath>

static float mix(float x,float y,float a){
  return x+(y-x)*a;
  }

static float dotV4(const ZMath::float4 &q1, const ZMath::float4 &q2)  {
  return q1.x*q2.x + q1.y*q2.y + q1.z*q2.z + q1.w*q2.w;
  }

static ZMath::float4 slerp(const ZMath::float4 &q1, const ZMath::float4 &q2, float t)  {
  ZMath::float4 q3;
  float dot = dotV4(q1, q2);

  /*	dot = cos(theta)
    if (dot < 0), q1 and q2 are more than 90 degrees apart,
    so we can invert one to reduce spinning	*/
  if( dot<0 ) {
    dot = -dot;
    q3.x = -q2.x;
    q3.y = -q2.y;
    q3.z = -q2.z;
    q3.w = -q2.w;
    } else {
    q3 = q2;
    }

  if( dot<0.95f) {
    float angle = std::acos(dot);
    float s     = std::sin(angle*(1.f-t));
    float st    = std::sin(angle*t);
    float sa    = std::sin(angle);

    q3.x = (q1.x*s + q3.x*st)/sa;
    q3.y = (q1.y*s + q3.y*st)/sa;
    q3.z = (q1.z*s + q3.z*st)/sa;
    q3.w = (q1.w*s + q3.w*st)/sa;

    return q3; //(q1*sinf(angle*(1-t)) + q3*sinf(angle*t))/sinf(angle);
    } else {
    // if the angle is small, use linear interpolation
    //return lerp(q1,q3,t);
    float t1=1.f-t;
    q3.x = q1.x*t1 + q3.x*t;
    q3.y = q1.y*t1 + q3.y*t;
    q3.z = q1.z*t1 + q3.z*t;
    q3.w = q1.w*t1 + q3.w*t;

    const float l = std::sqrt(q3.x*q3.x + q3.y*q3.y + q3.z*q3.z + q3.w*q3.w);
    q3.x /= l;
    q3.y /= l;
    q3.z /= l;
    q3.w /= l;

    return q3;
    }
  }

ZenLoad::zCModelAniSample mix(const ZenLoad::zCModelAniSample& x,const ZenLoad::zCModelAniSample& y,float a) {
  ZenLoad::zCModelAniSample r;

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

Tempest::Matrix4x4 mkMatrix(const ZenLoad::zCModelAniSample& s) {
  return mkMatrix(s.rotation.x,s.rotation.y,s.rotation.z,s.rotation.w,
                  s.position.x,s.position.y,s.position.z);
  }

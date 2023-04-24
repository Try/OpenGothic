#include "frustrum.h"

using namespace Tempest;

void Frustrum::make(const Matrix4x4& m, int32_t w, int32_t h) {
  width  = uint32_t(w);
  height = uint32_t(h);
  mat    = m;

  float clip[16] = {}, t=0;
  std::copy(m.data(), m.data()+16, clip );

  f[0][0] = clip[ 3] - clip[ 0];
  f[0][1] = clip[ 7] - clip[ 4];
  f[0][2] = clip[11] - clip[ 8];
  f[0][3] = clip[15] - clip[12];

  t = std::sqrt(f[0][0]*f[0][0] + f[0][1]*f[0][1] + f[0][2]*f[0][2]);
  if(t==0)
    return clear();

  f[0][0] /= t;
  f[0][1] /= t;
  f[0][2] /= t;
  f[0][3] /= t;

  f[1][0] = clip[ 3] + clip[ 0];
  f[1][1] = clip[ 7] + clip[ 4];
  f[1][2] = clip[11] + clip[ 8];
  f[1][3] = clip[15] + clip[12];

  t = std::sqrt(f[1][0]*f[1][0] + f[1][1]*f[1][1] + f[1][2]*f[1][2]);
  if(t==0)
    return clear();

  f[1][0] /= t;
  f[1][1] /= t;
  f[1][2] /= t;
  f[1][3] /= t;

  f[2][0] = clip[ 3] + clip[ 1];
  f[2][1] = clip[ 7] + clip[ 5];
  f[2][2] = clip[11] + clip[ 9];
  f[2][3] = clip[15] + clip[13];

  t = std::sqrt(f[2][0]*f[2][0] + f[2][1]*f[2][1] + f[2][2]*f[2][2]);
  if(t==0)
    return clear();

  f[2][0] /= t;
  f[2][1] /= t;
  f[2][2] /= t;
  f[2][3] /= t;

  f[3][0] = clip[ 3] - clip[ 1];
  f[3][1] = clip[ 7] - clip[ 5];
  f[3][2] = clip[11] - clip[ 9];
  f[3][3] = clip[15] - clip[13];

  t = std::sqrt(f[3][0]*f[3][0] + f[3][1]*f[3][1] + f[3][2]*f[3][2]);
  if(t==0)
    return clear();

  f[3][0] /= t;
  f[3][1] /= t;
  f[3][2] /= t;
  f[3][3] /= t;

  f[4][0] = clip[ 3] - clip[ 2];
  f[4][1] = clip[ 7] - clip[ 6];
  f[4][2] = clip[11] - clip[10];
  f[4][3] = clip[15] - clip[14];

  t = std::sqrt(f[4][0]*f[4][0] + f[4][1]*f[4][1] + f[4][2]*f[4][2]);
  if(t==0)
    return clear();

  f[4][0] /= t;
  f[4][1] /= t;
  f[4][2] /= t;
  f[4][3] /= t;

  f[5][0] = clip[ 3] + clip[ 2];
  f[5][1] = clip[ 7] + clip[ 6];
  f[5][2] = clip[11] + clip[10];
  f[5][3] = clip[15] + clip[14];

  t = std::sqrt(f[5][0]*f[5][0] + f[5][1]*f[5][1] + f[5][2]*f[5][2]);
  if(t==0)
    return clear();

  f[5][0] /= t;
  f[5][1] /= t;
  f[5][2] /= t;
  f[5][3] /= t;
  }

void Frustrum::clear() {
  mat = Matrix4x4();
  std::memset(f,0,sizeof(f));
  f[0][3] = -std::numeric_limits<float>::infinity();
  }

bool Frustrum::testPoint(float x, float y, float z) const {
  return testPoint(x,y,z,0);
  }

bool Frustrum::testPoint(float x, float y, float z, float R) const {
  // if(std::isnan(R))
  //   return false;

  for(size_t i=0; i<6; i++) {
    if(f[i][0]*x+f[i][1]*y+f[i][2]*z+f[i][3]<=-R)
      return false;
    }
  return true;
  }

bool Frustrum::testPoint(const Tempest::Vec3& p, float R) const {
  return testPoint(p.x,p.y,p.z,R);
  }

bool Frustrum::testPoint(const Vec3& p, float R, float& dist) const {
  for(size_t i=0; i<5; i++) {
    if(f[i][0]*p.x+f[i][1]*p.y+f[i][2]*p.z+f[i][3]<=-R)
      return false;
    }

  dist = f[5][0]*p.x+f[5][1]*p.y+f[5][2]*p.z+f[5][3];
  if(dist<-R)
    return false;

  return true;
  }

Frustrum::Ret Frustrum::testBbox(const Tempest::Vec3& min, const Tempest::Vec3& max) const {
  auto ret = Ret::T_Full;
  for(int i=0; i<6; i++) {
    float dmax =
          std::max(min.x * f[i][0], max.x * f[i][0])
        + std::max(min.y * f[i][1], max.y * f[i][1])
        + std::max(min.z * f[i][2], max.z * f[i][2])
        + f[i][3];
    float dmin =
          std::min(min.x * f[i][0], max.x * f[i][0])
        + std::min(min.y * f[i][1], max.y * f[i][1])
        + std::min(min.z * f[i][2], max.z * f[i][2])
        + f[i][3];
    if(dmax<0)
      return Ret::T_Invisible;
    if(dmin<0)
      ret = Ret::T_Partial;
    }
  return ret;
  }

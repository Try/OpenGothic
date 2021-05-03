#include "frustrum.h"

using namespace Tempest;

void Frustrum::make(const Matrix4x4& m, uint32_t w, uint32_t h) {
  width  = w;
  height = h;
  mat    = m;

  float clip[16], t=0;
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

bool Frustrum::testPoint(const Vec3 p, float R) const {
  for(size_t i=0; i<6; i++) {
    if(f[i][0]*p.x+f[i][1]*p.y+f[i][2]*p.z+f[i][3]<=-R)
      return false;
    }
  return true;
  }

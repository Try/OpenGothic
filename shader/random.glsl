#ifndef RANDOM_GLSL
#define RANDOM_GLSL

#include "common.glsl"

struct Random {
  uint state;
  };

Random srand(uvec2 fragCoord, uint seed) {
  Random r;
  r.state = uint(uint(fragCoord.x) * uint(1973) + uint(fragCoord.y) * uint(9277) + uint(seed) * uint(26699)) | uint(1);
  return r;
  }

float randf(inout Random r) {
  return float(wangHash(r.state)) / 4294967296.0;
  }

vec3 randVec3(inout Random rng) {
  float z = randf(rng) * 2.0 - 1.0;
  float a = randf(rng) * 2.0 * M_PI;
  float r = sqrt(1.0f - z * z);
  float x = r * cos(a);
  float y = r * sin(a);
  return vec3(x, y, z);
  }

vec3 randCosWeightedHemisphereDirection(const vec3 n, inout Random rng) {
  vec2 rv2 = vec2(randf(rng), randf(rng));

  vec3  uu = normalize( cross( n, vec3(0.0,1.0,1.0) ) );
  vec3  vv = normalize( cross( uu, n ) );

  float ra = sqrt(rv2.y);
  float rx = ra*cos(6.2831*rv2.x);
  float ry = ra*sin(6.2831*rv2.x);
  float rz = sqrt( 1.0-rv2.y );
  vec3  rr = vec3( rx*uu + ry*vv + rz*n );

  return normalize(rr);
  }

vec3 randCosWeightedHemisphereDirection(const mat3 tbn, inout Random rng) {
  vec2 rv2 = vec2(randf(rng), randf(rng));

  float ra = sqrt(rv2.y);
  float rx = ra*cos(6.2831*rv2.x);
  float ry = ra*sin(6.2831*rv2.x);
  float rz = sqrt( 1.0-rv2.y );
  vec3  rr = vec3( rx*tbn[0] + ry*tbn[1] + rz*tbn[2] );

  return normalize(rr);
  }

#endif
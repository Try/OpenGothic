#ifndef BVH_COMMON_GLSL
#define BVH_COMMON_GLSL

#if defined(DEBUG)
uint numTri = 0; //debug
uint numBox = 0;
#endif

const float TMax = 1e30f;

const uint BVH_NullNode = 0x00000000;
const uint BVH_BoxNode  = 0x10000000;
const uint BVH_Tri1Node = 0x20000000;
const uint BVH_Tri2Node = 0x30000000;

struct BVHNode {
  vec4 lmin; // unsigned left in w
  vec4 lmax; // unsigned right in w
  vec4 rmin;
  vec4 rmax;
  };

struct Ray {
  vec3 origin;
  vec3 dir;
  vec3 invDir;
  vec3 oriDir;
  };

//NOTE: GL_AMD_shader_trinary_minmax
float bvhMax3(float a, float b, float c) {
  return max(a, max(b, c));
  }

float bvhMin3(float a, float b, float c) {
  return min(a, min(b, c));
  }

vec3 rayTriangleTest(const Ray ray, const vec3 v0, const vec3 e1, const vec3 e2) {
#if defined(DEBUG)
  numTri++;
#endif

  const vec3  s1    = cross(ray.dir, e2);
  const float denom = dot(s1, e1);

  if(denom >= 0.0)
    return vec3(TMax);

  const float invDemom = 1.0 / denom;
  const vec3  d        = ray.origin - v0;
  const vec3  s2       = cross(d, e1);

  const float u = dot(d,       s1) * invDemom;
  const float v = dot(ray.dir, s2) * invDemom;

  if(( u < 0.0f ) || ( u > 1.0f ) || ( v < 0.0f ) || ( u + v > 1.0f )) {
    return vec3(TMax);
    }

  float t0 = dot(e2, s2) * invDemom;
  return vec3(t0, u, v);
  }

float rayBoxTest(const Ray ray, const vec3 boxMin, const vec3 boxMax, float hitT) {
#if defined(DEBUG)
  numBox++;
#endif

  vec3  tMin  = fma(boxMin, ray.invDir, ray.oriDir);
  vec3  tMax  = fma(boxMax, ray.invDir, ray.oriDir);
  vec3  t1    = min(tMin, tMax);
  vec3  t2    = max(tMin, tMax);

#if 1
  float tNear = max(0,    bvhMax3(t1.x, t1.y, t1.z));
  float tFar  = min(hitT, bvhMin3(t2.x, t2.y, t2.z));
#else
  float tNear = bvhMax3(t1.x, t1.y, t1.z);
  float tFar  = bvhMin3(t2.x, t2.y, t2.z);
#endif

  return tNear > tFar ? TMax : tNear;
  }

uint bvhGetNodeType(uint ptr) {
  return ptr & 0xF0000000;
  }

void bvhIntersectTri(const Ray ray, const BVHNode n, const uint type, inout vec3 hit) {
  const vec3 cl = vec3(0);

  const vec3 a  = n.lmin.xyz;
  const vec3 e1 = n.lmax.xyz;
  const vec3 e2 = n.rmin.xyz;
  const vec3 e3 = n.rmax.xyz;

  const vec3 ti0 = rayTriangleTest(ray, a, e1, e2);
  if(ti0.x < hit.x)
    hit = ti0;

  if(type==BVH_Tri1Node)
    return;

  const vec3 ti1 = rayTriangleTest(ray, a, e2, e3);
  if(ti1.x < hit.x)
    hit = ti1;
  }

#endif

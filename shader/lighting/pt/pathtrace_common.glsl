#ifndef PATHTRACE_COMMON_GLSL
#define PATHTRACE_COMMON_GLSL

#include "random.glsl"

const float TMax = 1e30f;

struct HitResolve {
  vec4  diff;
  vec3  norm;
  float rayT;
  bool  water;
  };

float rayPathTextureLOD(float dist, vec3 dir, vec3 norm) {
  dist /= 500.0;
  float mip = 0;
  mip += log2(dist);
  mip -= log2(abs(dot(dir, norm)));
  return mip;
  }

void rayQueryProceedAlphaTest(in rayQueryEXT rayQuery, inout Random rng) {
  while(rayQueryProceedEXT(rayQuery)) {
    const uint type = rayQueryGetIntersectionTypeEXT(rayQuery,false);
    if(type==gl_RayQueryCandidateIntersectionTriangleEXT) {
      const vec4 d = resolveHit(rayQuery);
      // const bool opaqueHit = (d.a>0.5);
      const bool opaqueHit = (d.a > randf(rng));
      if(opaqueHit)
        rayQueryConfirmIntersectionEXT(rayQuery);
      }
    }
  }

float rayQueryProceedShadow(const vec3 rayOrigin, const vec3 rayDirection, inout Random rngState) {
  // CullBack due to vegetation
  uint  flags = gl_RayFlagsSkipAABBEXT | gl_RayFlagsCullBackFacingTrianglesEXT;
  float tMin  = 1;

  rayQueryEXT rayQuery;
  rayQueryInitializeEXT(rayQuery, topLevelAS, flags, CM_ShadowCaster,
                        rayOrigin, tMin, rayDirection, 500*100);
  rayQueryProceedAlphaTest(rayQuery);
  // rayQueryProceedAlphaTest(rayQuery, rngState);
  if(rayQueryGetIntersectionTypeEXT(rayQuery, true) == gl_RayQueryCommittedIntersectionNoneEXT)
    return 1;
  return 0;
  }

HitResolve rayQueryProceedPrimary(const vec3 rayOrigin, const vec3 rayDirection, float mipOverride, inout Random rngState) {
  // CullBack due to vegetation
  uint  flags = gl_RayFlagsSkipAABBEXT | gl_RayFlagsCullBackFacingTrianglesEXT;
  float tMin  = 2;

  rayQueryEXT rayQuery;
  rayQueryInitializeEXT(rayQuery, topLevelAS, flags, 0xFF,
                        rayOrigin, tMin, rayDirection, 500*100);
  rayQueryProceedAlphaTest(rayQuery);
  // rayQueryProceedAlphaTest(rayQuery, rngState);
  if(rayQueryGetIntersectionTypeEXT(rayQuery, true) == gl_RayQueryCommittedIntersectionNoneEXT) {
    HitResolve ret;
    ret.rayT = TMax;
    return ret;
    }

  const HitDesc hit = pullCommitedHitDesc(rayQuery);

  const float rayT   = rayQueryGetIntersectionTEXT(rayQuery, true);
  const bool  face   = !(rayQueryGetIntersectionFrontFaceEXT(rayQuery, true)); //NOTE: not working on vegetation

  const uint  id     = hit.instanceId;
  const uvec3 index  = pullTrinagleIds(id,hit.primitiveId);

  const vec2  uv0    = pullTexcoord(id,index.x);
  const vec2  uv1    = pullTexcoord(id,index.y);
  const vec2  uv2    = pullTexcoord(id,index.z);

  const vec3  nr0    = pullNormal(id,index.x);
  const vec3  nr1    = pullNormal(id,index.y);
  const vec3  nr2    = pullNormal(id,index.z);

  const vec3  b      = hit.baryCoord;
  const vec2  uv     = (b.x*uv0 + b.y*uv1 + b.z*uv2);
  vec3        norm   = (b.x*nr0 + b.y*nr1 + b.z*nr2);

  const mat3x3 matrix = mat3x3(rayQueryGetIntersectionObjectToWorldEXT(rayQuery, true));
  norm = normalize(matrix*norm);

  const float mip    = mipOverride>=0 ? mipOverride : rayPathTextureLOD(rayT,rayDirection,norm);
  const vec4  diff   = textureLod(sampler2D(textures[nonuniformEXT(id)], smp),uv,mip);

  HitResolve ret;
  ret.diff  = diff;
  ret.norm  = norm;
  ret.rayT  = face ? -rayT : rayT;
  ret.water = hit.water;
  return ret;
  }

#endif
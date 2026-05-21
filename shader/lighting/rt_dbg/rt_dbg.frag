#version 460

#define LIGHTING 1
#define RAY_QUERY
#define RAY_QUERY_AT

#include "lighting/rt/rt_common.glsl"
#include "lighting/tonemapping.glsl"
#include "scene.glsl"
#include "common.glsl"

const float probeRayDistance = 200*100; // Lumen rt-probe uses 200-meters range

layout(location = 0) in  vec2 inUV;
layout(location = 0) out vec4 outColor;

layout(binding = 0, std140) uniform UboScene {
  SceneDesc scene;
  };
layout(binding = 2) uniform texture2D irradiance;
layout(binding = 3) uniform sampler2D textureSm1;

vec3 skyIrradiance(vec3 n) {
  ivec3 d;
  d.x = n.x>=0 ? 1 : 0;
  d.y = n.y>=0 ? 1 : 0;
  d.z = n.z>=0 ? 1 : 0;

  n = n*n;

  vec3 ret = vec3(0);
  ret += texelFetch(irradiance, ivec2(0,d.x), 0).rgb * n.x;
  ret += texelFetch(irradiance, ivec2(1,d.y), 0).rgb * n.y;
  ret += texelFetch(irradiance, ivec2(2,d.z), 0).rgb * n.z;

  return ret;
  }

float shadowSample(in sampler2D shadowMap, vec2 shPos) {
  shPos.xy = shPos.xy*vec2(0.5,0.5)+vec2(0.5);
  return textureLod(shadowMap,shPos,0).r;
  }

float calcShadow(vec3 shPos1) {
  //shPos1.y = max(0,shPos1.y);
  float sh1 = shadowSample(textureSm1,shPos1.xy);
  float v1  = (sh1 < shPos1.z) ? 1.0 : 0.0;
  return v1;
  }

float shadowFactor(vec3 pos) {
  vec4 shPos = scene.viewShadow[1]*vec4(pos,1);
  if(abs(shPos.x)>=shPos.w && abs(shPos.y)>=shPos.w)
    return 1;
  return calcShadow(shPos.xyz/shPos.w);
  }

float computeTextureLOD(float dist, vec3 dir, vec3 norm) {
  dist /= 500.0;
  float mip = 0;
  mip += log2(dist);
  mip -= log2(abs(dot(dir, norm)));
  return mip;
  }

vec4 raySampleScene(const vec3 rayOrigin, const vec3 rayDirection) {
  // CullBack due to vegetation
  uint  flags = gl_RayFlagsSkipAABBEXT | gl_RayFlagsCullBackFacingTrianglesEXT;
  float tMin  = 0;

  rayQueryEXT rayQuery;
  rayQueryInitializeEXT(rayQuery, topLevelAS, flags, 0xFF,
                        rayOrigin, tMin, rayDirection, probeRayDistance);
  rayQueryProceedAlphaTest(rayQuery);
  if(rayQueryGetIntersectionTypeEXT(rayQuery, true) == gl_RayQueryCommittedIntersectionNoneEXT) {
    return vec4(-1);
    }

  const HitDesc hit = pullCommitedHitDesc(rayQuery);

  const float rayT   = rayQueryGetIntersectionTEXT(rayQuery, true);
  const bool  face   = !(rayQueryGetIntersectionFrontFaceEXT(rayQuery, true)); //NOTE: not working on vegetation
  if(face)
    return vec4(1,1,0,1);

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

  //const float mip    = log2(rayT / 500.0);
  const float mip    = computeTextureLOD(rayT,rayDirection,norm);
  const vec3  diff   = textureLod(sampler2D(textures[nonuniformEXT(id)], smp),uv,mip).rgb;

#if defined(LIGHTING)
  float lamb   = max(dot(norm, scene.sunDir), 0);
  float shadow = shadowFactor(rayOrigin + rayT*rayDirection);
  float light  = lamb * shadow;

  const vec3 linear      = textureAlbedo(diff.rgb);
  const vec3 ambient     = scene.ambient + (norm.y*0.25+0.75) * NightAmbient * Fd_Lambert;
  const vec3 sky         = skyIrradiance(norm);

  const vec3 illuminance = scene.sunColor*light + ambient;
  const vec3 luminance   = linear * Fd_Lambert * illuminance  + (linear * sky);

  return vec4(luminance * scene.exposure, rayT);
#else
  return vec4(diff, 1.0);
#endif
  }

float computeCellSize(float d, float fov, float Ry, float sp, float smin) {
  float h  = d * tan(fov * 0.5);
  float sw = sp * (h * 2.0) / Ry;

  //From https://history.siggraph.org/wp-content/uploads/2022/08/2020-Talks-Gautron_Real-Time-Ray-Traced-Ambient-Occlusion-of-Complex-Scenes.pdf
  //s_wd = 2^(floor(log2(sw / smin))) * smin
  float exponent = floor(log2(sw / smin));
  float swd      = pow(2.0, exponent) * smin;
  return swd;
  }

void main(void) {
  const mat4 inv = scene.viewProjectInv;
  const vec2 uv  = inUV;
  const vec4 s   = inv*vec4(uv * 2.0 - 1.0, 0.0, 1);
  const vec4 e   = inv*vec4(uv * 2.0 - 1.0, 1.0, 1);

  const vec3 origin = s.xyz/s.w;
  const vec3 dir    = normalize(e.xyz/e.w - origin);

  outColor = raySampleScene(origin, dir);
  }

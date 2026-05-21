#version 460

#define LIGHTING 1
#define RAY_QUERY
#define RAY_QUERY_AT

#define SOFT_SHADOW

#include "lighting/rt/rt_common.glsl"
#include "lighting/tonemapping.glsl"
#include "scene.glsl"
#include "common.glsl"

//const float probeRayDistance = 200*100; // Lumen rt-probe uses 200-meters range
const float TMax         = 1e30f;
const vec3  groundAlbedo = vec3(0.3f); // testing

layout(location = 0) in  vec2 inUV;
layout(location = 0) out vec4 outColor;

layout(std140, push_constant) uniform Push {
  uint frameId;
  };
layout(binding = 0, std140) uniform UboScene {
  SceneDesc scene;
  };
layout(binding = 2) uniform texture2D irradiance;
layout(binding = 3) uniform sampler2D skyLUT;
layout(binding = 4) uniform sampler2D textureSm1;

uint wangHash(inout uint seed) {
  seed = uint(seed ^ uint(61)) ^ uint(seed >> uint(16));
  seed *= uint(9);
  seed = seed ^ (seed >> 4);
  seed *= uint(0x27d4eb2d);
  seed = seed ^ (seed >> 15);
  return seed;
  }

uint srand(uvec2 fragCoord, uint seed) {
  return uint(uint(fragCoord.x) * uint(1973) + uint(fragCoord.y) * uint(9277) + uint(seed) * uint(26699)) | uint(1);
  }

float randf(inout uint state) {
  return float(wangHash(state)) / 4294967296.0;
  }

vec3 randVec3(inout uint state) {
  float z = randf(state) * 2.0 - 1.0;
  float a = randf(state) * 2.0 * M_PI;
  float r = sqrt(1.0f - z * z);
  float x = r * cos(a);
  float y = r * sin(a);
  return vec3(x, y, z);
  }

vec3 randCosWeightedHemisphereDirection(const vec3 n, inout uint rngState) {
  vec2 rv2 = vec2(randf(rngState), randf(rngState));

  vec3  uu = normalize( cross( n, vec3(0.0,1.0,1.0) ) );
  vec3  vv = normalize( cross( uu, n ) );

  float ra = sqrt(rv2.y);
  float rx = ra*cos(6.2831*rv2.x);
  float ry = ra*sin(6.2831*rv2.x);
  float rz = sqrt( 1.0-rv2.y );
  vec3  rr = vec3( rx*uu + ry*vv + rz*n );

  return normalize(rr);
  }

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

void rayQueryProceedAlphaTest(in rayQueryEXT rayQuery, inout uint rngState) {
  while(rayQueryProceedEXT(rayQuery)) {
    const uint type = rayQueryGetIntersectionTypeEXT(rayQuery,false);
    if(type==gl_RayQueryCandidateIntersectionTriangleEXT) {
      const vec4 d = resolveHit(rayQuery);
      // const bool opaqueHit = (d.a>0.5);
      const bool opaqueHit = (d.a > randf(rngState));
      if(opaqueHit)
        rayQueryConfirmIntersectionEXT(rayQuery);
      }
    }
  }

float rayQueryProceedShadow(const vec3 rayOrigin, const vec3 rayDirection, inout uint rngState) {
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

float computeTextureLOD(float dist, vec3 dir, vec3 norm) {
  dist /= 500.0;
  float mip = 0;
  mip += log2(dist);
  mip -= log2(abs(dot(dir, norm)));
  return mip;
  }

struct HitResolve {
  vec4  diff;
  vec3  norm;
  float rayT;
  bool  water;
  };

vec3 randomizeRay(vec3 ray, float angle, inout uint rngState) {
  // https://www.shadertoy.com/view/3sfBWs
  const vec2 blueNoiseInDisk[64] = vec2[64](
      vec2(0.478712,0.875764),
      vec2(-0.337956,-0.793959),
      vec2(-0.955259,-0.028164),
      vec2(0.864527,0.325689),
      vec2(0.209342,-0.395657),
      vec2(-0.106779,0.672585),
      vec2(0.156213,0.235113),
      vec2(-0.413644,-0.082856),
      vec2(-0.415667,0.323909),
      vec2(0.141896,-0.939980),
      vec2(0.954932,-0.182516),
      vec2(-0.766184,0.410799),
      vec2(-0.434912,-0.458845),
      vec2(0.415242,-0.078724),
      vec2(0.728335,-0.491777),
      vec2(-0.058086,-0.066401),
      vec2(0.202990,0.686837),
      vec2(-0.808362,-0.556402),
      vec2(0.507386,-0.640839),
      vec2(-0.723494,-0.229240),
      vec2(0.489740,0.317826),
      vec2(-0.622663,0.765301),
      vec2(-0.010640,0.929347),
      vec2(0.663146,0.647618),
      vec2(-0.096674,-0.413835),
      vec2(0.525945,-0.321063),
      vec2(-0.122533,0.366019),
      vec2(0.195235,-0.687983),
      vec2(-0.563203,0.098748),
      vec2(0.418563,0.561335),
      vec2(-0.378595,0.800367),
      vec2(0.826922,0.001024),
      vec2(-0.085372,-0.766651),
      vec2(-0.921920,0.183673),
      vec2(-0.590008,-0.721799),
      vec2(0.167751,-0.164393),
      vec2(0.032961,-0.562530),
      vec2(0.632900,-0.107059),
      vec2(-0.464080,0.569669),
      vec2(-0.173676,-0.958758),
      vec2(-0.242648,-0.234303),
      vec2(-0.275362,0.157163),
      vec2(0.382295,-0.795131),
      vec2(0.562955,0.115562),
      vec2(0.190586,0.470121),
      vec2(0.770764,-0.297576),
      vec2(0.237281,0.931050),
      vec2(-0.666642,-0.455871),
      vec2(-0.905649,-0.298379),
      vec2(0.339520,0.157829),
      vec2(0.701438,-0.704100),
      vec2(-0.062758,0.160346),
      vec2(-0.220674,0.957141),
      vec2(0.642692,0.432706),
      vec2(-0.773390,-0.015272),
      vec2(-0.671467,0.246880),
      vec2(0.158051,0.062859),
      vec2(0.806009,0.527232),
      vec2(-0.057620,-0.247071),
      vec2(0.333436,-0.516710),
      vec2(-0.550658,-0.315773),
      vec2(-0.652078,0.589846),
      vec2(0.008818,0.530556),
      vec2(-0.210004,0.519896)
      );

  // get a blue noise sample position
  vec2 samplePos = blueNoiseInDisk[wangHash(rngState)%blueNoiseInDisk.length()];
  samplePos *= tan(angle);

  vec3  r  = normalize(vec3(0,0,1) + vec3(samplePos,0));

  vec3  uu = normalize( cross( ray, vec3(0.0,1.0,1.0) ) );
  vec3  vv = normalize( cross( uu, ray ) );

  return vec3( r.x*uu + r.y*vv + r.z*ray );
  }

HitResolve rayQueryProceedPrimary(const vec3 rayOrigin, const vec3 rayDirection, float mipOverride, inout uint rngState) {
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

  const float mip    = mipOverride>=0 ? mipOverride : computeTextureLOD(rayT,rayDirection,norm);
  const vec4  diff   = textureLod(sampler2D(textures[nonuniformEXT(id)], smp),uv,mip);

  HitResolve ret;
  ret.diff  = diff;
  ret.norm  = norm;
  ret.rayT  = face ? -rayT : rayT;
  ret.water = hit.water;
  return ret;
  }

float sampleDirectLight(vec3 norm, vec3 rayOrigin, vec3 rayDirection, bool shadowed, float softAngle, inout uint rngState) {
#if defined(SOFT_SHADOW)
  vec3  shRay  = randomizeRay(rayDirection, 0.5*M_PI/180.0, rngState);
#else
  vec3  shRay  = rayDirection;
#endif

  float lamb   = max(dot(norm, rayDirection), 0);
  if(!shadowed || lamb==0)
    return lamb;

  float shadow = rayQueryProceedShadow(rayOrigin, shRay, rngState);
  // float shadow = shadowFactor(rayOrigin);
  return (lamb * shadow);
  }

vec4 pathtrace(vec3 rayOrigin, vec3 rayDirection) {
  const int numBounces = 5;

  uint  rngState   = srand(uvec2(gl_FragCoord.xy), scene.tickCount32);
  vec3  thruput    = vec3(1);
  vec3  color      = vec3(0);
  bool  underWater = (scene.underWater!=0);
  float depth      = 0;

  for(int bounce=0;; ++bounce) {
    if(dot(thruput*scene.GSunIntensity*scene.exposure, vec3(0.2126, 0.7152, 0.0722)) < 0.01)
      break;

    if(bounce>=numBounces) {
      // return 0.5*vec4(1,0,1,depth)*scene.GSunIntensity;
      // rescale to 100% thruput
      vec3 scale = 1.0-thruput;
      if(scale.r>0.01 && scale.g>0.01 && scale.b>0.01)
        return vec4(color/scale, depth);
      return vec4(color, depth);
      }

    HitResolve hit = rayQueryProceedPrimary(rayOrigin, rayDirection, (bounce==0 ? 0 : -1), rngState);
    if(bounce==0)
      depth = abs(hit.rayT);

    if(hit.rayT<0) {
      hit.rayT = abs(hit.rayT); // backfaces are fine for PT
      hit.norm = -hit.norm;
      }

    if(hit.rayT==TMax) {
      vec3 sky = textureSkyLUT(skyLUT, vec3(0,RPlanet+max(rayOrigin.y*0.01,0),0), rayDirection, scene.sunDir) * scene.GSunIntensity;
      color += thruput*sky;
      color += thruput*(vec3(0.3, 0.26, 1)*0.15); //HACK for the night sky
      break;
      }

    if(underWater) {
      const float depth         = hit.rayT / 5000.0; // 50 meters
      const vec3  transmittance = exp(-depth * vec3(4,2,1) * 1.25);
      thruput *= transmittance;
      }

    if(hit.water) {
      const float ior  = (underWater ? IorAir : IorWater);
      const vec3  refl = reflect(rayDirection, hit.norm);
      const float f    = fresnel(refl,hit.norm,ior);
      const bool  path = (f>randf(rngState));

      rayOrigin    = (rayOrigin + rayDirection * hit.rayT);
      rayDirection = path ? refl : refract(rayDirection, hit.norm, ior);
      underWater   = path ? underWater : !underWater;
      thruput    *= WaterAlbedo;
      continue;
      }

    rayOrigin    = (rayOrigin + rayDirection * hit.rayT);
    rayDirection = randCosWeightedHemisphereDirection(hit.norm, rngState);
    thruput    *= textureAlbedo(hit.diff.rgb);
    //thruput    *= vec3(0.85); //snow
    //thruput    *= vec3(0.04); // asphalt
    //if(bounce>0)
    //  thruput *= groundAlbedo;
    //if(bounce>0)
    //  thruput    *= textureAlbedo(hit.diff.rgb);

    vec3  direct = vec3(0);
    direct += sampleDirectLight(hit.norm, rayOrigin, scene.sunDir, true, 0.54*M_PI/180.0, rngState) * scene.sunColor;
    //direct += sampleDirectLight(hit.norm, rayOrigin, normalize(vec3(-1,1,0)), false, 0.56*M_PI/180.0, rngState) * (vec3(0.3, 0.26, 1)*GMoonIntensity);

    //if(bounce==0)
      color += thruput*direct*Fd_Lambert;

    if(bounce==0)
      ;//color += thruput*(hit.norm.y*0.25+0.75) * NightAmbient;
    }

  return vec4(color, depth);
  }

float nonLinearDepth(float d, vec3 clipInfo) {
  float near = 10.f;
  float far  = 100000.f;
  // return clipInfo[0]/(d*clipInfo[1]) - clipInfo[2]/clipInfo[1];
  if(d <= near)
    return 0.0;
  return (far / (far - near)) * (1.0 - (near / d));
  }

// halton low discrepancy sequence, from https://www.shadertoy.com/view/wdXSW8
vec2 halton(uint index) {
  const vec2 coprimes = vec2(2.0f, 3.0f);
  vec2 s = vec2(index, index);
  vec4 a = vec4(1,1,0,0);
  while(s.x > 0.0 && s.y > 0.0) {
    a.xy  = a.xy/coprimes;
    a.zw += a.xy*mod(s, coprimes);
    s = floor(s/coprimes);
    }
  return a.zw;
  }

void main() {
  const vec2 subpixel = (halton(srand(uvec2(gl_FragCoord.xy),frameId))-0.5) * scene.screenResInv;

  const mat4 inv = scene.viewProjectInv;
  const vec2 uv  = inUV + subpixel;
  const vec4 s   = inv*vec4(uv * 2.0 - 1.0, 0.0, 1);
  const vec4 e   = inv*vec4(uv * 2.0 - 1.0, 1.0, 1);

  const vec3 origin = s.xyz/s.w;
  const vec3 dir    = normalize(e.xyz/e.w - origin);

  const vec4 pt = pathtrace(origin, dir);
  if(pt.a==0)
    discard;

  outColor = vec4(pt.rgb * scene.exposure, 1.0/(frameId+1.0));
  gl_FragDepth = nonLinearDepth(pt.a, scene.clipInfo);
  }

#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#if defined(RAY_QUERY)
#extension GL_EXT_ray_query : enable
#endif

#include "common.glsl"

layout(push_constant, std140) uniform PushConstant {
  mat4 mvp;
  } ubo;

layout(binding  = 0) uniform sampler2D lightingBuf;
layout(binding  = 1) uniform sampler2D diffuse;
layout(binding  = 2) uniform sampler2D normals;
layout(binding  = 3) uniform sampler2D depth;

#if defined(RAY_QUERY)
layout(binding  = 5) uniform accelerationStructureEXT topLevelAS;
#endif


layout(location = 0) in  vec2 uv;
layout(location = 1) in  vec2 inPos;
layout(location = 2) in  mat4 mvpInv;

layout(location = 0) out vec4 outColor;

const uint  numSamples = 16*4;
const float sphereLen  = 100;

uint hash(uint x) {
  x += ( x << 10u );
  x ^= ( x >>  6u );
  x += ( x <<  3u );
  x ^= ( x >> 11u );
  x += ( x << 15u );
  return x;
  }

uint hash(uvec2 v) {
  return hash(v.x ^ hash(v.y));
  }

float floatConstruct(uint m) {
  const uint ieeeMantissa = 0x007FFFFFu; // binary32 mantissa bitmask
  const uint ieeeOne      = 0x3F800000u; // 1.0 in IEEE binary32

  m &= ieeeMantissa;                     // Keep only mantissa bits (fractional part)
  m |= ieeeOne;                          // Add fractional part to 1.0

  float  f = uintBitsToFloat( m );       // Range [1:2]
  return f - 1.0;                        // Range [0:1]
  }

// Sample i-th point from Hammersley point set of NumSamples points total.
// See: http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html
vec2 sampleHammersley(uint i, uint numSamples) {
  uint bits = bitfieldReverse(i);
  float vdc = float(bits) * 2.3283064365386963e-10; // / 0x100000000
  return vec2(float(i)/float(numSamples), vdc);
  }

vec3 sampleHemisphere(uint i, uint numSamples, float offsetAng) {
  const vec2  xi  = sampleHammersley(i,numSamples);
  const float u1p = sqrt(max(0.0, 1.0 - xi.y*xi.y));
  const float a   = M_PI*2.0*xi.x + offsetAng;
  return vec3(cos(a) * u1p, sin(a) * u1p, xi.y);
  }

vec3 inverse(vec3 pos) {
  vec4 ret = mvpInv*vec4(pos,1.0);
  return (ret.xyz/ret.w);
  }

vec3 project(vec3 pos) {
  vec4 ret = ubo.mvp*vec4(pos,1.0);
  return (ret.xyz/ret.w);
  }

float readZ(vec2 pos) {
  return textureLod(depth, pos.xy*0.5+vec2(0.5), 0).r;
  }

float sampleRadius(uint i, uint maxSmp) {
  return 0.5+float(i)/float(2*maxSmp);
  }

#if defined(RAY_QUERY)
bool rayTest(vec3 pos0, vec3 dp, float tMin, inout float len) {
  float rayDistance  = len;
  vec3  rayDirection = normalize(dp);
  if(rayDistance<=tMin)
    return true;

  //uint flags = gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsCullNoOpaqueEXT;
  uint flags = gl_RayFlagsOpaqueEXT;
  rayQueryEXT rayQuery;
  rayQueryInitializeEXT(rayQuery, topLevelAS, flags, 0xFF,
                        pos0, tMin, rayDirection, rayDistance);

  while(rayQueryProceedEXT(rayQuery))
    ;

  if(rayQueryGetIntersectionTypeEXT(rayQuery, true)==gl_RayQueryCommittedIntersectionNoneEXT)
    return false;

  len = rayQueryGetIntersectionTEXT(rayQuery, true);
  return true;
  }
#endif

float calcOcclussion() {
  vec3 norm = normalize(textureLod(normals,uv,0).xyz*2.0-vec3(1.0));
  // Compute a tangent frame and rotate the half vector to world space
  vec3 up   = abs(norm.z) < 0.999f ? vec3(0.0f, 0.0f, 1.0f) : vec3(1.0f, 0.0f, 0.0f);
  mat3 tangent;
  tangent[0] = normalize(cross(up, norm));
  tangent[1] = cross(norm, tangent[0]);
  tangent[2] = norm;

  const vec3 at0  = vec3(inPos.xy,readZ(inPos.xy));
  const vec3 pos0 = inverse(at0);

  if(at0.z>=0.998)
    return 0; // sky

  uvec2 fragCoord = uvec2(gl_FragCoord);
  uint  h0        = hash(fragCoord);
  float f0        = M_PI*2.0*floatConstruct(h0);

  float occlusion = 0, weightAll = 0.0001;
  for(uint i=0; i<numSamples; ++i) {
    float r      = sampleRadius(i,numSamples);
    vec3  h      = sampleHemisphere(i,numSamples,f0);

    vec3  v      = tangent * h;
    vec3  at1    = project(pos0 + v*sphereLen*r);
    if(abs(at1.x)>1.0 || abs(at1.y)>1.0) {
#if defined(RAY_QUERY)
      vec3  pos1   = pos0 + v*sphereLen*r;
      vec3  dp     = (pos1-pos0);
      float len    = sphereLen*r;
      bool  isHit  = (rayTest(pos0,v,sphereLen*0.1,len));

      float lenQ   = len*len;
      float angW   = h.z;                                   // angle attenuation.
      float distW  = 1.0-min(lenQ/(sphereLen*sphereLen),1); // distance attenuation
      if(isHit)
        occlusion += angW*distW;
      weightAll += distW;
#endif
      continue;
      }
    float z      = readZ(at1.xy);
    vec3  at2    = vec3(at1.xy,z);
    vec3  pos2   = inverse(at2);
    vec3  dp     = (pos2-pos0);
    bool  isHit  = (z<at1.z);

    float lenQ   = dot(dp,dp);
    float angW   = h.z;                                   // angle attenuation.
    float distW  = 1.0-min(lenQ/(sphereLen*sphereLen),1); // distance attenuation
    if(isHit)
      occlusion += angW*distW;
    weightAll += distW;
    }

  occlusion /= weightAll;
  return occlusion*(M_PI*0.75);
  }

void main() {
  vec3 clr  = textureLod(diffuse,uv,0).rgb;
  vec4 lbuf = textureLod(lightingBuf,uv,0);

  float occ = calcOcclussion();
  outColor  = vec4(occ);
  }

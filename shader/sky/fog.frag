#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive    : enable
#extension GL_EXT_control_flow_attributes : enable

#if defined(VIRTUAL_SHADOW)
#include "virtual_shadow/vsm_common.glsl"
#endif

#include "sky_common.glsl"
#include "scene.glsl"

#if defined(GL_COMPUTE_SHADER)
layout(local_size_x = 1*8, local_size_y = 2*8) in;
const uint NumThreads = gl_WorkGroupSize.x*gl_WorkGroupSize.y*gl_WorkGroupSize.z;
vec2 inPos;
#else
layout(location = 0) in  vec2 inPos;
layout(location = 0) out vec4 outColor;
#endif

#if defined(GL_COMPUTE_SHADER)
// none
#else
layout(binding = 0) uniform sampler3D fogLut;
#endif

layout(binding = 1) uniform sampler2D depth;

layout(binding = 2, std140) uniform UboScene {
  SceneDesc scene;
  };

#if defined(VOLUMETRIC) && defined(GL_COMPUTE_SHADER)
layout(binding = 3, r32ui) uniform writeonly restrict uimage2D occlusionLut;
#elif defined(VOLUMETRIC)
layout(binding = 3, r32ui) uniform readonly  restrict uimage2D occlusionLut;
#endif

#if defined(VOLUMETRIC) && !defined(VIRTUAL_SHADOW) && defined(GL_COMPUTE_SHADER)
layout(binding = 4) uniform sampler2D textureSm1;
#endif

#if defined(VOLUMETRIC) && defined(VIRTUAL_SHADOW) && defined(GL_COMPUTE_SHADER)
layout(binding = 4) uniform utexture3D pageTbl;
layout(binding = 5) uniform texture2D  pageData;
#endif

const float dFogMin = 0;
//const float dFogMax = 1;
const float dFogMax = 0.9999;

#if defined(GL_COMPUTE_SHADER)
uvec2 invocationID = gl_GlobalInvocationID.xy;
#endif

float interleavedGradientNoise() {
#if defined(GL_COMPUTE_SHADER)
  return interleavedGradientNoise(invocationID.xy);
#else
  return interleavedGradientNoise(gl_FragCoord.xy);
#endif
  }

#if defined(VOLUMETRIC) && defined(VIRTUAL_SHADOW) && defined(GL_COMPUTE_SHADER)
bool shadowFactor(vec4 shPos) {
  vec3  shPos0 = shPos.xyz/shPos.w;
  int   mip    = vsmCalcMipIndexFog(shPos0.xy);
  vec2  page   = shPos0.xy / (1 << mip);
  if(any(greaterThan(abs(page), vec2(1))))
    return true;

  float v = shadowTexelFetch(page, mip, pageTbl, pageData);
  return v < shPos.z;
  }
#elif defined(VOLUMETRIC) && defined(GL_COMPUTE_SHADER)
float shadowSample(in sampler2D shadowMap, vec2 shPos) {
  shPos.xy = shPos.xy*vec2(0.5)+vec2(0.5);
  return textureLod(shadowMap,shPos,0).r;
  }

bool calcShadow(vec3 shPos) {
  float sh = shadowSample(textureSm1,shPos.xy);
  return sh < shPos.z;
  }

bool shadowFactor(vec4 shPos) {
  if(abs(shPos.x)>=shPos.w || abs(shPos.y)>=shPos.w)
    return true;
  return calcShadow(shPos.xyz/shPos.w);
  }
#else
bool shadowFactor(vec4 shPos) {
  return true;
  }
#endif

vec3 project(mat4 m, vec3 pos) {
  vec4 p = m*vec4(pos,1);
  return p.xyz/p.w;
  }

#if defined(VOLUMETRIC)
vec4 fog(vec2 uv, float z) {
  const int   steps    = 32;
  const float noise    = interleavedGradientNoise()/steps;

  const vec3  pos0     = project(scene.viewProjectLwcInv, vec3(inPos,dFogMin));
  const vec3  pos1     = project(scene.viewProjectLwcInv, vec3(inPos,dFogMax));
  const vec3  posz     = project(scene.viewProjectLwcInv, vec3(inPos,z));

#if defined(VIRTUAL_SHADOW)
  const vec4  shPos0   = scene.viewVirtualShadowLwc*vec4(pos0, 1);
  const vec4  shPos1   = scene.viewVirtualShadowLwc*vec4(posz, 1);
#else
  const vec4  shPos0   = scene.viewShadowLwc[1]*vec4(pos0, 1);
  const vec4  shPos1   = scene.viewShadowLwc[1]*vec4(posz, 1);
#endif

  const vec3  ray      = pos1.xyz - pos0.xyz;
  const float dist     = length(ray)*0.01;       // meters
  const float distZ    = length(posz-pos0)*0.01; // meters

  // float d    = (dZ-d0)/(d1-d0);
  // return vec4(debugColors[min(int(d*textureSize(fogLut,0).z), textureSize(fogLut,0).z-1)%MAX_DEBUG_COLORS], 1);

#if defined(GL_COMPUTE_SHADER)
  uint occlusion = 0;
  [[dont_unroll]]
  for(uint i=0; i<steps; ++i) {
    float t      = (i+0.3)/float(steps);
    float dd     = (t*distZ)/(dist);
    vec4  shPos  = mix(shPos0,shPos1,t+noise);
    bool  shadow = shadowFactor(shPos);
    occlusion = occlusion | ((shadow ? 1u : 0u) << uint(i));
    }
#else
  vec3  scatteredLight = vec3(0.0);
  float transmittance  = 1.0;
  vec4  prevVal        = textureLod(fogLut, vec3(uv,0), 0);
#endif

#if defined(GL_COMPUTE_SHADER)
  imageStore(occlusionLut, ivec2(invocationID.xy), uvec4(occlusion));
  return vec4(0);
#else
  // every bit = one sample of shadowmap
  const float occlusionScale = textureSize(depth,0).x/imageSize(occlusionLut).x;
  const ivec2 coord          = ivec2(gl_FragCoord.xy/occlusionScale);
  const uint  occlusion      = imageLoad(occlusionLut, coord).r;
  [[dont_unroll]]
  for(int i=0; i<steps; i++) {
    bool  bit    = bitfieldExtract(occlusion,i,1)!=0;
    float shadow = bit ? 1.0 : 0.05;

    {
      // scan for consecutive range of same bits
      uint oc = occlusion;
      if(bit)
        oc = oc ^ 0xFFFFFFFF;
      oc = oc & (0xFFFFFFFF << i);
      int lsb = findLSB(oc);
      i = (lsb<0) ? 31 : lsb-1;
    }

    const float t   = (i+0.3)/float(steps);
    const float dd  = (t*distZ)/(dist);

    const vec4  val = textureLod(fogLut, vec3(uv,dd), 0);
    transmittance   = val.a;
    scatteredLight += (val.rgb-prevVal.rgb)*shadow;
    prevVal = val;
    }
  return vec4(scatteredLight, transmittance);
#endif
  }
#else
#define MAX_DEBUG_COLORS 10
const vec3 debugColors[MAX_DEBUG_COLORS] = {
  vec3(1,1,1),
  vec3(1,0,0),
  vec3(0,1,0),
  vec3(0,0,1),
  vec3(1,1,0),
  vec3(1,0,1),
  vec3(0,1,1),
  vec3(1,0.5,0),
  vec3(0.5,1,0),
  vec3(0,0.5,1),
  };

vec4 fog(vec2 uv, float z) {
  float dZ   = linearDepth(      z, scene.clipInfo);
  float d0   = linearDepth(dFogMin, scene.clipInfo);
  float d1   = linearDepth(dFogMax, scene.clipInfo);
  float d    = (dZ-d0)/(d1-d0);
  // return vec4(debugColors[min(int(d*textureSize(fogLut,0).z), textureSize(fogLut,0).z-1)%MAX_DEBUG_COLORS], 0);
  return textureLod(fogLut, vec3(uv,d), 0);
  }
#endif

#if !defined(GL_COMPUTE_SHADER)
void main_frag() {
  vec2 uv     = inPos*vec2(0.5)+vec2(0.5);
  vec3 view   = normalize(inverse(vec3(inPos,1.0)));
  vec3 sunDir = scene.sunDir;

  const float dMax   = texelFetch(depth,ivec2(gl_FragCoord.xy),0).r;
  const bool  isSky  = (dMax==1.0);

  //const vec3  maxVal = isSky ? textureLod(fogLut, vec3(uv, textureSize(fogLut,0).z-1), 0).rgb : vec3(0);
  const vec4  val    = fog(uv,dMax);

  vec3  lum = val.rgb;// - maxVal;
  float tr  = isSky ? 1 : val.a;

  lum *= scene.GSunIntensity;
  lum *= scene.exposure;

  outColor = vec4(lum, 1.0-tr);
  }
#else
void main_comp() {
  const ivec2 size = imageSize(occlusionLut).xy;
  if(invocationID.x>=size.x || invocationID.y>=size.y)
    return;

  inPos = vec2(invocationID.xy+vec2(0.5))/vec2(size);
  inPos = inPos*2.0 - vec2(1.0);

  vec2  uv     = inPos*vec2(0.5)+vec2(0.5);
  vec3  view   = normalize(inverse(vec3(inPos,1.0)));
  vec3  sunDir = scene.sunDir;
  float z      = min(textureLod(depth,uv,0).r, dFogMax);

  fog(uv,z);
  }
#endif

void main() {
#if defined(GL_COMPUTE_SHADER)
  main_comp();
#else
  main_frag();
#endif
  }

#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive    : enable

#include "sky_common.glsl"
#include "../scene.glsl"

layout(binding = 0) uniform sampler2D tLUT;
layout(binding = 1) uniform sampler2D mLUT;
layout(binding = 2) uniform sampler2D skyLUT;
layout(binding = 3) uniform sampler2D depth;

layout(binding = 4, std140) uniform UboScene {
  SceneDesc scene;
  };
layout(binding = 5) uniform sampler2D cloudsLUT;

#if defined(VOLUMETRIC)
layout(binding = 6) uniform sampler3D fogLut;
layout(binding = 7) uniform sampler2D textureSm1;
#endif

layout(location = 0) in  vec2 inPos;
layout(location = 0) out vec4 outColor;

float interleavedGradientNoise(vec2 pixel) {
  return fract(52.9829189f * fract(0.06711056f*float(pixel.x) + 0.00583715f*float(pixel.y)));
  }

// based on: https://developer.amd.com/wordpress/media/2012/10/Wenzel-Real-time_Atmospheric_Effects_in_Games.pdf
float volumetricFog(in vec3 pos, in vec3 pos1) {
  vec3 dir = pos1-pos;
  // Fog props
  const float fogHeightDensityAtViewer = 0.5;
  const float globalDensity            = 0.005;
  const float heightFalloff            = 0.02;

  // float fogHeightDensityAtViewer = exp(-heightFalloff * pos.z);
  float fogInt = length(dir) * fogHeightDensityAtViewer;
  if(abs(dir.y) > 0.01) {
    float t = heightFalloff*dir.y;
    fogInt *= (1.0-exp(-t))/t;
    }

  return exp(-globalDensity*fogInt);
  }

vec3 transmittance(vec3 pos0, vec3 pos1) {
  const int   steps = 32;
  vec3  transmittance = vec3(1.0);
  vec3  dir  = pos1-pos0;
  float dist = length(dir);
  for(int i=1; i<=steps; ++i) {
    float t      = (float(i)/steps);
    float dt     = dist/steps;
    vec3  newPos = pos0 + t*dir + vec3(0,RPlanet,0);
    vec3  rayleighScattering = vec3(0);
    vec3  extinction         = vec3(0);
    float mieScattering      = float(0);
    scatteringValues(newPos, 0, rayleighScattering, mieScattering, extinction);
    transmittance *= exp(-dt*extinction);
    }

  return transmittance;
  }

#if defined(VOLUMETRIC)
vec4 shadowSample(in sampler2D shadowMap, vec2 shPos) {
  shPos.xy = shPos.xy*vec2(0.5,0.5)+vec2(0.5);
  return textureGather(shadowMap,shPos);
  }

float shadowResolve(in vec4 sh, float z) {
  z  = clamp(z,0,0.99);
  sh = step(sh,vec4(z));
  return 0.25*(sh.x+sh.y+sh.z+sh.w);
  }

float calcShadow(vec3 shPos1) {
  vec4  lay1 = shadowSample(textureSm1,shPos1.xy);
  float v1   = shadowResolve(lay1,shPos1.z);
  if(abs(shPos1.x)<1.0 && abs(shPos1.y)<1.0)
    return v1;
  return 1.0;
  }

float shadowFactor(vec4 shPos) {
  return calcShadow(shPos.xyz/shPos.w);
  }
#else
float shadowFactor(vec4 shPos) {
  return 1;
  }
#endif

vec3 project(mat4 m, vec3 pos) {
  vec4 p = m*vec4(pos,1);
  return p.xyz/p.w;
  }

#if defined(VOLUMETRIC)
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
  const int   stepsMs  = 2;
  const int   steps    = 16 * stepsMs;
  const float noise    = interleavedGradientNoise(gl_FragCoord.xy)/steps;

  const float dMin     = 0;
  const float dMax     = 0.9999;
  const vec3  pos0     = project(scene.viewProjectInv, vec3(inPos,dMin));
  const vec3  pos1     = project(scene.viewProjectInv, vec3(inPos,dMax));
  const vec3  posz     = project(scene.viewProjectInv, vec3(inPos,z));
  const vec4  shPos0   = scene.viewShadow[1]*vec4(pos0, 1);
  const vec4  shPos1   = scene.viewShadow[1]*vec4(posz, 1);

  const vec3  ray      = pos1.xyz - pos0.xyz;
  const float dist     = length(ray)*0.01;       // meters
  const float distZ    = length(posz-pos0)*0.01; // meters

  // float d    = (dZ-d0)/(d1-d0);
  // return vec4(debugColors[min(int(d*textureSize(fogLut,0).z), textureSize(fogLut,0).z-1)%MAX_DEBUG_COLORS], 1);

  vec3  scatteredLight = vec3(0.0);
  float transmittance  = 1.0;

  vec4  prevVal = textureLod(fogLut, vec3(uv,0), 0);
  for(int i=0; i<steps; ) {
    float t      = (i+0.3)/float(steps);
    float dd     = (t*distZ)/(dist);

    vec4  val    = textureLod(fogLut, vec3(uv,dd), 0);

    float shadow = 0;
    for(int r=0; r<stepsMs; ++r, ++i) {
      float t     = (i+0.3)/float(steps);
      vec4  shPos = mix(shPos0,shPos1,t+noise);
      shadow += shadowFactor(shPos)/float(stepsMs);
      }

    transmittance   = val.a;
    scatteredLight += (val.rgb-prevVal.rgb)*shadow;

    prevVal = val;
    }

  return vec4(scatteredLight, 1.0-transmittance);
  }
#else
vec4 fog(vec2 uv, float z) {
  float dMin = 0.0;
  float dMax = 1.0;

  float dZ   = linearDepth(   z, push.clipInfo);
  float d0   = linearDepth(dMin, push.clipInfo);
  float d1   = linearDepth(dMax, push.clipInfo);

  float d       = (dZ-d0)/(d1-d0);

  vec3  pos0    = inverse(vec3(inPos,0));
  vec3  posz    = inverse(vec3(inPos,z));

  vec3  val     = textureLod(skyLUT, uv, 0).rgb;
  vec3  trans   = vec3(1.0)-transmittance(pos0, posz);
  float fogDens = (trans.x+trans.y+trans.z)/3.0;

  vec3  lum      = val.rgb;
  lum *= clamp(d,0,1);
#if !defined(FOG)
  lum = vec3(0);
#endif
  return vec4(lum, fogDens);
  }
#endif

void main() {
  vec2 uv     = inPos*vec2(0.5)+vec2(0.5);
  vec3 view   = normalize(inverse(vec3(inPos,1.0)));
  vec3 sunDir = push.sunDir;

  const float z   = textureLod(depth,uv,0).r;
  const vec4  val = fog(uv,z);

  vec3  lum = val.rgb;
  float tr  = val.a;

  lum = lum * push.GSunIntensity;
  outColor = vec4(lum, tr);
  }

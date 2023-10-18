#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#include "scene.glsl"
#include "common.glsl"

#define SKY_LOD 2
#include "sky/clouds.glsl"

layout(location = 0) out vec4 outColor;

layout(binding = 0, std140) uniform UboScene {
  SceneDesc scene;
  };

#include "gerstner_wave.glsl"

layout(binding = 1) uniform sampler2D sceneColor;
layout(binding = 2) uniform sampler2D gbufDiffuse;
layout(binding = 3) uniform sampler2D gbufNormal;
layout(binding = 4) uniform sampler2D gbufDepth;
layout(binding = 5) uniform sampler2D zbuffer;

layout(binding =  6) uniform sampler2D skyLUT;
layout(binding =  7) uniform sampler2D textureDayL0;
layout(binding =  8) uniform sampler2D textureDayL1;
layout(binding =  9) uniform sampler2D textureNightL0;
layout(binding = 10) uniform sampler2D textureNightL1;

vec3 applyClouds(vec3 skyColor, vec3 refl) {
  float night    = scene.isNight;
  vec3  plPos    = vec3(0,RPlanet,0);

  return applyClouds(skyColor, skyLUT, plPos, scene.sunDir, refl, night,
                     scene.cloudsDir.xy, scene.cloudsDir.zw,
                     textureDayL1,textureDayL0, textureNightL1,textureNightL0);
  }

float intersectPlane(const vec3 pos, const vec3 dir, const vec4 plane) {
  float dist = dot(vec4(pos,1.0), plane);
  float step = -dot(plane.xyz,dir);
  if(abs(step)<0.0001)
    return 0;
  return dist/step;
  }

float intersectFrustum(const vec3 pos, const vec3 dir) {
  float ret = 0;
  for(int i=0; i<6; ++i) {
    // skip left and right planes to hide some SSR artifacts
    if(i==0 || i==1)
      continue;
    float len = intersectPlane(pos,dir,scene.frustrum[i]);
    if(len>0 && (len<ret || ret==0))
      ret = len;
    }
  return ret;
  }

vec3 sunBloom(vec3 refl) {
  const float sunSolidAngle  = 0.53*M_PI/180.0;
  const float minSunCosTheta = cos(sunSolidAngle);

  float cosTheta = dot(refl, scene.sunDir);
  if(cosTheta >= minSunCosTheta)
    return vec3(1.0);
  float offset = minSunCosTheta - cosTheta;
  float gaussianBloom = exp(-offset*50000.0)*0.5;
  float invBloom = 1.0/(0.02 + offset*300.0)*0.01;
  return vec3(gaussianBloom+invBloom);
  }

vec3 ssr(vec4 orig, vec3 start, vec3 refl, const float depth, vec3 sky) {
  const int   SSR_STEPS = 64;
  const float ZBias     = 0.0;
  // const float ZBias     = 0.00004;

  const float rayLen = intersectFrustum(start,refl);
  // return vec3(rayLen*0.01);
  if(rayLen<=0)
    return sky;

  const vec4  dest = scene.viewProject*vec4(start+refl*rayLen, 1.0);
  const float off  = interleavedGradientNoise(gl_FragCoord.xy);

  vec2 uv       = (orig.xy/orig.w)*0.5+vec2(0.5);
  bool found    = false;
  bool occluded = false;

  for(int i=1; i<=SSR_STEPS; ++i) {
    const float t     = (float(i+off)/SSR_STEPS);
    const vec4  pos4  = mix(orig,dest,t*t);
    const vec3  pos   = pos4.xyz/pos4.w;
    if(pos.z>=1.0)
      break;

    const vec2  p        = pos.xy*0.5+vec2(0.5);
    const float smpDepth = textureLod(zbuffer,p,0).r + ZBias;
    if(smpDepth==1.0)
      continue;

    if(pos.z>smpDepth) {
      // screen-space data is occluded
      occluded = true;
      }

    if(pos.z>smpDepth) {
      vec4  hit4   = scene.viewProjectInv*vec4(pos.xy,smpDepth,1.0);
      vec3  hit    = /*normalize*/(hit4.xyz/hit4.w - start);
      float angCos = dot(hit,refl);

      if(angCos>0.0) {
        uv    = p;
        found = true;
        break;
        }
      }
    }
  // return sky;

  const vec3  reflection = textureLod(sceneColor,uv,0).rgb;
  const float att        = smoothstep(0, 0.1, uv.y);// min(1.0,uv.y*4.0);

  if(found)
    return mix(sky,reflection,att);

  if(occluded || true) {
    const float z  = texelFetch(zbuffer, ivec2(gl_FragCoord.xy), 0).r;
    const float zs = linearDepth(z,     scene.clipInfo.xyz);
    const float zb = linearDepth(depth, scene.clipInfo.xyz);
    float h = zs - zb;
    //h = 0.2 + 0.8*smoothstep(0, 1, h*0.01);
    h = 0.1 + 0.9*smoothstep(0, 0.25, h*0.5);
    return sky*h;
    //return vec3(h);
    }

  return sky; //mix(sky, scene.ambient*sky, shadow*0.2);
  }

#if defined(SSR)
vec3 reflection(vec4 orig, vec3 start, vec3 refl, const float depth, vec3 sky) {
  return ssr(orig,start,refl,depth,sky);
  }
#else
vec3 reflection(vec4 orig, vec3 start, vec3 refl, const float depth, vec3 sky) {
  return sky;
  }
#endif

vec3 decodeNormal(vec3 n) {
  n.xz  = n.xz*2.0 - vec2(1.0);
  n.xz *= n.y;
  n.y   = sqrt(1 - n.x*n.x - n.y*n.y);// Y-up
  return n;
  // return vec3(0,1,0);
  }

void main() {
  const vec2  fragCoord = (gl_FragCoord.xy*scene.screenResInv)*2.0-vec2(1.0);
  const vec4  diff      = texelFetch(gbufDiffuse, ivec2(gl_FragCoord.xy), 0);

  if(!isGBufWater(diff.a))
    discard;

  const bool  underWater = (scene.underWater!=0);
  const float ior        = underWater ? IorAir : IorWater;

  const float depth   = texelFetch(gbufDepth,  ivec2(gl_FragCoord.xy), 0).r;
  const vec3  nrm     = texelFetch(gbufNormal, ivec2(gl_FragCoord.xy), 0).rgb;

  const vec4  start4  = scene.viewProjectInv*vec4(fragCoord.x, fragCoord.y, depth, 1.0);
  const vec3  start   = start4.xyz/start4.w;

  const vec3  normal  = decodeNormal(nrm);
  const vec4  scr     = vec4(fragCoord.x, fragCoord.y, depth, 1.0);

  const vec4  camPos4 = scene.viewProjectInv*vec4(0,0,0,1.0);
  const vec3  camPos  = camPos4.xyz/camPos4.w;

  const vec3  view    = normalize(start - camPos);
        vec3  refl    = reflect(view, normal);
  if(refl.y<0) {
    refl.y = 0;
    refl   = normalize(refl);
    }

  const float f = fresnel(refl,normal,ior);
  if(f<=0.0001)
    discard;

  vec3 sky = vec3(0);
  if(!underWater) {
    vec3 sun = sunBloom(refl) * scene.sunCl;// (1.0-scene.isNight);
    sky = textureSkyLUT(skyLUT, vec3(0,RPlanet,0), refl, scene.sunDir);
    // sky = applyClouds(sky+sun, refl);
    sky *= scene.GSunIntensity;
    sky *= scene.exposure;
    }

  vec3 r = reflection(scr,start,refl,depth,sky) * WaterAlbedo * f;
  // vec3  r = reflection(scr,start,refl,depth,sky);

  outColor = vec4(r, 0.0);
  //outColor = vec4(sky, 0.0);
  //outColor = vec4(normal, 0.0);
  }

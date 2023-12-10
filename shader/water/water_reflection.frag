#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#include "scene.glsl"
#include "common.glsl"

layout(location = 0) out vec4 outColor;

layout(binding = 0, std140) uniform UboScene {
  SceneDesc scene;
  };

#include "gerstner_wave.glsl"

layout(binding = 1) uniform sampler2D  sceneColor;
layout(binding = 2) uniform sampler2D  gbufDiffuse;
layout(binding = 3) uniform usampler2D gbufNormal;
layout(binding = 4) uniform sampler2D  gbufDepth;
layout(binding = 5) uniform sampler2D  zbuffer;

layout(binding =  6) uniform sampler2D skyLUT;

float intersectPlane(const vec3 pos, const vec3 dir, const vec4 plane) {
  float dist = dot(vec4(pos,1.0), plane);
  float step = -dot(plane.xyz,dir);
  if(abs(step)<=0.0)
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

vec3 ssr(vec3 orig, vec3 start, vec3 refl, const float depth, vec3 sky) {
  const int   SSR_STEPS     = 64;
  const int   SSR_OCCLUSION = 50;
  const float ZBias         = 0.0;

  const float rayLen = intersectFrustum(start,refl);
  // return vec3(rayLen*0.01);
  if(rayLen<=0)
    return sky;

  const vec4  dest4 = scene.viewProject*vec4(start+refl*rayLen, 1.0);
  const vec3  dest  = dest4.xyz/dest4.w;
  const float off   = interleavedGradientNoise(gl_FragCoord.xy);

  vec2 uv       = orig.xy*0.5+vec2(0.5);
  vec2 uvAlt    = uv;
  bool found    = false;
  int  occluded = 0;

  for(int i=1; i<=SSR_STEPS; ++i) {
    const float t   = (float(i+off)/SSR_STEPS);
    const vec3  pos = mix(orig,dest,t*t);
    if(pos.z>=1.0)
      break;

    const vec2  p        = pos.xy*0.5+vec2(0.5);
    const float smpDepth = textureLod(zbuffer,p,0).r + ZBias;
    if(smpDepth==1.0)
      continue;

    if(smpDepth > pos.z)
      continue; // futher away, than expected

    vec4  hit4   = scene.viewProjectInv*vec4(pos.xy,smpDepth,1.0);
    vec3  hit    = /*normalize*/(hit4.xyz/hit4.w - start);
    float angCos = dot(hit,refl);

    if(angCos<0.0) {
      // screen-space data is occluded
      occluded++;
      uvAlt = p;
      continue;
      }

    uv    = p;
    found = true;
    break;
    }

  // return sky;
  if(!found)
    uv = uvAlt;

  const vec3  reflection = textureLod(sceneColor,uv,0).rgb;
  const float att        = smoothstep(0, 0.1, uv.y);

  if(found)
    return mix(sky,reflection,att);

  if(occluded>SSR_OCCLUSION) {
    const float z  = texelFetch(zbuffer, ivec2(gl_FragCoord.xy), 0).r;

    const vec3 zs = projectiveUnproject(scene.projectInv, vec3(orig.xy, z    ));
    const vec3 zb = projectiveUnproject(scene.projectInv, vec3(orig.xy, depth));

    float h = length(zs - zb)*0.1;
    // float h = 1;
    h = 0.1 + 0.9*smoothstep(0, 0.25, h*0.005);
    return sky*h;
    //return vec3(h);
    }

  return sky; //mix(sky, scene.ambient*sky, shadow*0.2);
  }

#if defined(SSR)
vec3 reflection(vec3 orig, vec3 start, vec3 refl, const float depth, vec3 sky) {
  return ssr(orig,start,refl,depth,sky);
  }
#else
vec3 reflection(vec3 orig, vec3 start, vec3 refl, const float depth, vec3 sky) {
  return sky;
  }
#endif

void main() {
  const vec2  fragCoord = (gl_FragCoord.xy*scene.screenResInv)*2.0-vec2(1.0);
  const vec4  diff      = texelFetch(gbufDiffuse, ivec2(gl_FragCoord.xy), 0);

  if(!isGBufWater(diff.a))
    discard;

  const bool  underWater = (scene.underWater!=0);
  const float ior        = (underWater ? IorAir : IorWater);

  const float depth   = texelFetch (gbufDepth,  ivec2(gl_FragCoord.xy), 0).r;
  const vec3  normal  = normalFetch(gbufNormal, ivec2(gl_FragCoord.xy));

  const vec4  start4  = scene.viewProjectInv*vec4(fragCoord.x, fragCoord.y, depth, 1.0);
  const vec3  start   = start4.xyz/start4.w;

  const vec4  scr     = vec4(fragCoord.x, fragCoord.y, depth, 1.0);

  const vec4  camPos4 = scene.viewProjectInv*vec4(0,0,0,1.0);
  const vec3  camPos  = camPos4.xyz/camPos4.w;

  const vec3  view    = normalize(start - camPos);
        vec3  refl    = reflect(view, normal);
  if(refl.y<0 && !underWater) {
    refl.y = 0;
    refl   = normalize(refl);
    }

  const float f = fresnel(refl,normal,ior);
  if(f<=0.0001)
    discard;

  vec3 sky = vec3(0);
  if(!underWater) {
    vec3 sun = sunBloom(refl) * scene.sunCl;
    sky = textureSkyLUT(skyLUT, vec3(0,RPlanet,0), refl, scene.sunDir);
    sky *= scene.GSunIntensity;
    sky *= scene.exposure;
    }

  vec3 r = reflection(scr.xyz/scr.w,start,refl,depth,sky) * WaterAlbedo * f;
  // vec3  r = reflection(scr,start,refl,depth,sky);

  outColor = vec4(r, 0.0);
  //outColor = vec4(sky, 0.0);
  //outColor = vec4(normal, 0.0);
  }

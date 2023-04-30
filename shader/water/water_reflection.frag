#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#if defined(RAY_QUERY)
#extension GL_EXT_ray_query : enable
#endif

#if defined(RAY_QUERY_AT)
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_ray_flags_primitive_culling : enable
#endif

#include "../scene.glsl"
#include "../common.glsl"
#include "../sky/clouds.glsl"

layout(location = 0) out vec4 outColor;
layout(location = 0) in  vec2 UV;

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

vec4 clouds(vec3 at, vec3 highlight) {
  uint  ticks = scene.tickCount32;
  float t0    = float(ticks%90000 )/90000.f;
  float t1    = float(ticks%270000)/270000.f;
  float night = 0;

  return clouds(at, night, highlight,
                vec2(t0,0), vec2(t1,0),
                textureDayL1,textureDayL0, textureNightL1,textureNightL0);
  }

vec3 applyClouds(vec3 skyColor, vec3 view, vec3 refl) {
  vec3  pos = vec3(0,RPlanet,0);
  float L   = rayIntersect(pos, refl, RClouds);

  // TODO: http://killzone.dl.playstation.net/killzone/horizonzerodawn/presentations/Siggraph15_Schneider_Real-Time_Volumetric_Cloudscapes_of_Horizon_Zero_Dawn.pdf
  // fake cloud scattering inspired by Henyey-Greenstein model
  vec3 lum = vec3(0);
  lum += textureSkyLUT(skyLUT, vec3(0,RPlanet,0), vec3( view.x, view.y*0.0, view.z), scene.sunDir);
  lum += textureSkyLUT(skyLUT, vec3(0,RPlanet,0), vec3(-view.x, view.y*0.0, view.z), scene.sunDir);
  lum += textureSkyLUT(skyLUT, vec3(0,RPlanet,0), vec3(-view.x, view.y*0.0,-view.z), scene.sunDir);
  lum += textureSkyLUT(skyLUT, vec3(0,RPlanet,0), vec3( view.x, view.y*0.0,-view.z), scene.sunDir);
  lum = lum*scene.GSunIntensity;

  lum *= 5.0; //fixme?
  //lum = vec3(1)*scene.GSunIntensity;

  vec4  cloud = clouds(pos + refl*L, lum);
  return mix(skyColor, cloud.rgb, cloud.a);
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
    ;//sun = vec3(1.0);
  float offset = minSunCosTheta - cosTheta;
  float gaussianBloom = exp(-offset*50000.0)*0.5;
  float invBloom = 1.0/(0.02 + offset*300.0)*0.01;
  return vec3(gaussianBloom+invBloom);
  }

vec3 ssr(vec4 orig, vec3 start, vec3 refl, float shadow, vec3 sky) {
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

    const vec2  p     = pos.xy*0.5+vec2(0.5);
    const float depth = textureLod(zbuffer,p,0).r + ZBias;
    if(depth==1.0)
      continue;

    if(pos.z>depth) {
      // screen-space data is occluded
      occluded = true;
      }

    if(pos.z>depth) {
      vec4  hit4   = scene.viewProjectInv*vec4(pos.xy,depth,1.0);
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
  const float att        = smoothstep(-1.0, -0.8, uv.y);// min(1.0,uv.y*4.0);
  if(found)
    return mix(sky,reflection,att);
  if(occluded)
    return sky*0.5; //mix(sky,reflection,att*(1.0-shadow));
  return sky; //mix(sky, scene.ambient*sky, shadow*0.2);
  /*
  float att = min(1.0,uv.y*4.0);//*(max(0,1.0-abs(uv.x*2.0-1.0)));
  if(found)
    return mix(sky,reflection,att);
    //return reflection;
  if(occluded)
    return mix(sky,reflection,att*(1.0-shadow));
  //return sky;
  return mix(sky, scene.ambient*sky, shadow*0.2);
  */
  }

void decodeBits(float v, out bool water) {
  int x = int(v*255+0.5);

  //flt   = (x & (1 << 1))!=0;
  //atst  = (x & (1 << 2))!=0;
  water = (x & (1 << 3))!=0;
  }

vec3 calcNormal(vec3 pos, float waveMaxAmplitude, vec2 offset) {
  const float amplitudeMin = 10;
  const float amplitude    = max(amplitudeMin, waveMaxAmplitude*0.5);

  vec3 lx = dFdx(pos), ly = dFdy(pos);
  float minLength = max(length(lx),length(ly));

  float ang = (offset.y-0.5)*M_PI;
  vec2  off = vec2(cos(ang), sin(ang)) * offset.x;

  Wave w = wave(pos - vec3(off.x,0,off.y), minLength, waveIterationsHigh, amplitude);
  return normalize(cross(w.binormal,w.tangent));
  }

void main(void) {
  const vec2  fragCoord = (gl_FragCoord.xy*scene.screenResInv)*2.0-vec2(1.0);
  const float depth     = texelFetch(gbufDepth,  ivec2(gl_FragCoord.xy), 0).r;
  const vec3  nrm       = texelFetch(gbufNormal, ivec2(gl_FragCoord.xy), 0).rgb;

  const vec4  start4    = scene.viewProjectInv*vec4(fragCoord.x, fragCoord.y, depth, 1.0);
  const vec3  start     = start4.xyz/start4.w;

  const vec3  normal    = calcNormal(start,nrm.x,nrm.yz);

  const vec4  diff      = texelFetch(gbufDiffuse, ivec2(gl_FragCoord.xy), 0);
  bool isWater = false;
  decodeBits(diff.a,isWater);
  if(!isWater)
    discard;

  const vec4  scr     = vec4(fragCoord.x, fragCoord.y, depth, 1.0);

  const vec4  camPos4 = scene.viewProjectInv*vec4(0,0,0,1.0);
  const vec3  camPos  = camPos4.xyz/camPos4.w;

  const vec3  view    = normalize(start - camPos);
        vec3  refl    = reflect(view, normal);
  if(refl.y<0) {
    refl.y = 0;
    refl   = normalize(refl);
    }

  const float f = fresnel(refl,normal,IorWater);
  if(f<=0.0001) {
    discard;
    }

  vec3 sun = sunBloom(refl);
  vec3 sky = textureSkyLUT(skyLUT, vec3(0,RPlanet,0), refl, scene.sunDir) * scene.GSunIntensity;
  sky = applyClouds(sky+sun, view, refl);

  float shadow     = 1;
  vec3  reflection = ssr(scr,start,refl,shadow,sky) * WaterAlbedo * f;

  outColor = vec4(reflection, 0.0);
  //outColor = vec4(sky, 0.0);
  //outColor = vec4(normal, 0.0);
  }

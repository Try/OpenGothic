#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#define FRAGMENT
#include "materials_common.glsl"
#include "../water/gerstner_wave.glsl"
#include "../lighting/shadow_sampling.glsl"
#include "../lighting/tonemapping.glsl"

#if defined(MAT_VARYINGS)
layout(location = 0) in Varyings shInp;
#endif

#if DEBUG_DRAW
layout(location = DEBUG_DRAW_LOC) in flat uint debugId;
#endif

#if defined(GBUFFER)
layout(location = 0) out vec4 outDiffuse;
layout(location = 1) out vec4 outNormal;
#elif defined(WATER)
layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outDiffuse;
layout(location = 2) out vec4 outNormal;
#elif !defined(DEPTH_ONLY)
layout(location = 0) out vec4 outColor;
#endif

#if defined(FORWARD)
float lambert() {
  vec3  normal  = normalize(shInp.normal);
  return clamp(dot(scene.sunDir,normal), 0.0, 1.0);
  }

vec3 diffuseLight() {
  float light  = lambert();
  float shadow = calcShadow(vec4(shInp.pos,1), 0, scene, textureSm0, textureSm1);
  return scene.sunCl.rgb*light*shadow + scene.ambient;
  }

vec4 dbgLambert() {
  float l = lambert();
  return vec4(l,l,l,1.0);
  }
#endif

#if defined(GHOST)
vec3 ghostColor(vec3 selfColor) {
  vec4 back = texelFetch(sceneColor, ivec2(gl_FragCoord.xy), 0);
  return back.rgb+selfColor;
  }
#endif

#if defined(MAT_UV)
vec4 diffuseTex() {
#if (defined(LVL_OBJECT) || defined(WATER))
  vec2 texAnim = vec2(0);
  {
    // FIXME: this not suppose to run for every-single material
    if(bucket.texAniMapDirPeriod.x!=0) {
      uint fract = scene.tickCount32 % abs(bucket.texAniMapDirPeriod.x);
      texAnim.x  = float(fract)/float(bucket.texAniMapDirPeriod.x);
      }
    if(bucket.texAniMapDirPeriod.y!=0) {
      uint fract = scene.tickCount32 % abs(bucket.texAniMapDirPeriod.y);
      texAnim.y  = float(fract)/float(bucket.texAniMapDirPeriod.y);
      }
  }
  const vec2 uv = shInp.uv + texAnim;
#else
  const vec2 uv = shInp.uv;
#endif
  return texture(textureD,uv);
  }
#endif

vec4 forwardShading(vec4 t) {
  vec3  color = t.rgb;
  float alpha = t.a;

#if defined(GHOST)
  color = ghostColor(t.rgb);
#endif

#if defined(FORWARD)
  color *= diffuseLight();
#endif

  return vec4(color,alpha);
  }

#if defined(WATER)
vec4 underWaterColorDepth(vec3 normal) {
  const float ior = IorWater;

  vec4  camPos = scene.viewProjectInv*vec4(0,0,0,1.0);
  camPos.xyz /= camPos.w;

  const vec3  view   = normalize(shInp.pos - camPos.xyz);
  const vec3  refr   = refract(view, normal, ior);

  const float depth  = texelFetch(gbufferDepth, ivec2(gl_FragCoord.xy), 0).r;
  const float ground = linearDepth(depth, scene.clipInfo.xyz);
  const float water  = linearDepth(gl_FragCoord.z, scene.clipInfo.xyz);

  vec3  back     = texelFetch(sceneColor, ivec2(gl_FragCoord.xy), 0).rgb;
  float dist     = (ground-water);
  vec3  rPos     = shInp.pos + dist*refr*10.0;
  vec4  rPosScr  = scene.viewProject*vec4(rPos,1.0);
  rPosScr.xyz /= rPosScr.w;
  const vec2  p2 = rPosScr.xy*0.5+vec2(0.5);
  float depth2   = textureLod(gbufferDepth, p2, 0).r;

  if(depth2>gl_FragCoord.z) {
    back = textureLod(sceneColor, p2, 0).rgb;
    }

  if(depth2>depth) {
    const float ground2 = linearDepth(depth2, scene.clipInfo.xyz);
    dist = (ground2-water);
    }

  return vec4(back,dist);
  }

vec3 waterScatter(vec3 back, vec3 normal, float depth) {
  /**
    TODO: Cheap and Convincing Subsurface Scattering Look
    https://www.slideshare.net/colinbb/colin-barrebrisebois-gdc-2011-approximating-translucency-for-a-fast-cheap-and-convincing-subsurfacescattering-look-7170855
    */
  const float attenuation   = min(1.0 - exp(-4.0 * depth), 1.0);
  const float transmittance = exp(-depth*1.5);

  const float lamb = max(dot(scene.sunDir,normal), 0.0);

  // vec3 scatterBase = vec3(0.25,0.55,0.5);
  vec3 scatterBase = vec3(0.25,0.55,0.5)/vec3(0.94, 0.87, 0.76);

  // NOTE: need to fix out HDR(overall) and use scene.GSunIntensity
  vec3 scatter     = scatterBase * lamb * scene.sunCl.rgb;

  back    = back * transmittance;
  scatter = back * scatter;

  return mix(back, scatter, attenuation);
  }
#endif

#if defined(WATER)
vec4 waterShading(vec4 t, const vec3 normal) {
  const float ior = IorWater;

  vec4  camPos = scene.viewProjectInv*vec4(0,0,0,1.0);
  camPos.xyz /= camPos.w;

  const vec3  view   = normalize(shInp.pos - camPos.xyz);
  const vec3  refr   = refract(view, normal, ior);
        vec3  refl   = reflect(view, normal);
  if(refl.y<0) {
    //refl.y = 0;
    //refl   = normalize(refl);
    }

  const vec4  back = underWaterColorDepth(normal);
  const float f    = fresnel(refl,normal,ior);

  vec3 color = waterScatter(back.rgb, normal, back.a) * (1.0-f);
  // color = waterColor(color,normal) * WaterAlbedo ;
  return vec4(color,1);
  }
#endif

bool isFlat() {
#if defined(GBUFFER) && (MESH_TYPE==T_LANDSCAPE)
  {
    vec3 pos   = shInp.pos;
    vec3 dx    = dFdx(pos);
    vec3 dy    = dFdy(pos);
    vec3 flatN = (cross(dx,dy));
    if(dot(normalize(flatN),scene.sunDir)<=0.01)
      return true;
  }
#endif
  return false;
  }

float encodeHintBits() {
  const int flt  = (isFlat() ? 1 : 0) << 1;
#if defined(ATEST)
  const int atst = (1) << 2;
#else
  const int atst = (0) << 2;
#endif

#if defined(WATER)
  const int water = (1) << 3;
#else
  const int water = (0) << 3;
#endif

  return float(flt | atst | water)/255.0;
  }

vec3 encodeNormal(vec3 n) {
  float l = length(n.xz);
  if(l>0.0)
    n.xz /= l;
  n.y   = l;
  n.xz = n.xz*0.5 + vec2(0.5);
  return n;
  }

void main() {
#if defined(MAT_UV)
  vec4 t = diffuseTex();
#  if defined(ATEST)
  if(t.a<0.5)
    discard;
#  endif
#endif

#if defined(MAT_COLOR)
  t *= shInp.color;
#endif

#if defined(MAT_LINEAR_CLR)
  t.rgb = textureLinear(t.rgb);
#endif

#if defined(GBUFFER)
  //outDiffuse.rgb = vec3(1);
  outDiffuse.rgb = t.rgb;
  outDiffuse.a   = encodeHintBits();
  outNormal      = vec4(shInp.normal*0.5 + vec3(0.5),1.0);
#if DEBUG_DRAW
  outDiffuse.rgb *= debugColors[debugId%MAX_DEBUG_COLORS];
#endif
#endif

#if defined(WATER)
  {
  vec3 lx = dFdx(shInp.pos), ly = dFdy(shInp.pos);
  float minLength = max(length(lx),length(ly));

  Wave wx = wave(shInp.pos, minLength, waveIterationsHigh, waveAmplitude());

  if(gl_FrontFacing)
    wx.normal = -wx.normal;

  outColor       = waterShading(t,wx.normal);
  outDiffuse.rgb = t.rgb;
  outDiffuse.a   = encodeHintBits();
  outNormal      = vec4(encodeNormal(wx.normal), 0);
  }
#elif !defined(GBUFFER) && !defined(DEPTH_ONLY)
  outColor   = forwardShading(t);
#if DEBUG_DRAW
  outColor   = vec4(debugColors[debugId%MAX_DEBUG_COLORS],1.0);
#endif

  //outColor = vec4(inZ.xyz/inZ.w,1.0);
  //outColor = vec4(vec3(inPos.xyz)/1000.0,1.0);
  //outColor = vec4(vec3(shMap),1.0);
  //outColor = vec4(vec3(calcLight()),1.0);
  //outColor = vec4(vec3(calcShadow()),1.0);
  //vec3 shPos0  = (shInp.shadowPos[0].xyz)/shInp.shadowPos[0].w;
  //outColor   = vec4(vec3(shPos0.xy,0),1.0);
  //outColor = dbgLambert();
#endif
  }

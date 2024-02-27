#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#include "materials_common.glsl"
#include "water/gerstner_wave.glsl"
#include "lighting/shadow_sampling.glsl"
#include "lighting/tonemapping.glsl"

#if defined(MAT_VARYINGS)
layout(location = 0) in flat uint bucketId;
layout(location = 1) in Varyings  shInp;
#endif

#if DEBUG_DRAW
layout(location = DEBUG_DRAW_LOC) in flat uint debugId;
#endif

#if defined(GBUFFER)
layout(location = 0) out vec4 outDiffuse;
layout(location = 1) out uint outNormal;
#elif defined(WATER)
layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outDiffuse;
layout(location = 2) out uint outNormal;
#elif !defined(DEPTH_ONLY)
layout(location = 0) out vec4 outColor;
#endif

#if defined(WATER) || defined(GHOST)
float unproject(float depth) {
  mat4 projInv = scene.projectInv;
  vec4 o;
  o.z = depth * projInv[2][2] + projInv[3][2];
  o.w = depth * projInv[2][3] + projInv[3][3];
  return o.z/o.w;
  }
#endif

bool isFlat() {
#if defined(GBUFFER) && defined(FLAT_NORMAL)
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
  const int water = (gl_FrontFacing) ? 0 : (1 << 3);
#elif defined(LVL_OBJECT)
  // const int water = (bucket.envMapping>0.01 ? 1 : 0) << 3;
  const int water = (0) << 3;
#else
  const int water = (0) << 3;
#endif

  return float(flt | atst | water)/255.0;
  }

#if defined(GBUFFER)
vec3 flatNormal() {
#if defined(FLAT_NORMAL)
  vec3 pos   = shInp.pos;
  vec3 dx    = dFdx(pos);
  vec3 dy    = dFdy(pos);
  return normalize(cross(dx,dy));
#else
  return shInp.normal;
#endif
  }
#endif

#if defined(FORWARD)
float lambert() {
  vec3  normal  = normalize(shInp.normal);
  return clamp(dot(scene.sunDir,normal), 0.0, 1.0);
  }

vec3 diffuseLight() {
  float light  = lambert();
  float shadow = calcShadow(vec4(shInp.pos,1), 0, scene, textureSm0, textureSm1);

  vec3  lcolor  = scene.sunColor * Fd_Lambert * light * shadow;
  vec3  ambient = scene.ambient; // TODO: irradiance
  lcolor *= (1.0/M_PI); // magic constant, non motivated by physics

  return lcolor + ambient;
  }

vec4 dbgLambert() {
  float l = lambert();
  return vec4(l,l,l,1.0);
  }
#endif

#if defined(MAT_UV)
vec4 diffuseTex() {
#if !defined(SIMPLE_MAT) && (MESH_TYPE!=T_PFX)
  ivec2 texAniMapDirPeriod = bucket[bucketId].texAniMapDirPeriod;
  float alphaWeight        = bucket[bucketId].alphaWeight;
#else
  ivec2 texAniMapDirPeriod = ivec2(0);
  float alphaWeight        = 1;
#endif

#if !defined(SIMPLE_MAT)
  vec2 texAnim = vec2(0);
  {
    // FIXME: this not suppose to run for every-single material
    if(texAniMapDirPeriod.x!=0) {
      uint fract = scene.tickCount32 % abs(texAniMapDirPeriod.x);
      texAnim.x  = float(fract)/float(texAniMapDirPeriod.x);
      }
    if(texAniMapDirPeriod.y!=0) {
      uint fract = scene.tickCount32 % abs(texAniMapDirPeriod.y);
      texAnim.y  = float(fract)/float(texAniMapDirPeriod.y);
      }
  }
  const vec2 uv = shInp.uv + texAnim;
#else
  const vec2 uv = shInp.uv;
#endif

#if defined(BINDLESS)
  nonuniformEXT uint tId = bucketId;
#else
  const         uint tId = 0;
#endif

  vec4 tex = texture(sampler2D(textureMain[tId], samplerMain),uv);

#if !defined(SIMPLE_MAT)
  tex.a *= alphaWeight;
#endif

  // return vec4(1,1,1, tex.a);
  return tex;
  }
#endif

#if defined(GBUFFER)
void mainGBuffer(vec4 t) {
  outDiffuse.rgb = t.rgb;
  outDiffuse.a   = encodeHintBits();
  outNormal      = encodeNormal(shInp.normal);
  // outNormal      = vec4(flatNormal()*0.5 + vec3(0.5), 1.0);
#if DEBUG_DRAW
  outDiffuse.rgb *= debugColors[debugId%debugColors.length()];
#endif
  }
#endif

#if defined(FORWARD)
void mainForward(vec4 t) {
  vec3  color = t.rgb;
  float alpha = t.a;

#if defined(ATEST)
  alpha = (alpha-0.5)*2.0;
#endif

  color = textureLinear(color.rgb);
  color *= diffuseLight();
  color *= scene.exposure;

  outColor = vec4(color,alpha);
  }
#endif

#if defined(EMISSIVE)
void mainEmissive(vec4 t) {
  vec3 color = textureLinear(t.rgb);
  color *= 3.0;

  outColor = vec4(color,t.a);
  }
#endif

#if defined(GHOST)
void mainGhost(vec4 t) {
  vec3  color  = textureLinear(t.rgb) * 5.0;
  vec3  normal = normalize(shInp.normal);

  normal = (scene.viewProject*vec4(normal,0.0)).xyz;

  vec2  fragCoord = (gl_FragCoord.xy*scene.screenResInv)*2.0-vec2(1.0);
  fragCoord += normal.xy * 0.01;

  vec4 back = textureLod(sceneColor, (fragCoord*0.5+0.5), 0);

  outColor = vec4(mix(back.rgb * color, back.rgb, vec3(0.6)), t.a);
  }
#endif

#if defined(WATER)
vec4 underWaterColorDepth(vec3 normal) {
  const vec2  fragCoord = (gl_FragCoord.xy*scene.screenResInv)*2.0-vec2(1.0);
  const float ior       = IorWater;
  //return vec4(0);

  vec4  camPos = scene.viewProjectInv*vec4(0,0,0.0,1.0);
  camPos.xyz /= camPos.w;

  const vec3  view   = normalize(shInp.pos - camPos.xyz);
  const vec3  refr   = refract(view, normal, ior);

  vec3        back   = texelFetch(sceneColor,   ivec2(gl_FragCoord.xy), 0).rgb;
  const float depth  = texelFetch(gbufferDepth, ivec2(gl_FragCoord.xy), 0).r;

  const float ground = unproject(depth);
  const float water  = unproject(gl_FragCoord.z);
  float       dist   = (ground-water);

  const vec2 p2 = (gl_FragCoord.xy*scene.screenResInv) + normal.xz * min(dist*0.01,1.0) * 0.1;

  float depth2 = textureLod(gbufferDepth, p2, 0).r;
  if(depth2>gl_FragCoord.z) {
    back   = textureLod(sceneColor, p2, 0).rgb;
    const float ground2 = unproject(depth2);
    dist = (ground2-water);
    } else {
    depth2 = depth;
    }

  vec4 fragPos1 = scene.viewProjectInv*vec4(fragCoord,depth2,1.0);
  fragPos1.xyz /= fragPos1.w;

  //return vec4(back,length(fragPos1.xyz - fragPos0.xyz));
  return vec4(back,length(fragPos1.xyz - shInp.pos.xyz));
  }

vec3 waterScatter(vec3 back, vec3 normal, float len) {
  float depth         = len / 5000.0; // 50 meters
  vec3  transmittance = exp(-depth * vec3(4,2,1)*1.25);
  // note: less sun light and less obsevable light
  transmittance = transmittance*transmittance;

  const float f       = fresnel(scene.sunDir,normal,IorWater);
  const vec3  scatter = f * scene.sunColor * (1-exp(-len/20000.0)) * max(scene.sunDir.y, 0);
  return (back + scatter*scene.exposure)*transmittance;
  }

vec4 waterShading(vec4 t, const vec3 normal) {
  const bool underWater = (scene.underWater!=0);

  const float ior = underWater ? IorAir : IorWater;

  vec4 camPos = scene.viewProjectInv*vec4(0,0,0,1.0);
  camPos.xyz /= camPos.w;

  const vec3  view   = normalize(shInp.pos - camPos.xyz);
  const vec3  refr   = refract(view, normal, ior);
        vec3  refl   = reflect(view, normal);

  const float f      = fresnel(refl,normal,ior);

  if(underWater) {
    vec3 back = texelFetch(sceneColor, ivec2(gl_FragCoord.xy), 0).rgb;
    return vec4(back.rgb * (1.0-f),1);
    }

  const vec4 back  = underWaterColorDepth(normal);
  const vec3 color = waterScatter(back.rgb, normal, back.a) * (1.0-f);
  // color = waterColor(color,normal) * WaterAlbedo ;
  return vec4(color,1);
  }

void mainWater(vec4 t) {
  const float waveMaxAmplitude = bucket[bucketId].waveMaxAmplitude;

  vec3 lx = dFdx(shInp.pos), ly = dFdy(shInp.pos);
  float minLength = max(length(lx),length(ly));

  Wave wx = wave(shInp.pos, minLength, waveIterationsHigh, waveAmplitude(waveMaxAmplitude));

  if(gl_FrontFacing) {
    // BROKEN: water mesh is two sided
    wx.normal = -wx.normal;
    }

  outColor       = waterShading(t,wx.normal);
  outDiffuse.rgb = t.rgb;
  outDiffuse.a   = encodeHintBits();
  outNormal      = encodeNormal(wx.normal);
  }
#endif

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

#if defined(GBUFFER)
  mainGBuffer(t);
#elif defined(WATER)
  mainWater(t);
#elif defined(FORWARD) && !defined(DEPTH_ONLY)
  mainForward(t);
#elif defined(EMISSIVE) && !defined(DEPTH_ONLY)
  mainEmissive(t);
#elif defined(GHOST) && !defined(DEPTH_ONLY)
  mainGhost(t);
#endif

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
  }

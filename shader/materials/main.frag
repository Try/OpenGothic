#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#if defined(VIRTUAL_SHADOW)
#include "virtual_shadow/vsm_common.glsl"
#endif

#include "materials_common.glsl"
#include "water/gerstner_wave.glsl"
#include "lighting/shadow_sampling.glsl"
#include "lighting/tonemapping.glsl"

#if defined(MAT_VARYINGS)
layout(location = 0) in flat uint bucketId;
layout(location = 1) in Varyings  shInp;
#endif

#if defined(VIRTUAL_SHADOW)
layout(location = 3) in flat uint vsmMipId;
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

#if defined(VIRTUAL_SHADOW)
layout(push_constant, std430) uniform Push {
  uint commandId;
  } push;
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
float lambert(const vec3 normal) {
  return clamp(dot(scene.sunDir,normal), 0.0, 1.0);
  }

float henyeyGreenstein(float cosTheta, float g) {
  //g = clamp(g, -0.99, 0.99);
  float g2 = g * g;
  float denom = 1.0 + g2 - 2.0 * g * cosTheta;
  return (1.0 - g2) / (4.0 * M_PI * pow(denom, 1.5));
  }

vec3 diffuseLight(float a) {
  vec3  norm   = normalize(shInp.normal);
#if (MESH_TYPE==T_PFX)
  vec3  view   = normalize(shInp.pos - scene.camPos);
  float light  = henyeyGreenstein(-dot(view,scene.sunDir), a*0.63);
#else
  float light  = lambert(norm);
#endif
  float shadow = calcShadow(vec4(shInp.pos,1), 0, scene, textureSm0, textureSm1);

  vec3  lcolor  = scene.sunColor * light * shadow;
  vec3  ambient = scene.ambient + (norm.y*0.25+0.75) * NightAmbient * Fd_Lambert;
  vec3  sky     = vec3(0); // TODO: irradiance

  return (lcolor + ambient + ambient);
  }

vec4 dbgLambert() {
  vec3  norm = normalize(shInp.normal);
  float l    = lambert(norm);
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

  color = textureAlbedo(color.rgb);
  color *= diffuseLight(alpha);
  color *= scene.exposure;

  outColor = vec4(color,alpha);
  }
#endif

#if defined(EMISSIVE)
// ---------------------------------------------------------------------------
// Gothic-1 magic barrier: a procedural electric energy dome.
//
// The dome mesh is flat-shaded, so its per-triangle normals are useless as a
// pattern domain (they produce faceted blobs). Instead we derive a *smooth
// radial direction* from the bucket's bounding box centre: every fragment is
// turned into a unit-sphere direction, giving a seamless, scale-independent
// domain that wraps the whole dome regardless of its size in world units.
//
// Electricity is drawn as thin branching filaments: a fractal scalar field is
// domain-warped and we glow along its iso-contours with a 1/distance falloff,
// giving a bright hot core surrounded by a soft halo - the classic look of an
// electric arc - rather than soft noise clouds.
//
// The barrier is drawn as three coincident additive layers (SrcAlpha/One):
//   layer 1 - the membrane    (faint translucent shell + fresnel rim)
//   layer 2 - the main bolts   (thick, slow, branching arcs)
//   layer 3 - the crackle      (fine fast sparks)
// The on-screen contribution of each layer is color.rgb * alpha.
// ---------------------------------------------------------------------------

float g1bHash(vec3 p) {
  p  = fract(p * vec3(0.1031, 0.1030, 0.0973));
  p += dot(p, p.yzx + 19.19);
  return fract((p.x + p.y) * p.z);
  }

float g1bNoise(vec3 x) {
  vec3 p = floor(x);
  vec3 f = fract(x);
  f = f * f * (3.0 - 2.0 * f);
  return mix(mix(mix(g1bHash(p + vec3(0,0,0)), g1bHash(p + vec3(1,0,0)), f.x),
                 mix(g1bHash(p + vec3(0,1,0)), g1bHash(p + vec3(1,1,0)), f.x), f.y),
             mix(mix(g1bHash(p + vec3(0,0,1)), g1bHash(p + vec3(1,0,1)), f.x),
                 mix(g1bHash(p + vec3(0,1,1)), g1bHash(p + vec3(1,1,1)), f.x), f.y), f.z);
  }

float g1bFbm(vec3 p) {
  float f = 0.0, amp = 0.5;
  for(int i=0; i<4; ++i) {
    f   += amp * g1bNoise(p);
    p   *= 2.02;
    amp *= 0.5;
    }
  return f;
  }

// Distance to the nearest electric filament. A fractal field is strongly
// domain-warped so its iso-contours become jagged, branching channels; the
// returned value is ~0 on a channel and grows away from it.
float g1bArcField(vec3 p) {
  vec3 w = vec3(g1bNoise(p), g1bNoise(p + 4.7), g1bNoise(p + 9.2));
  p += (w - 0.5) * 2.4;             // jagged branching displacement
  float f = g1bFbm(p * 1.7);
  return abs(f - 0.5);
  }

// Turn the filament distance into a glowing arc: a thin white-hot core with a
// soft falloff halo. 'width' controls thickness, 'sharp' the falloff.
float g1bArcGlow(float dist, float width, float sharp) {
  return pow(width / (dist + width), sharp);
  }

// Patchy temporal flicker so different regions of the dome strike at different
// moments instead of pulsing as one - real lightning is intermittent.
float g1bFlicker(vec3 dir, float time) {
  float n = g1bNoise(vec3(dir.xz * 3.0 + dir.y * 2.0, time));
  return smoothstep(0.45, 0.95, n);
  }

void mainEmissive(vec4 t) {
  vec3  color = textureEmmisive(t.rgb);
  float alpha = t.a;

#if !defined(SIMPLE_MAT) && (MESH_TYPE!=T_PFX)
  if((bucket[bucketId].flags & BK_G1_BARRIER) != 0) {
    // world-space fragment position, reconstructed from depth
    vec2 ndc     = (gl_FragCoord.xy*scene.screenResInv)*2.0 - vec2(1.0);
    vec4 wpos    = scene.viewProjectInv * vec4(ndc, gl_FragCoord.z, 1.0);
    vec3 fragPos = wpos.xyz / wpos.w;

    // Smooth radial direction from the dome's bounding-box centre. The mesh is
    // flat-shaded, so this - not the faceted vertex normal - is what gives a
    // continuous pattern domain across the whole dome.
    vec3 bmin   = bucket[bucketId].bbox[0].xyz;
    vec3 bmax   = bucket[bucketId].bbox[1].xyz;
    vec3 center = (bmin + bmax) * 0.5;
    vec3 rad    = max((bmax - bmin) * 0.5, vec3(1.0));
    vec3 dir    = normalize((fragPos - center) / rad);

    vec3  view = normalize(scene.camPos - fragPos);
    float ndv  = abs(dot(view, dir));
    // grazing-angle fresnel: the dome silhouette glows brightest
    float fres = pow(clamp(1.0 - ndv, 0.0, 1.0), 2.5);

    float time  = float(scene.tickCount32 % 4194304u) * 0.001;
    uint  layer = bucket[bucketId].g1BarrierLayer;

    // electric blue-white palette
    const vec3 cDeep   = vec3(0.01, 0.06, 0.16);   // shell tint
    const vec3 cArc    = vec3(0.15, 0.55, 1.00);   // arc halo
    const vec3 cHot     = vec3(0.75, 0.92, 1.00);  // near-core
    const vec3 cCore   = vec3(0.95, 0.99, 1.00);   // white-hot core

    if(layer == 1u) {
      // ---- membrane: a clearly visible, slowly drifting energy shell -------
      float e1     = g1bFbm(dir*2.0 + vec3(0.0,        -time*0.04, 0.0));
      float e2     = g1bFbm(dir*4.0 + vec3(time*0.02,  -time*0.03, 0.0));
      float energy = e1*0.6 + e2*0.4;

      vec3  tint = mix(cDeep, cArc, energy) * 1.2 + cHot*fres*0.9;
      // a constant floor keeps the whole dome visible; energy + rim add shape
      float glow = 0.30 + energy*0.30 + fres*0.70;

      color = tint;
      alpha = clamp(glow, 0.0, 1.0);
      }
    else if(layer == 2u) {
      // ---- main bolts: thick, stable, slowly travelling branching arcs -----
      vec3  p    = dir*5.0 + vec3(0.0, -time*0.15, time*0.03);
      float d    = g1bArcField(p);
      float bolt = g1bArcGlow(d, 0.025, 1.6);
      // slow, smooth brightening - the arcs stay lit and gently pulse
      bolt *= mix(0.55, 1.0, g1bFlicker(dir, time*0.8));

      float core = smoothstep(0.55, 1.0, bolt);
      color = cArc*bolt*1.4 + cCore*core*1.6;
      alpha = clamp(bolt*1.1 + core, 0.0, 1.0) * mix(0.8, 1.0, fres);
      }
    else {
      // ---- crackle: secondary finer arcs, drifting slowly ------------------
      vec3  p     = dir*9.0 + vec3(3.3, -time*0.30, time*0.06);
      float d     = g1bArcField(p);
      float crack = g1bArcGlow(d, 0.016, 1.8);
      crack *= mix(0.35, 1.0, g1bFlicker(dir*2.0, time*1.6));

      float core = smoothstep(0.6, 1.0, crack);
      color = cHot*crack + cCore*core*1.4;
      alpha = clamp(crack*0.85 + core, 0.0, 1.0);
      }

    // The barrier blends additively, so the contribution is color*alpha. Fold
    // it into a premultiplied colour and softly roll off the peak so several
    // overlapping bright arcs cannot blow out to a flat white sheet.
    vec3  contrib = color * alpha;
    float peak    = max(contrib.r, max(contrib.g, contrib.b));
    contrib *= 4.0 / max(4.0, peak);
    color = contrib;
    alpha = 1.0;
    }
#endif

  outColor = vec4(color, alpha);
  }
#endif

#if defined(GHOST)
void mainGhost(vec4 t) {
  vec3  color  = textureAlbedo(t.rgb) * 5.0;
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

  const vec3  camPos = scene.camPos;
  const vec3  view   = normalize(shInp.pos - camPos);
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

#if DEBUG_DRAW && !defined(GBUFFER) && !defined(DEPTH_ONLY)
  outColor   = vec4(debugColors[debugId%debugColors.length()],1.0);
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

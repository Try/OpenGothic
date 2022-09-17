#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#define FRAGMENT
#include "materials_common.glsl"

#if defined(MAT_VARYINGS)
layout(location = 0) in Varyings shInp;
#endif

#if DEBUG_DRAW
layout(location = DEBUG_DRAW_LOC) in flat uint debugId;
#endif

#if !defined(DEPTH_ONLY)
layout(location = 0) out vec4 outColor;
#endif

#if defined(GBUFFER)
layout(location = 1) out vec4 outDiffuse;
layout(location = 2) out vec4 outNormal;
layout(location = 3) out vec4 outDepth;
#endif

#if !defined(DEPTH_ONLY)
vec4 shadowSample(in sampler2D shadowMap, vec2 shPos) {
  shPos.xy = shPos.xy*vec2(0.5,0.5)+vec2(0.5);
  return textureGather(shadowMap,shPos);
  }

float shadowResolve(in vec4 sh, float z) {
  z  = max(0,z);
  sh = step(sh,vec4(z));
  return 0.25*(sh.x+sh.y+sh.z+sh.w);
  }

float calcShadow(vec3 shPos0, vec3 shPos1) {
  vec4  lay0   = shadowSample(textureSm0,shPos0.xy);
  vec4  lay1   = shadowSample(textureSm1,shPos1.xy);

  vec2  minMax = scene.closeupShadowSlice;
  bool  inSm0  = abs(shPos0.x)<1.0 && abs(shPos0.y)<1.0;
  bool  inSm1  = abs(shPos1.x)<1.0 && abs(shPos1.y)<1.0;

  if(inSm0 && lay1.x<minMax[1])
    return shadowResolve(lay0,shPos0.z);
  if(inSm1)
    return shadowResolve(lay1,shPos1.z);
  return 1.0;
  }

float calcShadow() {
  // const vec2  fragCoord = (gl_FragCoord.xy*scene.screenResInv)*2.0-vec2(1.0);
  // const vec4  scr       = vec4(fragCoord.x, fragCoord.y, gl_FragCoord.z, 1.0)/gl_FragCoord.w;
  // vec4  pos4 = scene.viewProjectInv * scr;

  vec4 shadowPos[2];
  // shadowPos[0] = scene.viewShadow[0]*vec4(pos4);
  // shadowPos[1] = scene.viewShadow[1]*vec4(pos4);
  shadowPos[0] = shInp.shadowPos[0];
  shadowPos[1] = shInp.shadowPos[1];

  vec3 shPos0  = (shadowPos[0].xyz)/shadowPos[0].w;
  vec3 shPos1  = (shadowPos[1].xyz)/shadowPos[1].w;
  return calcShadow(shPos0,shPos1);
  }
#endif

#if !defined(DEPTH_ONLY)
vec4 dbgLambert() {
  vec3  normal  = normalize(shInp.normal);
  float lambert = max(0.0,dot(scene.sunDir,normal));
  return vec4(lambert,lambert,lambert,1.0);
  }

vec3 flatNormal() {
  const vec2  fragCoord = (gl_FragCoord.xy*scene.screenResInv)*2.0-vec2(1.0);
  const vec4  scr       = vec4(fragCoord.x, fragCoord.y, gl_FragCoord.z, 1.0)/gl_FragCoord.w;
  const vec4  pos4      = scene.viewProjectInv * scr;

  vec3 pos = pos4.xyz/pos4.w;
  vec3 dx  = dFdx(pos);
  vec3 dy  = dFdy(pos);
  return /*normalize*/(cross(dx,dy));
  }

vec3 calcLight() {
  vec3  normal  = normalize(shInp.normal);
  float lambert = max(0.0,dot(scene.sunDir,normal));

#if (MESH_TYPE==T_LANDSCAPE)
  // fix self-shadowed surface
  // float flatSh = dot(scene.sunDir,flatNormal());
  // if(flatSh<=0) {
  //   lambert = 0;
  //   }
#endif

  float light   = lambert*calcShadow();
  vec3  color   = scene.sunCl.rgb*clamp(light,0.0,1.0);
  return color + scene.ambient;
  }
#endif

#if defined(WATER)
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

vec3 ssr(vec4 orig, vec3 start, vec3 refl, vec3 fallbackClr) {
  const int SSR_STEPS = 64;

  vec3 sky = textureSkyLUT(skyLUT, vec3(0,RPlanet,0), refl, scene.sunDir);
  sky *= GSunIntensity;
  sky = jodieReinhardTonemap(sky);
  sky = srgbEncode(sky);

  // const float rayLen = 10000;
  const float rayLen = intersectFrustum(start,refl);
  // return vec3(rayLen*0.01);
  if(rayLen<=0)
    return sky;

  const vec4  dest = scene.viewProject*vec4(start+refl*rayLen, 1.0);

  vec2 uv       = (orig.xy/orig.w)*0.5+vec2(0.5);
  bool found    = false;
  bool occluded = false;

  for(int i=1; i<=SSR_STEPS; ++i) {
    const float t     = (float(i)/SSR_STEPS);
    const vec4  pos4  = mix(orig,dest,t*t);
    const vec3  pos   = pos4.xyz/pos4.w;
    if(pos.z>=1.0)
      break;

    const vec2  p     = pos.xy*0.5+vec2(0.5);
    const float depth = texture(gbufferDepth,p).r;
    if(depth==1.0)
      continue;

    if(pos.z>=depth) {
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

  const vec3 reflection = textureLod(gbufferDiffuse,uv,0).rgb;
  float att = min(1.0,uv.y*4.0);//*(max(0,1.0-abs(uv.x*2.0-1.0)));
  if(found)
    return mix(sky,reflection,att);
    //return reflection;
  if(occluded)
    return mix(sky,reflection,att);
  return sky;
  }

vec4 waterColor(vec3 color, vec3 albedo) {
  const float F   = 0.02;
  const float ior = 1.0 / 1.52; // air / water
  //return vec4(vec3(albedo.a),1);

  vec4  camPos = scene.viewProjectInv*vec4(0,0,0,1.0);
  camPos.xyz /= camPos.w;

  const vec3  view     = normalize(shInp.pos - camPos.xyz);
  const vec3  refl     = reflect(view, shInp.normal);
  const vec3  refr     = refract(view, shInp.normal, ior);

  const float depth    = texelFetch(gbufferDepth,   ivec2(gl_FragCoord.xy), 0).r;
  const vec3  backOrig = texelFetch(gbufferDiffuse, ivec2(gl_FragCoord.xy), 0).rgb;

  const float ground   = reconstructCSZ(depth, scene.clipInfo);
  const float water    = reconstructCSZ(gl_FragCoord.z, scene.clipInfo);
  const float dist     = -(water-ground);
  //return vec4(vec3(dist),1);

  const vec2  fragCoord = (gl_FragCoord.xy*scene.screenResInv)*2.0-vec2(1.0);
  const vec4  scr       = vec4(fragCoord.x, fragCoord.y, gl_FragCoord.z, 1.0)/gl_FragCoord.w;

  vec3 sky = ssr(scr,shInp.pos,refl,backOrig)*albedo;
  // return vec4(sky,1);

  const float f = fresnel(refl,shInp.normal,ior);
  //return vec4(f,f,f,1);

  vec3  rPos     = shInp.pos + dist*10.0*refr;
  vec4  rPosScr  = scene.viewProject*vec4(rPos,1.0);
  rPosScr.xyz /= rPosScr.w;
  vec2  p2       = rPosScr.xy*0.5+vec2(0.5);
  vec3  back     = texture(gbufferDiffuse,p2).rgb;
  float depth2   = texture(gbufferDepth,  p2).r;
  if(depth2<gl_FragCoord.z)
    back  = backOrig;
  //return vec4(p2,0,1);
  //return vec4(back,1);

  float transmittance = min(1.0 - exp(-0.95 * abs(dist)*5.0), 1.0);
  //return vec4(vec3(transmittance),1);

  back = mix(back.rgb,back.rgb*color.rgb,transmittance);
  //return vec4(back,1);

  vec3 clr = mix(back,sky,f);
  return vec4(clr,1);
  }
#endif

#if defined(GHOST)
vec3 ghostColor(vec3 selfColor) {
  vec4 back = texelFetch(gbufferDiffuse, ivec2(gl_FragCoord.xy), 0);
  return back.rgb+selfColor;
  }
#endif

#if defined(MAT_UV)
vec4 diffuseTex() {
#if defined(MAT_ANIM)
  const vec2 uv = shInp.uv + material.texAnim;
#else
  const vec2 uv = shInp.uv;
#endif
  return texture(textureD,uv);
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

#if !defined(DEPTH_ONLY)
  vec3  color = vec3(0);
  float alpha = 1;

#if defined(GHOST)
  color = ghostColor(t.rgb);
  alpha = t.a;
#else
  color = t.rgb;
  alpha = t.a;
#endif

#if defined(MAT_COLOR)
  color *= shInp.color.rgb;
  alpha *= shInp.color.a;
#endif

#if defined(LIGHT)
  color *= calcLight();
#endif

#if defined(WATER)
  {
    vec4 wclr = waterColor(color,vec3(0.8,0.9,1.0));
    color  = wclr.rgb;
    alpha  = wclr.a;
  }
#endif

#if !defined(DEPTH_ONLY)
  outColor      = vec4(color,alpha);
#endif

#ifdef GBUFFER
  outDiffuse    = t;
  outNormal     = vec4(normalize(shInp.normal)*0.5 + vec3(0.5),1.0);
  outDepth      = vec4(gl_FragCoord.z,0.0,0.0,0.0);
#endif

#if DEBUG_DRAW
  outColor = vec4(debugColors[debugId%MAX_DEBUG_COLORS],1.0);
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

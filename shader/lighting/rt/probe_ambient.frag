#version 460

#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive    : enable
#extension GL_EXT_control_flow_attributes : enable

#include "lighting/rt/probe_common.glsl"
#include "lighting/tonemapping.glsl"
#include "lighting/purkinje_shift.glsl"
#include "scene.glsl"
#include "common.glsl"

#define SSAO 1

const int   KERNEL_RADIUS = 1;
const float blurSharpness = 0.8;

layout(binding  = 0, std140) uniform UboScene {
  SceneDesc scene;
  };
layout(binding  = 1) uniform sampler2D probesLighting;
layout(binding  = 2) uniform sampler2D gbufDiffuse;
layout(binding  = 3) uniform sampler2D gbufNormal;
layout(binding  = 4) uniform sampler2D depth;
layout(binding  = 5) uniform sampler2D ssao;

layout(binding  = 6, std430) readonly buffer Hbo0 { uint hashTable[]; };
layout(binding  = 7, std430) readonly buffer Pbo  { ProbesHeader probeHeader; Probe probe[]; };

layout(location = 0) out vec4 outColor;

vec4 colorSum  = vec4(0);
bool err       = false;

vec3 unprojectDepth(const float z) {
  const ivec2 fragCoord  = ivec2(gl_FragCoord.xy);
  const vec2  screenSize = ivec2(textureSize(depth,0));

  const vec2  inPos      = vec2(2*fragCoord+ivec2(1,1))/vec2(screenSize)-vec2(1,1);
  const vec4  pos        = vec4(inPos.xy,z,1);
  const vec4  ret        = scene.viewProjectInv*pos;
  return (ret.xyz/ret.w);
  }

int probeGridComputeLod() {
  ivec2 fragCoord  = ivec2(gl_FragCoord.xy);
  ivec2 screenSize = textureSize(depth,0);

  // must be exactly same as in allocation !
  const float z = texelFetch(depth,fragCoord,0).x;
  return probeGridComputeLod(fragCoord, screenSize, z, scene.viewProjectInv);
  }

float texLinearDepth(vec2 uv) {
  float d = textureLod(depth, uv, 0).x;
  return linearDepth(d, scene.clipInfo);
  }

#if defined(SSAO)
float blurFunction(vec2 uv, float r, float centerD, inout float wTotal) {
  float c = textureLod(ssao, uv, 0).r;
  float d = texLinearDepth(uv);

  const float blurSigma   = float(KERNEL_RADIUS) * 0.5;
  const float blurFalloff = 1.0/(2.0*blurSigma*blurSigma);

  float ddiff = (d - centerD) * blurSharpness;
  float w     = exp2(-r*r*blurFalloff - ddiff*ddiff);
  wTotal += w;

  return c*w;
  }

float smoothSsao() {
  vec2  uv      = (gl_FragCoord.xy / vec2(textureSize(depth,0)));
  float centerC = textureLod(ssao, uv, 0).r;
  float centerD = texLinearDepth(uv);

  float cTotal  = centerC;
  float wTotal  = 1.0;

  vec2 invRes = vec2(1.0)/vec2(textureSize(ssao,0));
  for(float i=-KERNEL_RADIUS; i<=KERNEL_RADIUS; ++i)
    for(float r=-KERNEL_RADIUS; r<=KERNEL_RADIUS; ++r) {
      if((i==0 && r==0)) // || (abs(i)==KERNEL_RADIUS && abs(r)==KERNEL_RADIUS))
        continue;
      vec2 at = uv + invRes * vec2(i,r);
      cTotal += blurFunction(at, r, centerD, wTotal);
      }

  // return 0;
  return clamp(cTotal/wTotal, 0, 1);
  }
#else
float smoothSsao() { return 0; }
#endif

Probe mkZeroProbe() {
  Probe px;
  px.bits = UNUSED_BIT;
  return px;
  }

Probe readProbe(const ivec3 gridPos, const vec3 wpos, out uint id) {
  const uint h       = probeGridPosHash(gridPos) % hashTable.length();
  uint       probeId = hashTable[h];

  [[loop]]
  for(int i=0; i<8; ++i) {
    if(probeId>=probeHeader.count)
      return mkZeroProbe();

    const Probe p = probe[probeId];
    if((p.bits & UNUSED_BIT)!=0)
      err = true;

    if((p.bits & TRACED_BIT)==0) {
      probeId = p.pNext;
      continue;
      }

    if(!probeIsSame(p.pos, wpos, 0)) {
      probeId = p.pNext;
      continue;
      }
    id = probeId;
    return p;
    }

  return mkZeroProbe();
  }

void processProbe(ivec3 gridPos, vec3 wpos, int lod, vec3 pixelPos, vec3 pixelNorm) {
  const vec3 ldir = wpos-pixelPos;
  if(dot(ldir, pixelNorm)<=0)
    return;

  uint probeId = 0;
  const Probe p = readProbe(gridPos, wpos, probeId);
  if((p.bits & TRACED_BIT)==0)
    return;

  vec3 dp = wpos - p.pos;
  if(dot(dp,dp)>1.0) {
    // debug
    // colorSum += vec4(vec3(0,0,1)*scene.GSunIntensity,1);
    // return; // position is not what was expeted - hash collision
    }

  if(ivec3(p.pos/probeGridStep)!=gridPos) {
    // debug
    // colorSum += vec4(vec3(0,0,1)*scene.GSunIntensity,1);
    // return;
    }

  float weight = 1;
  // cosine weight from https://advances.realtimerendering.com/s2015/SIGGRAPH_2015_Remedy_Notes.pdf
  weight *= max(0.01, dot(normalize(ldir), pixelNorm));

  // wrap weight https://www.jcgt.org/published/0008/02/01/paper-lowres.pdf
  // weight *= max(0.0, dot(normalize(ldir), pixelNorm))*0.5 + 0.5;

  // distnace based weight
  weight *= 1.0/(dot(ldir,ldir) + 0.00001);
  if((p.bits & BAD_BIT)!=0) {
    weight *= 0.00001;
    // weight = min(weight, 0.00001);
    }

  colorSum.rgb += probeReadAmbient(probesLighting, probeId, pixelNorm, p.normal) * weight;
  colorSum.w   += weight;
  }

void gather(vec3 basePos, vec3 pos, vec3 norm, int lod) {
  colorSum = vec4(0);

  probeQuery pq;
  probeQueryInitialize(pq, pos, lod);
  while(probeQueryProceed(pq)) {
    vec3  wpos = probeQueryWorldPos(pq);
    ivec3 gPos = probeQueryGridPos(pq);
    processProbe(gPos, wpos, lod, basePos, norm);
    }
  }

void main() {
  const float minW = uintBitsToFloat(0x00000008);
  const float z    = texelFetch(depth,ivec2(gl_FragCoord.xy),0).x;
  if(z>=1.0)
    discard; // sky

  const vec3 diff = texelFetch(gbufDiffuse, ivec2(gl_FragCoord.xy), 0).rgb;
  const vec3 norm = normalize(texelFetch(gbufNormal,ivec2(gl_FragCoord.xy),0).xyz*2.0-vec3(1.0));

  const int  lod     = probeGridComputeLod();
  const vec3 basePos = unprojectDepth(z);
  const vec3 pos     = basePos + norm*probeCageBias;

  gather(basePos, pos, norm, lod);

  if(colorSum.w<=minW) {
    // debug
    // colorSum = vec4(vec3(1,0,0)*scene.GSunIntensity,1);
    colorSum.rgb = probeReadAmbient(probesLighting, 0, norm, vec3(0,1,0));
    colorSum.w   = 1;
    } else {
    colorSum.rgb = colorSum.rgb/max(colorSum.w, minW);
    }

  //const vec3  linear = vec3(1);
  const vec3  linear = textureLinear(diff); //  * Fd_Lambert is accounted in integration
  const float ao     = smoothSsao();

  vec3 color = colorSum.rgb;
  color *= linear;
  color *= (1-ao);
  // night shift
  color += purkinjeShift(color);
  color *= scene.exposure;
  outColor = vec4(color, 1);

  // outColor = vec4(srgbEncode(linear), 0);
  // outColor = vec4(0.001 * vec3(harmonicR.r/max(harmonicR.g, 0.000001)), 0);
  // if(err)
  //   outColor = vec4(1,0,0,0);
  }

#version 460

#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive    : enable
#extension GL_EXT_control_flow_attributes : enable

#include "lighting/rt/probe_common.glsl"
#include "lighting/tonemapping.glsl"
#include "lighting/purkinje_shift.glsl"
#include "scene.glsl"
#include "common.glsl"

layout(binding  = 0, std140) uniform UboScene {
  SceneDesc scene;
  };
layout(binding  = 1) uniform sampler2D gbufDiffuse;
layout(binding  = 2) uniform sampler2D gbufNormal;
layout(binding  = 3) uniform sampler2D depth;
layout(binding  = 4, std430) buffer Hbo0 { Hash hashTable[]; };
layout(binding  = 5, std430) readonly buffer Pbo { ProbesHeader probeHeader; Probe probe[]; };

layout(location = 0) out vec4 outColor;

vec4 colorSum = vec4(0);
bool err      = false;

vec3 unprojectDepth(const float z) {
  const vec2  screenSize = ivec2(textureSize(depth,0));
  const vec2  inPos      = vec2(2*gl_FragCoord.xy+ivec2(1,1))/vec2(screenSize)-vec2(1,1);
  const vec4  pos        = vec4(inPos.xy,z,1);
  const vec4  ret        = scene.viewProjectInv*pos;
  return (ret.xyz/ret.w);
  }

Probe mkZeroProbe() {
  Probe px;
  px.color[0][0] = vec4(0);
  px.color[0][1] = vec4(0);
  px.color[1][0] = vec4(0);
  px.color[1][1] = vec4(0);
  px.color[2][0] = vec4(0);
  px.color[2][1] = vec4(0);
  px.bits        = UNUSED_BIT;
  return px;
  }

Probe readProbe(const ivec3 gridPos, const vec3 wpos) {
  const uint h       = probeGridPosHash(gridPos) % hashTable.length();
  uint       probeId = hashTable[h].value;

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

    const vec3 dp = wpos - p.pos;
    if(dot(dp,dp)>1.0) {
      probeId = p.pNext;
      continue;
      }
    return p;
    }

  return mkZeroProbe();
  }

void processProbe(ivec3 gridPos, vec3 wpos, int lod, vec3 pixelPos, vec3 pixelNorm, bool ignoreBad) {
  const vec3 ldir = wpos-pixelPos;
  if(dot(ldir, pixelNorm)<=0)
    return;

  const Probe p = readProbe(gridPos, wpos);
  if((p.bits & TRACED_BIT)==0)
    return;

  if((p.bits & BAD_BIT)!=0 && !ignoreBad)
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
  // distnace based weight
  weight *= 1.0/max(length(ldir), 20);

  // weight *= 1.0/max(length(ldir), 0.00001);
  colorSum.rgb += probeReadAmbient(p, pixelNorm) * weight;
  colorSum.w   += weight;
  }

void gather(vec3 pos, vec3 norm, int lod, bool ignoreBad) {
  colorSum = vec4(0);

  probeQuery pq;
  probeQueryInitialize(pq, pos, lod);
  while(probeQueryProceed(pq)) {
    vec3  wpos = probeQueryWorldPos(pq);
    ivec3 gPos = probeQueryGridPos(pq);
    processProbe(gPos, wpos, lod, pos, norm, ignoreBad);
    }
  }

vec3 textureAlbedo(vec3 diff) {
  return textureLinear(diff) * PhotoLum;
  //return srgbDecode(diff.rgb);
  }

void main() {
  const float z = texelFetch(depth,ivec2(gl_FragCoord.xy),0).x;
  if(z>=0.99995)
    discard; // sky

  const vec3  diff = texelFetch(gbufDiffuse, ivec2(gl_FragCoord.xy), 0).rgb;
  const vec3  norm = normalize(texelFetch(gbufNormal,ivec2(gl_FragCoord.xy),0).xyz*2.0-vec3(1.0));

  const float dist = linearDepth(z, scene.clipInfo);
  const vec3  pos  = unprojectDepth(z) + norm*probeCageBias;
  const int   lod  = probeGridLodFromDist(dist);

  gather(pos, norm, lod, false);
  if(colorSum.w<=0.000001) {
    // gather(pos, norm, lod, true);
    }

  if(colorSum.w<=0.000001) {
    // debug
    // colorSum = vec4(vec3(1,0,0)*scene.GSunIntensity,1);
    colorSum = vec4(0,0,0,1);
    }

  const vec3 linear = textureAlbedo(diff);

  vec3 color = colorSum.rgb/max(colorSum.w,0.000001);
  color *= linear;
  // night shift
  color += purkinjeShift(color);
  color *= scene.exposure;
  // color = linear;
  outColor = vec4(color, 1);

  if(err)
    outColor = vec4(1,0,0,0);
  }

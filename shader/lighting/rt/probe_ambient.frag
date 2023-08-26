#version 460

#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive    : enable
#extension GL_EXT_control_flow_attributes : enable

#include "lighting/rt/probe_common.glsl"
#include "lighting/tonemapping.glsl"
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

vec3 unprojectDepth(const float z) {
  const vec2  screenSize = ivec2(textureSize(depth,0));
  const vec2  inPos      = vec2(2*gl_FragCoord.xy+ivec2(1,1))/vec2(screenSize)-vec2(1,1);
  const vec4  pos        = vec4(inPos.xy,z,1);
  const vec4  ret        = scene.viewProjectInv*pos;
  return (ret.xyz/ret.w);
  }

void processProbe(ivec3 gridPos, vec3 pos, int lod, vec3 pixelPos, vec3 pixelNorm, bool ignoreBad) {
  const uint h      = probeGridPosHash(gridPos) % hashTable.length();
  const uint cursor = hashTable[h].value;
  if(cursor>=probeHeader.count)
    return;

  const Probe p    = probe[cursor];
  const vec3  ldir = p.pos-pixelPos;

  if((p.bits & BAD_BIT)!=0 && !ignoreBad)
    return;
  if(dot(ldir, pixelNorm)<=0)
    return;

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
  probeQuery pq;
  probeQueryInitialize(pq, pos, lod);
  while(probeQueryProceed(pq)) {
    vec3 wpos = probeQueryWorldPos(pq);
    vec3 dir  = (wpos-pos);

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

  const float bias = 2.0;
  const float dist = linearDepth(z, scene.clipInfo);
  const vec3  pos  = unprojectDepth(z) + norm*bias;
  const int   lod  = probeGridLodFromDist(dist);

  gather(pos, norm, lod, false);
  if(colorSum.w<=0.000001) {
    // maybe all probes do have bad-bit, check one LOD above for data
    gather(pos, norm, lod, true);
    }

  const vec3 linear = textureAlbedo(diff);

  vec3 color = colorSum.rgb/max(colorSum.w,0.000001);
  color *= linear;
  color *= scene.exposure;
  // color = linear;
  outColor = vec4(color, 1);
  }

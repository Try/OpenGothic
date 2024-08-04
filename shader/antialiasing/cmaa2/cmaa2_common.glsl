#ifndef CMAA2_COMMON_GLSL
#define CMAA2_COMMON_GLSL

#extension GL_EXT_samplerless_texture_functions : enable

#include "common.glsl"

#define CMAA2_PROCESS_CANDIDATES_NUM_THREADS        128
#define CMAA2_SUPPORT_HDR_COLOR_RANGE               0
#define CMAA2_EDGE_DETECTION_LUMA_PATH              0       // We should use HDR luma from a separate buffer in the future
#define CMAA2_EXTRA_SHARPNESS                       0

struct DispatchIndirectCommand {
  uint  x;
  uint  y;
  uint  z;
  };

struct DrawIndirectCommand {
  uint  vertexCount;
  uint  instanceCount;
  uint  firstVertex;
  uint  firstInstance;
  };

const float symmetryCorrectionOffset = 0.22;

#if CMAA2_EXTRA_SHARPNESS
const float dampeningEffect = 0.11;
const float cmaa2LocalContrastAdaptationAmount = 0.15f
const float cmaa2SimpleShapeBlurinessAmount = 0.07f
#else
const float dampeningEffect = 0.15;
const float cmaa2LocalContrastAdaptationAmount =  0.10f;
const float cmaa2SimpleShapeBlurinessAmount = 0.10f;
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// VARIOUS QUALITY SETTINGS
//
// Longest line search distance; must be even number; for high perf low quality start from ~32 - the bigger the number, 
// the nicer the gradients but more costly. Max supported is 128!
const uint maxLineLength = 86;
// 

layout(binding = 0)     uniform texture2D sceneTonemapped;
layout(binding = 1, r8) uniform image2D   workingEdges;

layout(binding = 2) buffer UboWorkingShapeCandidates {
  uint shapeCandidates[];
  };

layout(binding = 3) buffer UboWorkingDeferredBlendLocationList {
  uint deferredBlendLocationList[];
  };

layout(binding = 4) buffer UboWorkingDeferredBlendItemList {
  uvec2 deferredBlendItemList[];
  };

layout(binding = 5, r32ui) uniform uimage2D deferredBlendItemListHeads;

layout(binding = 6) buffer UboWorkingControlBuffer {
  uint iterator;
  uint shapeCandidateCount;
  uint blendColorSamplesCount;
  uint blendLocationCount;
  uint subsequentPassWorkloadSize;
  } controlBuffer;


vec4 unpackEdgesFlt(uint value) {
  vec4 ret;
  ret.x = float((value & 0x01) != 0);
  ret.y = float((value & 0x02) != 0);
  ret.z = float((value & 0x04) != 0);
  ret.w = float((value & 0x08) != 0);
  return ret;
  }

vec3 loadSourceColor(ivec2 pixelPos, ivec2 offset) {
  vec3 color = texelFetch(sceneTonemapped, pixelPos + offset, 0).xyz;
  return color;
  }

void storeEdge(uvec2 pos, uvec4 outEdges) {
  imageStore(workingEdges, ivec2(pos.x / 2, pos.y + 0), vec4(((outEdges[1] << 4) | outEdges[0]) / 255.0));
  imageStore(workingEdges, ivec2(pos.x / 2, pos.y + 1), vec4(((outEdges[3] << 4) | outEdges[2]) / 255.0));
  }

uint loadEdge(ivec2 pixelPos, ivec2 offset) {
  uint a    = uint((pixelPos.x + offset.x) % 2);
  uint edge = uint(imageLoad(workingEdges, ivec2((pixelPos.x + offset.x) / 2, pixelPos.y + offset.y)).x * 255. + 0.5);
  edge = (edge >> (a * 4)) & 0xF;
  return edge;
  }

vec3 internalUnpackColor(uint packedColor) {
#if CMAA2_SUPPORT_HDR_COLOR_RANGE
  return unpackR11G11B10F(packedColor);
#else
  return unpackUnorm4x8(packedColor).rgb;
  // return unpackR11G11B10E4F(packedColor);
#endif
  }

uint internalPackColor(vec3 color) {
#if CMAA2_SUPPORT_HDR_COLOR_RANGE
  return packR11G11B10F(color);
#else
  return packUnorm4x8(vec4(color,0));
  // return packR11G11B10E4F(color);
#endif
  }

#endif

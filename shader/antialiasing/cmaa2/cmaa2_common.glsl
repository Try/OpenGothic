#ifndef CMAA2_COMMON_GLSL
#define CMAA2_COMMON_GLSL

#extension GL_EXT_samplerless_texture_functions : enable

#include "common.glsl"

#define CMAA_PACK_SINGLE_SAMPLE_EDGE_TO_HALF_WIDTH  1
#define CMAA2_PROCESS_CANDIDATES_NUM_THREADS        128
#define CMAA2_USE_HALF_FLOAT_PRECISION              0
#define CMAA2_SUPPORT_HDR_COLOR_RANGE               0
#define CMAA2_EDGE_DETECTION_LUMA_PATH              0       // We should use HDR luma from a separate buffer in the future
#define CMAA2_EDGE_UNORM                            0
#define CMAA2_EXTRA_SHARPNESS                       0

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

layout(binding = 0) uniform texture2D sceneTonemapped;

#if CMAA2_EDGE_UNORM 
layout(binding = 2, r8)    uniform image2D workingEdges;
#else
layout(binding = 2, r32ui) uniform uimage2D workingEdges;
#endif

layout(binding = 3) buffer UboWorkingShapeCandidates {
  uint shapeCandidates[];
  };

layout(binding = 4) buffer UboWorkingDeferredBlendLocationList {
  uint deferredBlendLocationList[];
  };

layout(binding = 5) buffer UboWorkingDeferredBlendItemList {
  uvec2 deferredBlendItemList[];
  };

layout(binding = 6, r32ui) uniform uimage2D deferredBlendItemListHeads;

layout(binding = 7) buffer UboWorkingControlBuffer {
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

vec3 internalUnpackColor(uint packedColor) {
#if CMAA2_SUPPORT_HDR_COLOR_RANGE
  return unpackR11G11B10F(packedColor);
#else
  return unpackR11G11B10E4F(packedColor);
#endif
  }

uint internalPackColor(vec3 color) {
#if CMAA2_SUPPORT_HDR_COLOR_RANGE
  return packR11G11B10F(color);
#else
  return packR11G11B10E4F(color);
#endif
  }

#endif

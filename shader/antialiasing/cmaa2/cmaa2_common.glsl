#define CMAA_PACK_SINGLE_SAMPLE_EDGE_TO_HALF_WIDTH  1
#define CMAA2_PROCESS_CANDIDATES_NUM_THREADS        128
#define CMAA2_DEFERRED_APPLY_NUM_THREADS            32
#define CMAA2_DEFERRED_APPLY_THREADGROUP_SWAP       1 
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

layout(binding = 0) uniform sampler2D sceneTonemapped;

#if CMAA2_EDGE_UNORM 
  layout(r8, binding = 2) uniform image2D workingEdges;
#else
  layout(r32ui, binding = 2) uniform uimage2D workingEdges;
#endif

layout(binding = 3) buffer UboWorkingShapeCandidates {
  uint workingShapeCandidates[];
  };

layout(binding = 4) buffer UboWorkingDeferredBlendLocationList {
  uint workingDeferredBlendLocationList[];
  };

layout(binding = 5) buffer UboWorkingDeferredBlendItemList {
  uvec2 workingDeferredBlendItemList[];
  };

layout(r32ui, binding = 6) uniform uimage2D workingDeferredBlendItemListHeads;

layout(binding = 7) buffer UboWorkingControlBuffer {
  uint workingControlBuffer[];
  };

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

uint packHalf1x16(float arg) {
  return packHalf2x16(vec2(arg, 0.));
  }

float unpackUintAsFloat16AndConvertToFloat32(uint arg) {
  return unpackHalf2x16(arg).x;
  }

// the next packing/unpacking methods are taken from
// https://github.com/Microsoft/DirectX-Graphics-Samples/blob/master/MiniEngine/Core/Shaders/PixelPacking_R11G11B10.hlsli

uint packR11G11B10F(vec3 rgb) {
  rgb = min(rgb, uintBitsToFloat(0x477C0000));
  uint r = ((packHalf1x16(rgb.r) + 8) >> 4) & 0x000007FF;
  uint g = ((packHalf1x16(rgb.g) + 8) << 7) & 0x003FF800;
  uint b = ((packHalf1x16(rgb.b) + 16) << 17) & 0xFFC00000;
  return r | g | b;
  }

vec3 unpackR11G11B10F(uint rgb) {
  float r = unpackUintAsFloat16AndConvertToFloat32((rgb << 4) & 0x7FF0);
  float g = unpackUintAsFloat16AndConvertToFloat32((rgb >> 7) & 0x7FF0);
  float b = unpackUintAsFloat16AndConvertToFloat32((rgb >> 17) & 0x7FE0);
  return vec3(r, g, b);
  }

// This is like R11G11B10F except that it moves one bit from each exponent to each mantissa.
uint packR11G11B10E4F(vec3 rgb) {
  rgb = clamp(rgb, 0.0, uintBitsToFloat(0x3FFFFFFF));
  uint r = ((packHalf1x16(rgb.r) + 4) >> 3) & 0x000007FF;
  uint g = ((packHalf1x16(rgb.g) + 4) << 8) & 0x003FF800;
  uint b = ((packHalf1x16(rgb.b) + 8) << 18) & 0xFFC00000;
  return r | g | b;
  }

vec3 unpackR11G11B10E4F(uint rgb) {
  float r = unpackUintAsFloat16AndConvertToFloat32((rgb << 3) & 0x3FF8);
  float g = unpackUintAsFloat16AndConvertToFloat32((rgb >> 8) & 0x3FF8);
  float b = unpackUintAsFloat16AndConvertToFloat32((rgb >> 18) & 0x3FF0);
  return vec3(r, g, b);
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

uint packR10G10B10A2Unorm(vec4 unpackedInput) {
  return ((uint(clamp(unpackedInput.x, 0.0, 1.0) * 1023 + 0.5)) |
    (uint(clamp(unpackedInput.y, 0.0, 1.0) * 1023 + 0.5) << 10) |
    (uint(clamp(unpackedInput.z, 0.0, 1.0) * 1023 + 0.5) << 20) |
    (uint(clamp(unpackedInput.w, 0.0, 1.0) * 3 + 0.5) << 30));
  }

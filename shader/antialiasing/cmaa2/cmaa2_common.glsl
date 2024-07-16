// create several presets for defines in shader setup
#define CMAA_PACK_SINGLE_SAMPLE_EDGE_TO_HALF_WIDTH  1
#define CMAA2_PROCESS_CANDIDATES_NUM_THREADS        128
#define CMAA2_DEFERRED_APPLY_NUM_THREADS            32
#define CMAA2_DEFERRED_APPLY_THREADGROUP_SWAP       1 
#define CMAA2_USE_HALF_FLOAT_PRECISION              0
#define CMAA2_SUPPORT_HDR_COLOR_RANGE               0
#define CMAA2_EDGE_DETECTION_LUMA_PATH              0       // We should use HDR luma from a separate buffer in the future
#define CMAA_MSAA_SAMPLE_COUNT                      1       // for now force mode without MSAA
#define CMAA_MSAA_USE_COMPLEXITY_MASK               1
#define CMAA2_EDGE_UNORM                            0
#define CMAA2_EXTRA_SHARPNESS                       0

const float c_symmetryCorrectionOffset = 0.22;

#if CMAA2_EXTRA_SHARPNESS
  const float c_dampeningEffect = 0.11;
  #define g_CMAA2_LocalContrastAdaptationAmount       0.15f
  #define g_CMAA2_SimpleShapeBlurinessAmount          0.07f
#else
  const float c_dampeningEffect = 0.15;
  #define g_CMAA2_LocalContrastAdaptationAmount       0.10f
  #define g_CMAA2_SimpleShapeBlurinessAmount          0.10f
#endif

#if CMAA_MSAA_SAMPLE_COUNT > 1
  #define CMAA_MSAA_USE_COMPLEXITY_MASK 1
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// VARIOUS QUALITY SETTINGS
//
// Longest line search distance; must be even number; for high perf low quality start from ~32 - the bigger the number, 
// the nicer the gradients but more costly. Max supported is 128!
const uint c_maxLineLength = 86;
// 

#if CMAA_MSAA_SAMPLE_COUNT > 1
  layout(binding = 1) uniform sampler2DArray g_inColorMSReadonly;
  layout(binding = 0) uniform sampler2D g_inColorMSComplexityMaskReadonly;
#else
  layout(binding = 0) uniform sampler2D g_inoutColorReadonly;
#endif 

#if CMAA2_EDGE_UNORM 
  layout(r8, binding = 2) uniform image2D g_workingEdges;
#else
  layout(r32ui, binding = 2) uniform uimage2D g_workingEdges;
#endif

layout(binding = 3) buffer UboWorkingShapeCandidates {
  uint g_workingShapeCandidates[];
  };

layout(binding = 4) buffer UboWorkingDeferredBlendLocationList {
  uint g_workingDeferredBlendLocationList[];
  };

layout(binding = 5) buffer UboWorkingDeferredBlendItemList {
  uvec2 g_workingDeferredBlendItemList[];
  };

layout(r32ui, binding = 6) uniform uimage2D g_workingDeferredBlendItemListHeads;

layout(binding = 7) buffer UboWorkingControlBuffer {
  uint g_workingControlBuffer[];
  };

#define CmaaSaturate(x) clamp(x, 0.0, 1.0) 

uvec4 UnpackEdges(uint value) {
  uvec4 ret;
  ret.x = uint((value & 0x01) != 0);
  ret.y = uint((value & 0x02) != 0);
  ret.z = uint((value & 0x04) != 0);
  ret.w = uint((value & 0x08) != 0);
  return ret;
  }

vec4 UnpackEdgesFlt(uint value) {
  vec4 ret;
  ret.x = float((value & 0x01) != 0);
  ret.y = float((value & 0x02) != 0);
  ret.z = float((value & 0x04) != 0);
  ret.w = float((value & 0x08) != 0);
  return ret;
  }

vec3 LoadSourceColor(ivec2 pixelPos, ivec2 offset, int sampleIndex) {
#if CMAA_MSAA_SAMPLE_COUNT > 1
  vec3 color = texelFetch(g_inColorMSReadonly, ivec3(pixelPos + offset, sampleIndex), 0).xyz;
#else
  vec3 color = texelFetch(g_inoutColorReadonly, pixelPos + offset, 0).xyz;
#endif 
  return color;
  }

uint Pack_R11G11B10_FLOAT(vec3 rgb) {
  rgb = min(rgb, uintBitsToFloat(0x477C0000));
  uint r = ((floatBitsToUint(rgb.x) + 8) >> 4) & 0x000007FF;
  uint g = ((floatBitsToUint(rgb.y) + 8) << 7) & 0x003FF800;
  uint b = ((floatBitsToUint(rgb.z) + 16) << 17) & 0xFFC00000;
  return r | g | b;
  }

vec3 Unpack_R11G11B10_FLOAT(uint rgb) {
  float r = uintBitsToFloat((rgb << 4) & 0x7FF0);
  float g = uintBitsToFloat((rgb >> 7) & 0x7FF0);
  float b = uintBitsToFloat((rgb >> 17) & 0x7FE0);
  return vec3(r, g, b);
  }

uint PackFloat32AsFloat16AndConvertToUint(float arg) {
  return packHalf2x16(vec2(arg, 0.));
  }

float UnpackUintAsFloat16AndConvertToFloat32(uint arg) {
  return unpackHalf2x16(arg).x;
  }

uint Pack_R11G11B10_E4_FLOAT(vec3 rgb) {
  rgb = clamp(rgb, 0.0, uintBitsToFloat(0x3FFFFFFF));
  uint r = ((PackFloat32AsFloat16AndConvertToUint(rgb.r) + 4) >> 3) & 0x000007FF;
  uint g = ((PackFloat32AsFloat16AndConvertToUint(rgb.g) + 4) << 8) & 0x003FF800;
  uint b = ((PackFloat32AsFloat16AndConvertToUint(rgb.b) + 8) << 18) & 0xFFC00000;
  return r | g | b;
  }

vec3 Unpack_R11G11B10_E4_FLOAT(uint rgb) {
  float r = UnpackUintAsFloat16AndConvertToFloat32((rgb << 3) & 0x3FF8);
  float g = UnpackUintAsFloat16AndConvertToFloat32((rgb >> 8) & 0x3FF8);
  float b = UnpackUintAsFloat16AndConvertToFloat32((rgb >> 18) & 0x3FF0);
  return vec3(r, g, b);
  }

vec3 InternalUnpackColor(uint packedColor) {
#if CMAA2_SUPPORT_HDR_COLOR_RANGE
  return Unpack_R11G11B10_FLOAT(packedColor);
#else
  return Unpack_R11G11B10_E4_FLOAT(packedColor);
#endif
  }

uint InternalPackColor(vec3 color) {
#if CMAA2_SUPPORT_HDR_COLOR_RANGE
  return Pack_R11G11B10_FLOAT(color);
#else
  return Pack_R11G11B10_E4_FLOAT(color);
#endif
  }

float LINEAR_to_SRGB(float val) {
  if (val < 0.0031308) val *= 12.92;
  else val = 1.055 * pow(abs(val), 1.0 / 2.4) - 0.055;
  return val;
  }

vec3 LINEAR_to_SRGB(vec3 val) {
  return vec3(LINEAR_to_SRGB(val.x), LINEAR_to_SRGB(val.y), LINEAR_to_SRGB(val.z));
  }

uint FLOAT4_to_R8G8B8A8_UNORM(vec4 unpackedInput) {
  return ((uint(clamp(unpackedInput.x, 0.0, 1.0) * 255 + 0.5)) |
    (uint(clamp(unpackedInput.y, 0.0, 1.0) * 255 + 0.5) << 8) |
    (uint(clamp(unpackedInput.z, 0.0, 1.0) * 255 + 0.5) << 16) |
    (uint(clamp(unpackedInput.w, 0.0, 1.0) * 255 + 0.5) << 24));
  }

uint FLOAT4_to_R10G10B10A2_UNORM(vec4 unpackedInput) {
  return ((uint(clamp(unpackedInput.x, 0.0, 1.0) * 1023 + 0.5)) |
    (uint(clamp(unpackedInput.y, 0.0, 1.0) * 1023 + 0.5) << 10) |
    (uint(clamp(unpackedInput.z, 0.0, 1.0) * 1023 + 0.5) << 20) |
    (uint(clamp(unpackedInput.w, 0.0, 1.0) * 3 + 0.5) << 30));
  }

uint LoadEdge(ivec2 pixelPos, ivec2 offset, uint msaaSampleIndex) {
#if CMAA_MSAA_SAMPLE_COUNT > 1
  uint edge = imageLoad(g_workingEdges, pixelPos + offset).x;
  edge = (edge >> (msaaSampleIndex * 4)) & 0xF;
#else
  #if defined(CMAA_PACK_SINGLE_SAMPLE_EDGE_TO_HALF_WIDTH)
    uint a = uint((pixelPos.x + offset.x) % 2);

    #if CMAA2_EDGE_UNORM
      uint edge = uint(imageLoad(g_workingEdges, ivec2((pixelPos.x + offset.x) / 2, pixelPos.y + offset.y)).x * 255. + 0.5);
    #else
      uint edge = imageLoad(g_workingEdges, ivec2((pixelPos.x + offset.x) / 2, pixelPos.y + offset.y)).x;
    #endif

    edge = (edge >> (a * 4)) & 0xF;
  #else
    uint edge = imageLoad(g_workingEdges, ivec2(pixelPos + offset)).x;
  #endif
#endif
  return edge;
  }

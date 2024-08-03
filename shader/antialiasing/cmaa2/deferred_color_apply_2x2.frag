#version 460

#extension GL_ARB_separate_shader_objects       : enable
#extension GL_GOOGLE_include_directive          : enable
#extension GL_EXT_control_flow_attributes       : enable

#include "common.glsl"
#include "cmaa2_common.glsl"

layout(location = 0) out vec4 result;

layout(location = 0) in flat uint  currentQuadOffsetXY;
layout(location = 1) in flat uint  inCounterIndexWithHeader;

void main() {
  uint counterIndexWithHeader = inCounterIndexWithHeader;

  vec4 outColors = vec4(0);
  const uint maxLoops = 32u;
  for(uint i = 0; (counterIndexWithHeader != 0xFFFFFFFF) && (i < maxLoops); i++) {
    uint  offsetXY       = (counterIndexWithHeader >> 30) & 0x03;
    bool  isComplexShape = bool((counterIndexWithHeader >> 26) & 0x01);
    uvec2 val            = workingDeferredBlendItemList[counterIndexWithHeader & ((1u << 26) - 1)];

    counterIndexWithHeader = val.x;

    if(offsetXY == currentQuadOffsetXY) {
      vec3  color  = internalUnpackColor(val.y);
      float weight = 0.8 + 1.0 * float(isComplexShape);
      outColors += vec4(color * weight, weight);
      }
    }

  if(outColors.a == 0)
    discard;

  result = vec4(outColors.rgb/outColors.a, 1);
  }

#version 460

#extension GL_ARB_separate_shader_objects       : enable
#extension GL_GOOGLE_include_directive          : enable
#extension GL_EXT_control_flow_attributes       : enable

#include "common.glsl"
#include "cmaa2_common.glsl"

out gl_PerVertex {
  vec4 gl_Position;
  float gl_PointSize;
  };

layout(location = 0) out flat uint  currentQuadOffsetXY;
layout(location = 1) out flat uint  counterIndexWithHeader;

void main() {
  const uint  currentCandidate = gl_VertexIndex/4;
  currentQuadOffsetXY          = gl_VertexIndex%4;

  const ivec2 viewportSize = textureSize(sceneTonemapped, 0);
  const uint  pixelID      = deferredBlendLocationList[currentCandidate];
  const ivec2 quadPos      = ivec2((pixelID >> 16), pixelID & 0xFFFF);
  const ivec2 qeOffsets[4] = ivec2[4](ivec2(0, 0), ivec2(1, 0), ivec2(0, 1), ivec2(1, 1));
  const uvec2 pixelPos     = quadPos * 2 + qeOffsets[currentQuadOffsetXY];

  counterIndexWithHeader = imageLoad(deferredBlendItemListHeads, quadPos).r;
  gl_Position            = vec4((vec2(pixelPos+0.5)/vec2(viewportSize))*2.0-1.0, 0, 1);
  gl_PointSize           = 1;
  }

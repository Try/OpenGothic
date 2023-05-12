#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0) uniform sampler2D src;
layout(binding = 1) uniform sampler2D zs;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outDepth;

void main() {
  outColor = texelFetch(src, ivec2(gl_FragCoord.xy), 0);
  outDepth = texelFetch(zs,  ivec2(gl_FragCoord.xy), 0);
  }

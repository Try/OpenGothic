#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_samplerless_texture_functions : enable

//layout(binding = 0) uniform utexture2D visBuffer;
layout(binding = 0, r32ui) uniform uimage2D visBuffer;

layout(location = 0) out vec4 outColor;

vec3 upackColor(uint v) {
  uint r = (v >> 11u) & 0x1F;
  uint g = (v >> 5u)  & 0x3F;
  uint b = (v)        & 0x1F;
  return vec3(r, g, b)/vec3(31,63,31);
  }

void main() {
  uint v  = imageLoad(visBuffer, ivec2(gl_FragCoord.xy)).r;
  vec3 cl = upackColor(v & 0xFFFF);
  imageStore(visBuffer, ivec2(gl_FragCoord.xy), uvec4(0));

  outColor = (v>0 ? vec4(cl,1) : vec4(0));
  }

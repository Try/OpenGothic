#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(push_constant, std140) uniform PushConstant {
  vec3 ambient;
  } ubo;

layout(binding  = 0) uniform sampler2D lightingBuf;
layout(binding  = 1) uniform sampler2D diffuse;
layout(binding  = 2) uniform sampler2D ssao;

layout(location = 0) in  vec2 uv;
layout(location = 0) out vec4 outColor;

void main() {
  vec4  lbuf    = textureLod(lightingBuf,uv,0);
  vec3  clr     = textureLod(diffuse,    uv,0).rgb;
  float occ     = textureLod(ssao,       uv,0).r;
  vec3  ambient = ubo.ambient;

  // outColor = vec4(1-occ);
  outColor = vec4(lbuf.rgb-clr*ambient*occ, lbuf.a);
  }

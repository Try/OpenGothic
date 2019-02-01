#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 1) uniform sampler2D textureD;

layout(binding = 0) uniform UniformBufferObject {
  mat4 mvp;
  vec2 dxy;
  } ubo;

layout(location = 0) in vec2 inPos;
layout(location = 0) out vec4 outColor;

void main() {
  vec4 v = ubo.mvp*vec4(inPos,1.0,1.0);
  v.xyz/=v.w;
  vec3 view = v.xyz/max(abs(v.y),0.001);//normalize(v.xyz);

  outColor = texture(textureD,view.xz*0.2+ubo.dxy);
  }

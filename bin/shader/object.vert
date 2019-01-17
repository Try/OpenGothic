#version 450
#extension GL_ARB_separate_shader_objects : enable

out gl_PerVertex {
  vec4 gl_Position;
  };

layout(binding = 0) uniform UniformBufferObject {
  mat4 mvp;
  } ubo;

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec4 inColor;

layout(location = 0) out vec2 outUV;
layout(location = 1) out vec3 outNormmal;
layout(location = 2) out vec4 outColor;

void main() {
  outNormmal  = inNormal;
  outUV       = inUV;
  outColor    = inColor;

  gl_Position = ubo.mvp*vec4(inPos.xyz, 1.0);
  }

#version 450

#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUv;
layout(location = 3) in uint inColor;

layout(push_constant, std430) uniform Push {
  mat4 model;
  mat4 viewProject;
  vec4 params;
  } push;

out gl_PerVertex {
  vec4 gl_Position;
  };

void main() {
  vec4 clip = push.viewProject * push.model * vec4(inPosition,1.0);

  // A minute bias toward the camera makes an equal-depth replay of the
  // selected mesh robust against floating-point differences. Occluding scene
  // geometry still wins the regular LEqual depth test.
  clip.z -= push.params.x * clip.w;
  gl_Position = clip;
  }

#version 450
#extension GL_ARB_separate_shader_objects : enable

out gl_PerVertex {
  vec4 gl_Position;
  };

layout(std140,binding = 0) uniform UboScene {
  vec3 ldir;
  mat4 mv;
  } scene;

layout(std140,binding = 1) uniform UboObject {
  mat4 obj;
  mat4 skel[96];
  } ubo;

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec4 inColor;
layout(location = 3) in vec3 inPos;

layout(location = 0) out vec2 outUV;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec4 outColor;

layout(location = 3) out vec3 outLight;

void main() {
  outUV     = inUV;
  outColor  = inColor;
  outLight  = scene.ldir;

  outNormal   = (ubo.obj*vec4(inNormal,0.0)).xyz;
  gl_Position = scene.mv*ubo.obj*vec4(inPos.xyz, 1.0);
  }

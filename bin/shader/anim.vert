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

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec4 inId;
layout(location = 4) in vec4 inWeight;

layout(location = 0) out vec2 outUV;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec4 outColor;

layout(location = 3) out vec3 outLight;

void main() {
  outUV     = inUV;
  outColor  = vec4(1.0);
  outLight  = scene.ldir;

  vec4 pos = vec4(inPos,1.0);
  vec4 t0  = ubo.skel[int(inId.x)]*pos;
  vec4 t1  = ubo.skel[int(inId.y)]*pos;
  vec4 t2  = ubo.skel[int(inId.z)]*pos;
  vec4 t3  = ubo.skel[int(inId.w)]*pos;

  pos   = t0*inWeight.x + t1*inWeight.y + t2*inWeight.z + t3*inWeight.w;
  //pos   = vec4(inPos,1.0);
  pos.w = 1.0;

  outNormal   = (ubo.obj*vec4(inNormal,0.0)).xyz;
  gl_Position = scene.mv*ubo.obj*pos;
  }

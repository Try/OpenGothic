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
layout(location = 3) in vec3 inPos0;
layout(location = 4) in vec3 inPos1;
layout(location = 5) in vec3 inPos2;
layout(location = 6) in vec3 inPos3;
layout(location = 7) in vec4 inId;
layout(location = 8) in vec4 inWeight;

layout(location = 0) out vec2 outUV;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec4 outColor;

layout(location = 3) out vec3 outLight;

void main() {
  outUV     = inUV;
  outColor  = vec4(1.0);
  outLight  = scene.ldir;

  vec4 pos0 = vec4(inPos0,1.0);
  vec4 pos1 = vec4(inPos1,1.0);
  vec4 pos2 = vec4(inPos2,1.0);
  vec4 pos3 = vec4(inPos3,1.0);
  vec4 norm = vec4(inNormal,0.0);

  vec4 t0 = ubo.skel[int(inId.x*255.0)]*pos0;
  vec4 t1 = ubo.skel[int(inId.y*255.0)]*pos1;
  vec4 t2 = ubo.skel[int(inId.z*255.0)]*pos2;
  vec4 t3 = ubo.skel[int(inId.w*255.0)]*pos3;

  vec4 n0 = ubo.skel[int(inId.x)]*norm;
  vec4 n1 = ubo.skel[int(inId.y)]*norm;
  vec4 n2 = ubo.skel[int(inId.z)]*norm;
  vec4 n3 = ubo.skel[int(inId.w)]*norm;

  vec4 pos = t0*inWeight.x + t1*inWeight.y + t2*inWeight.z + t3*inWeight.w;
  norm     = n0*inWeight.x + n1*inWeight.y + n2*inWeight.z + n3*inWeight.w;

  outNormal   = (ubo.obj*norm).xyz;
  gl_Position = scene.mv*ubo.obj*pos;
  }

#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive    : enable

out gl_PerVertex {
  vec4 gl_Position;
  };

layout(push_constant, std430) uniform UboPush {
  vec2  sunPos;
  vec2  sunSz;
  float GSunIntensity;
  uint  isSun;
  } push;

layout(location = 0) out vec2 uv;

const vec2 vert[] = {
   {-1,-1},{ 1,1},{1,-1},
   {-1,-1},{-1,1},{1, 1}
};

void main() {
  const bool isSun = push.isSun!=0;

  vec2 v = vert[gl_VertexIndex];
  uv = v*0.5 + vec2(0.5);

  float scale = 1.0 - (isSun ? length(push.sunPos)*0.25 : 0);
  vec2  pos   = v*(push.sunSz*scale) + push.sunPos;

  gl_Position = vec4(pos, 1.0, 1.0);
  }

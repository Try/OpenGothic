#version 450
#extension GL_ARB_separate_shader_objects : enable

out gl_PerVertex {
  vec4 gl_Position;
  };

layout(std140,push_constant) uniform UboPush {
  mat4  mvp;
  mat4  mvpInv;
  } push;

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec4 inCentral;
layout(location = 2) in vec3 inColor;

layout(location = 0) out vec4 scrPosition;
layout(location = 1) out vec4 cenPosition;
layout(location = 2) out vec3 color;

void main(void) {
  vec4 pos = push.mvp*vec4(inPos,1.0);

  vec4 cen = push.mvp*vec4(inCentral.xyz,1.0);
  cen.xyz /= cen.w;

  vec4 closeup = push.mvpInv*vec4(0.0,0.0,0.0,1.0);
  closeup.xyz /= closeup.w;
  vec3 dpos = (closeup.xyz-inCentral.xyz);

  if(dot(dpos,dpos)<=pow(inCentral.w+1000.0,2.0)) {
    // transform close lights into FSQ
    vec3 fsq = (inPos-inCentral.xyz)/inCentral.w;
    pos = vec4(fsq.xy,0.0,1.0);
    }

  gl_Position = pos;
  scrPosition = pos;
  cenPosition = inCentral;
  color       = inColor;
  }

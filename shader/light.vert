#version 450
#extension GL_ARB_separate_shader_objects : enable

out gl_PerVertex {
  vec4 gl_Position;
  };

layout(std140,binding = 3) uniform Ubo {
  mat4  mvp;
  mat4  mvpInv;
  vec4  fr[6];
  } ubo;

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec4 inCentral;
layout(location = 2) in vec3 inColor;

layout(location = 0) out vec4 scrPosition;
layout(location = 1) out vec4 cenPosition;
layout(location = 2) out vec3 color;

vec3 v[8] = {
  {-1,-1,-1},
  { 1,-1,-1},
  { 1, 1,-1},
  {-1, 1,-1},

  {-1,-1, 1},
  { 1,-1, 1},
  { 1, 1, 1},
  {-1, 1, 1},
  };

bool testFrustrum(in vec3 at, in float R){
  for(int i=0; i<6; i++) {
    if(dot(ubo.fr[i],vec4(at,1.0))<-R)
      return false;
    }
  return true;
  }

void main(void) {
  if(!testFrustrum(inCentral.xyz,inCentral.w)) {
    // skip invisible lights, make sure that they don't turn into FQS
    gl_Position = vec4(0.0,0.0,-1.0,1.0);
    scrPosition = vec4(0.0);
    cenPosition = inCentral;
    color       = inColor;
    return;
    }

  vec4 pos = ubo.mvp*vec4(inPos,1.0);
  vec4 cen = ubo.mvp*vec4(inCentral.xyz,1.0);

  int neg = 0;
  for(int i=0;i<8;++i) {
    vec3 at  = inCentral.xyz + v[i]*inCentral.w;
    vec4 pos = ubo.mvp*vec4(at,1.0);

    if(pos.z<0.0)
      neg++;
    pos.xy/=pos.w;
    }

  if(neg>0 && neg<8) {
    // transform close lights into FSQ
    vec3 fsq = (inPos-inCentral.xyz)/inCentral.w;
    pos = vec4(fsq.xy,0.0,1.0);
    }

  gl_Position = pos;
  scrPosition = pos;
  cenPosition = inCentral;
  color       = inColor;
  }

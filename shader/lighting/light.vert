#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive    : enable

#include "scene.glsl"

out gl_PerVertex {
  vec4 gl_Position;
  };

layout(push_constant, std140) uniform Pbo {
  vec3  origin; //lwc
  } push;

layout(binding = 0, std140) uniform UboScene {
  SceneDesc scene;
  };

layout(binding = 4, std140) readonly buffer SsboLighting {
  LightSource lights[];
  };

layout(location = 0) out vec4 cenPosition;
layout(location = 1) out vec3 color;

vec3 v[] = {
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
    if(dot(scene.frustrum[i],vec4(at,1.0))<-R)
      return false;
    }
  return true;
  }

void main(void) {
  LightSource light = lights[gl_InstanceIndex];

  if(!testFrustrum(light.pos,light.range)) {
    // skip invisible lights, make sure that they don't turn into FQS
    gl_Position = vec4(0.0,0.0,-1.0,1.0);
    cenPosition = vec4(0.0);
    color       = vec3(0.0);
    return;
    }

  int neg = 0;
  for(int i=0;i<8;++i) {
    vec3 at  = light.pos + v[i]*light.range;
    vec4 pos = scene.viewProject*vec4(at,1.0);

    if(pos.z<0.0)
      neg++;
    pos.xy/=pos.w;
    // TODO: list of fsq lights
    }

  const vec3 inPos = v[gl_VertexIndex];
  vec4 pos = vec4(0);
  if(neg>0 && neg<8) {
    // transform close lights into FSQ
    vec2 fsq = vec2(inPos.x, -inPos.y);
    if(gl_VertexIndex>=4)
      pos = vec4(uintBitsToFloat(0x7fc00000)); else
      pos = vec4(fsq.xy,0.0,1.0);
    } else {
    pos = scene.viewProject*vec4(light.pos+inPos*light.range, 1.0);
    }

  //const vec3 origin = vec3(38983.9336, 4080.52637, -1888.59839);
  gl_Position = pos;
  cenPosition = vec4(light.pos,light.range);
  color       = light.color;
  }

#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 2) uniform sampler2D textureD;

layout(location = 0) in vec2 inUV;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec4 inColor;

layout(location = 3) in vec3 inLight;

layout(location = 0) out vec4 outColor;

void main() {
  vec4 t = texture(textureD,inUV);
  if(t.a<0.5)
    discard;
  float l     = dot(inLight,normalize(inNormal));
  vec3  color = mix(vec3(0.5),vec3(1.0),max(l,0.0));

  outColor    = vec4(1.0,0.0,0.0,1.0)*t*vec4(color,1.0)*vec4(inColor);
  }

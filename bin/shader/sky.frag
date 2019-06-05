#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 1) uniform sampler2D textureL0;
layout(binding = 2) uniform sampler2D textureL1;

layout(std140,binding = 0) uniform UniformBufferObject {
  mat4 mvp;
  vec4 color;
  vec2 dxy0;
  vec2 dxy1;
  vec3 sunDir;
  } ubo;

layout(location = 0) in vec2 inPos;
layout(location = 0) out vec4 outColor;

float phase(float alpha, float g) {
  float a = 3.0*(1.0-g*g);
  float b = 2.0*(2.0+g*g);
  float c = 1.0+alpha*alpha;
  float d = pow(1.0+g*g-2.0*g*alpha, 1.5);
  d = max(d, 0.00001);
  return (a/b)*(c/d);
  }

vec3 sunColor(vec3 ray, vec3 sunDir){
  float alpha = max(0.0,dot(ray,sunDir));
  float sun   = smoothstep(0.0, 100.0, phase(alpha, 0.9995));
  return sun*vec3(1.0,0.6,0.05);
  }

void main() {
  vec4 v = ubo.mvp*vec4(inPos,1.0,1.0);
  v.xyz/=v.w;
  vec3 view = v.xyz/max(abs(v.y),0.001);//normalize(v.xyz);

  vec4 cloudL1 = texture(textureL1,view.zx*0.2+ubo.dxy1);
  vec4 cloudL0 = texture(textureL0,view.zx*0.2+ubo.dxy0);

  vec3  sun = sunColor(normalize(v.xyz),ubo.sunDir);
  vec4  c   = cloudL0+cloudL1;

  outColor = mix(ubo.color+vec4(sun,0.0),c,min(1.0,c.a));

  //vec4 l1 = mix(ubo.color,cloudL1,cloudL1.a);
  //outColor = mix(l1,cloudL0,cloudL0.a);
  }

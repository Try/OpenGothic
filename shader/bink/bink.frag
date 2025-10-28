#version 450

#extension GL_ARB_separate_shader_objects : enable

layout(push_constant, std430) uniform UboPush {
  uint strideY;
  uint strideU;
  uint strideV;
  };
layout(binding = 0, std430) readonly buffer CBY { uint planeY[]; };
layout(binding = 1, std430) readonly buffer CBU { uint planeU[]; };
layout(binding = 2, std430) readonly buffer CBV { uint planeV[]; };

layout(location = 0) out vec4 outColor;

uint readPlaneY(ivec2 at) {
  uint i = at.x + at.y*strideY;
  uint r = planeY[i/4];
  return (r >> (i%4)*8) & 0xFF;
  }

uint readPlaneU(ivec2 at) {
  uint i = at.x + at.y*strideU;
  uint r = planeU[i/4];
  return (r >> (i%4)*8) & 0xFF;
  }

uint readPlaneV(ivec2 at) {
  uint i = at.x + at.y*strideV;
  uint r = planeV[i/4];
  return (r >> (i%4)*8) & 0xFF;
  }

void main() {
  ivec2 fragCoord = ivec2(gl_FragCoord.xy);

  float Y = readPlaneY(fragCoord);
  float U = readPlaneU(fragCoord/2);
  float V = readPlaneV(fragCoord/2);

  float r = 1.164f * (Y - 16.f) + 1.596f * (V - 128.f);
  float g = 1.164f * (Y - 16.f) - 0.813f * (V - 128.f) - 0.391f * (U - 128.f);
  float b = 1.164f * (Y - 16.f) + 2.018f * (U - 128.f);

  vec3 cl = clamp(vec3(r,g,b), vec3(0), vec3(255))/255.0;
  outColor = vec4(cl,1);
  }

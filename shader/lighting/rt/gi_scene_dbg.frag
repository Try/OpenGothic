#version 460

#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive    : enable

layout(location = 0) out vec4 outColor;

layout(location = 0) in  vec3      inColor;
layout(location = 1) in  vec3      inBCoord;
layout(location = 2) in  flat uint inLevel;

const vec3 debugColors[] = {
  vec3(1,1,1),
  vec3(1,0,0),
  vec3(0,1,0),
  vec3(0,0,1),
  vec3(1,1,0),
  vec3(1,0,1),
  vec3(0,1,1),
  vec3(1,0.5,0),
  vec3(0.5,1,0),
  vec3(0,0.5,1),
  };

uint barycentricsToSpaceFillingCurveIndex(float u, float v, uint level) {
  u = clamp(u, 0.0f, 1.0f);
  v = clamp(v, 0.0f, 1.0f);

  uint iu, iv, iw;

  // Quantize barycentric coordinates
  float fu = u * (1u << level);
  float fv = v * (1u << level);

  iu = uint(fu);
  iv = uint(fv);

  float uf = fu - float(iu);
  float vf = fv - float(iv);

  if(iu >= (1u << level))
    iu = (1u << level) - 1u;
  if(iv >= (1u << level))
    iv = (1u << level) - 1u;

  uint iuv = iu + iv;

  if(iuv >= (1u << level))
    iu -= iuv - (1u << level) + 1u;

  iw = ~(iu + iv);

  if(uf + vf >= 1.0f && iuv < (1u << level) - 1u)
    --iw;

  uint b0 = ~(iu ^ iw);
  b0 &= ((1u << level) - 1u);
  uint t = (iu ^ iv) & b0;

  uint f = t;
  f ^= f >> 1u;
  f ^= f >> 2u;
  f ^= f >> 4u;
  f ^= f >> 8u;
  uint b1 = ((f ^ iu) & ~b0) | t;

  // Interleave bits
  b0 = (b0 | (b0 << 8u)) & 0x00ff00ffu;
  b0 = (b0 | (b0 << 4u)) & 0x0f0f0f0fu;
  b0 = (b0 | (b0 << 2u)) & 0x33333333u;
  b0 = (b0 | (b0 << 1u)) & 0x55555555u;
  b1 = (b1 | (b1 << 8u)) & 0x00ff00ffu;
  b1 = (b1 | (b1 << 4u)) & 0x0f0f0f0fu;
  b1 = (b1 | (b1 << 2u)) & 0x33333333u;
  b1 = (b1 | (b1 << 1u)) & 0x55555555u;

  return b0 | (b1 << 1u);
  }

void main(void) {
  outColor = vec4(inColor, 1.0);
  outColor = vec4(inBCoord,1.0);

  uint idx = barycentricsToSpaceFillingCurveIndex(inBCoord.y,inBCoord.z,inLevel);
  outColor = vec4(debugColors[idx%debugColors.length()],1.0);
  // outColor = vec4(inLevel/100.0);
  }

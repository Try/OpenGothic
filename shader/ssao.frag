#version 450
#extension GL_ARB_separate_shader_objects : enable

const float M_PI  = 3.1415926535897932384626433832795;

layout(binding  = 0) uniform sampler2D lightingBuf;
layout(binding  = 1) uniform sampler2D diffuse;
layout(binding  = 2) uniform sampler2D normals;
layout(binding  = 3) uniform sampler2D depth;

layout(push_constant, std140) uniform PushConstant {
  mat4 mvp;
  mat4 mvpInv;
  } ubo;

layout(location = 0) in  vec2 uv;
layout(location = 1) in  vec2 inPos;

layout(location = 0) out vec4 outColor;

const uint  numSamples = 16;
const float sphereLen  = 50;

uint hash(uint x) {
  x += ( x << 10u );
  x ^= ( x >>  6u );
  x += ( x <<  3u );
  x ^= ( x >> 11u );
  x += ( x << 15u );
  return x;
  }

uint hash(uvec2 v) {
  return hash(v.x ^ hash(v.y));
  }

float floatConstruct(uint m) {
  const uint ieeeMantissa = 0x007FFFFFu; // binary32 mantissa bitmask
  const uint ieeeOne      = 0x3F800000u; // 1.0 in IEEE binary32

  m &= ieeeMantissa;                     // Keep only mantissa bits (fractional part)
  m |= ieeeOne;                          // Add fractional part to 1.0

  float  f = uintBitsToFloat( m );       // Range [1:2]
  return f - 1.0;                        // Range [0:1]
  }

vec3 sampleDisc(uint i, uint maxSamples, uint h0) {
  const float a = M_PI*2.0*float(i)/float(maxSamples);
  const float r = floatConstruct(hash(i) ^ h0);
  return vec3(cos(a)*r, sin(a)*r, 0);
  }

vec3 inverse(vec3 pos) {
  vec4 ret = ubo.mvpInv*vec4(pos,1.0);
  return (ret.xyz/ret.w);
  }

vec3 project(vec3 pos) {
  vec4 ret = ubo.mvp*vec4(pos,1.0);
  return (ret.xyz/ret.w);
  }

vec3 amendZ(vec3 pos) {
  pos.z = textureLod(depth, pos.xy*0.5+vec2(0.5), 0).r;
  return pos;
  }

float calcOcclussion() {
  vec3 norm = normalize(textureLod(normals,uv,0).xyz*2.0-vec3(1.0));
  // Compute a tangent frame and rotate the half vector to world space
  vec3 up   = abs(norm.z) < 0.999f ? vec3(0.0f, 0.0f, 1.0f) : vec3(1.0f, 0.0f, 0.0f);
  mat3 tangent;
  tangent[0] = normalize(cross(up, norm));
  tangent[1] = cross(norm, tangent[0]);
  tangent[2] = norm;

  const vec3 at0   = amendZ(vec3(inPos.xy,0));
  const vec3 pos0  = inverse(at0);

  if(at0.z>=1.0)
    return 0; // sky

  uint  h0 = hash(uvec2(gl_FragCoord));
  float occlusion = 0, weightAll = 0;
  for(uint i=0; i<numSamples; ++i) {
    vec3  v      = tangent * sampleDisc(i,numSamples,h0);

    vec3  at1    = amendZ(project(pos0 + v*sphereLen));
    vec3  pos1   = inverse(at1);

    //if(at1.z>=1.0)
    //  continue; // sky

    vec3  dp     = pos1-pos0;
    float weight = dot(dp,norm);

    float occ = 1.0;
    if(weight>0.1*sphereLen)
      occlusion += 1.0-min(pow(weight/sphereLen,2),1);
    weightAll += occ;
    }

  if(weightAll<=0.0)
    return 0;
  return min(1,occlusion/weightAll);
  }

void main() {
  vec4 clr  = textureLod(diffuse,uv,0);
  vec4 lbuf = textureLod(lightingBuf,uv,0);

  float occ     = calcOcclussion();
  vec4  ambient = vec4(0.2); // TODO: uniform it

  outColor  = lbuf-clr*ambient*occ;
  // outColor  = vec4(1-occ);
  }

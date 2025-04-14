#ifndef COMMON_GLSL
#define COMMON_GLSL

#extension GL_EXT_samplerless_texture_functions : enable

const float M_PI = 3.1415926535897932384626433832795;

//Environment
const float RPlanet  = 6360e3;       // Radius of the planet in meters
const float RClouds  = RPlanet+3000; // Clouds height in meters
const float RAtmos   = 6460e3;       // Radius of the atmosphere in meters

const float Ffresnel = 0.02;
const float IorWater = 1.0 / 1.52; // air / water
const float IorAir   = 1.52;       // water /air
const vec3  WaterAlbedo = vec3(0.8,0.9,1.0);

const vec3  GGroundAlbedo = vec3(0.3);
const float Fd_Lambert    = (1.0/M_PI);
const float Fd_LambertInv = (M_PI);

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

float linearDepth(float d, vec3 clipInfo) {
  // z_n * z_f,  z_n - z_f, z_f
  return (clipInfo[0] / (clipInfo[1] * d + clipInfo[2]));
  }

// Shlick's approximation of the Fresnel factor.
vec3 fresnelSchlick(vec3 F0, float cosTheta) {
  return F0 + (vec3(1.0) - F0) * pow(1.0 - cosTheta, 5.0);
  }

// https://www.scratchapixel.com/lessons/3d-basic-rendering/introduction-to-shading/reflection-refraction-fresnel
float fresnel(const vec3 incident, const vec3 normal, const float ior) {
  float cosi = clamp(-1, 1, dot(incident, normal));
  float etai = 1, etat = ior;
  if(cosi > 0) {
    float a = etai;
    etai = etat;
    etat = a;
    }
  // Compute sini using Snell's law
  float sint = etai / etat * sqrt(max(0.f, 1 - cosi * cosi));
  // Total internal reflection
  if(sint >= 1)
    return 1;

  float cost = sqrt(max(0.f, 1 - sint * sint));
  cosi = abs(cosi);
  float Rs = ((etat * cosi) - (etai * cost)) / ((etat * cosi) + (etai * cost));
  float Rp = ((etai * cosi) - (etat * cost)) / ((etai * cosi) + (etat * cost));
  float kr = (Rs * Rs + Rp * Rp) / 2;

  // As a consequence of the conservation of energy, transmittance is given by:
  // kt = 1 - kr;
  return kr;
  }

float safeacos(const float x) {
  return acos(clamp(x, -1.0, 1.0));
  }

vec3 srgbDecode(vec3 color){
  return pow(color,vec3(2.2));
  }

vec3 srgbEncode(vec3 color){
  return pow(color,vec3(1.0/2.2));
  }

vec4 mixClr(vec4 s, vec4 d) {
  float a  =  (1-s.a)*d.a + s.a;
  if(a<=0.0)
    return vec4(0);
  vec3  c  = ((1-s.a)*d.a*d.rgb+s.a*s.rgb)/a;
  return vec4(c,a);
  }

float interleavedGradientNoise(vec2 pixel) {
  return fract(52.9829189f * fract(0.06711056f*float(pixel.x) + 0.00583715f*float(pixel.y)));
  }

void decodeBits(float v, out bool flt, out bool atst, out bool water) {
  int x = int(v*255+0.5);

  flt   = (x & (1 << 1))!=0;
  atst  = (x & (1 << 2))!=0;
  water = (x & (1 << 3))!=0;
  }

bool isGBufFlat(float v) {
  int x = int(v*255+0.5);
  return (x & (1 << 1))!=0;
  }

bool isGBufWater(float v) {
  int x = int(v*255+0.5);
  return (x & (1 << 3))!=0;
  }

float packHiZ(float z) {
  // return z;
  return (z-0.95)*20.0;
  }

// From https://gamedev.stackexchange.com/questions/96459/fast-ray-sphere-collision-code.
float rayIntersect(vec3 v, vec3 d, float R) {
  float b = dot(v, d);
  float c = dot(v, v) - R*R;
  if(c > 0.0f && b > 0.0)
    return -1.0;
  float discr = b*b - c;
  if(discr < 0.0)
    return -1.0;
  // Special case: inside sphere, use far discriminant
  if(discr > b*b)
    return (-b + sqrt(discr));
  return -b - sqrt(discr);
  }

/*
 * Final output basically looks up the value from the skyLUT, and then adds a sun on top,
 * does some tonemapping.
 */
vec3 textureSkyLUT(in sampler2D skyLUT, const vec3 viewPos, vec3 rayDir, vec3 sunDir) {
  //const vec3  viewPos = vec3(0.0, RPlanet + push.plPosY, 0.0);
  float height = viewPos.y;
  vec3  up     = vec3(0,1,0); //NOTE: this is not a space-sim game, so just Y-up

  float horizonAngle  = safeacos(sqrt(height*height - RPlanet*RPlanet) / height);
  float altitudeAngle = horizonAngle - acos(dot(rayDir, up)); // Between -PI/2 and PI/2
  float azimuthAngle; // Between 0 and 2*PI
  if(abs(altitudeAngle) > (0.5*M_PI - .0001)) {
    // Looking nearly straight up or down.
    azimuthAngle = 0.0;
    } else {
    vec3 right   = cross(sunDir, up);
    vec3 forward = cross(up, right);

    vec3 projectedDir = normalize(rayDir - up*(dot(rayDir, up)));
    float sinTheta = dot(projectedDir, right);
    float cosTheta = dot(projectedDir, forward);
    azimuthAngle = atan(sinTheta, cosTheta) + M_PI;
    }

  // Non-linear mapping of altitude angle. See Section 5.3 of the paper.
  float v  = 0.5 + 0.5*sign(altitudeAngle)*sqrt(abs(altitudeAngle)*2.0/M_PI);
  vec2  uv = vec2(azimuthAngle / (2.0*M_PI), v);
  return textureLod(skyLUT, uv, 0).rgb;
  }

// Sample i-th point from Hammersley point set of NumSamples points total.
// See: http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html
vec2 sampleHammersley(uint i, uint numSamples) {
  uint  bits = bitfieldReverse(i);
  float vdc  = float(bits) * 2.3283064365386963e-10; // / 0x100000000
  return vec2(float(i)/float(numSamples), vdc);
  }

// Uniform sampling
vec3 sampleHemisphere(uint i, uint numSamples, float offsetAng) {
  const vec2  xi  = sampleHammersley(i,numSamples);
  const float u   = 1-xi.x;
  const float u1p = sqrt(1.0 - u*u);
  const float a   = M_PI*2.0*xi.y + offsetAng;
  return vec3(cos(a) * u1p, xi.x, sin(a) * u1p);
  }

// Cosine sampling
vec3 sampleHemisphereCos(uint i, uint numSamples, float offsetAng) {
  const vec2  xi  = sampleHammersley(i,numSamples);
  const float u   = sqrt(1 - xi.x);
  const float u1p = sqrt(1 - u*u);
  const float a   = M_PI*2.0*xi.y + offsetAng;
  return vec3(cos(a) * u1p, xi.x, sin(a) * u1p);
  }

vec3 sampleSphere(uint i, uint numSamples, float offsetAng) {
  // fibonacci_lattice
  const float PHI = 0.5*(sqrt(5.0) + 1.0);
  vec2 xy = vec2((float(i)+0.5)/float(numSamples), mod(float(i)/PHI, 1.0));

  vec2 pt = vec2(2.0*M_PI*xy.y, acos(2.0*xy.x - 1.) - M_PI*0.5);
  return vec3(cos(pt.x)*cos(pt.y), sin(pt.x)*cos(pt.y), sin(pt.y));
  }

vec3 projectiveUnproject(in mat4 projectiveInv, in vec3 pos) {
  vec4 o;
  o.x = pos.x * projectiveInv[0][0];
  o.y = pos.y * projectiveInv[1][1];
  o.z = pos.z * projectiveInv[2][2] + projectiveInv[3][2];
  o.w = pos.z * projectiveInv[2][3] + projectiveInv[3][3];
  return o.xyz/o.w;
  }

vec3 projectiveProject(in mat4 projective, in vec3 pos) {
  vec4 o;
  o.x = pos.x * projective[0][0];
  o.y = pos.y * projective[1][1];
  o.z = pos.z * projective[2][2] + projective[3][2];
  o.w = pos.z * projective[2][3] + projective[3][3];
  return o.xyz/o.w;
  }

vec2 msign( vec2 v ) {
  return vec2( (v.x>=0.0) ? 1.0 : -1.0,
               (v.y>=0.0) ? 1.0 : -1.0 );
  }

// https://www.shadertoy.com/view/llfcRl
uint octahedral_32( in vec3 nor ) {
  nor.xy /= (abs(nor.x) + abs(nor.y) + abs(nor.z));
  nor.xy  = (nor.z >= 0.0) ? (nor.xy) : (1.0-abs(nor.yx))*msign(nor.xy);
  return packSnorm2x16(nor.xy);
  }

vec3 i_octahedral_32( uint data ) {
  // Rune Stubbe's version
  vec2 v   = unpackSnorm2x16(data);
  vec3 nor = vec3(v, 1.0 - abs(v.x) - abs(v.y));
  float t = max(-nor.z, 0.0);
  nor.x += (nor.x>0.0) ? -t : t;
  nor.y += (nor.y>0.0) ? -t : t;
  return normalize(nor);
  }

// the next packing/unpacking methods are taken from
// https://github.com/Microsoft/DirectX-Graphics-Samples/blob/master/MiniEngine/Core/Shaders/PixelPacking_R11G11B10.hlsli
uint packR11G11B10F(vec3 rgb) {
  rgb = min(rgb, uintBitsToFloat(0x477C0000));
  uint r = ((packHalf2x16(vec2(rgb.r, 0)) + 8) >> 4) & 0x000007FF;
  uint g = ((packHalf2x16(vec2(rgb.g, 0)) + 8) << 7) & 0x003FF800;
  uint b = ((packHalf2x16(vec2(rgb.b, 0)) + 16) << 17) & 0xFFC00000;
  return r | g | b;
  }

vec3 unpackR11G11B10F(uint rgb) {
  float r = unpackHalf2x16((rgb << 4)  & 0x7FF0).r;
  float g = unpackHalf2x16((rgb >> 7)  & 0x7FF0).r;
  float b = unpackHalf2x16((rgb >> 17) & 0x7FE0).r;
  return vec3(r, g, b);
  }

// This is like R11G11B10F except that it moves one bit from each exponent to each mantissa.
uint packR11G11B10E4F(vec3 rgb) {
  rgb = clamp(rgb, 0.0, uintBitsToFloat(0x3FFFFFFF));
  uint r = ((packHalf2x16(vec2(rgb.r, 0)) + 4) >> 3) & 0x000007FF;
  uint g = ((packHalf2x16(vec2(rgb.g, 0)) + 4) << 8) & 0x003FF800;
  uint b = ((packHalf2x16(vec2(rgb.b, 0)) + 8) << 18) & 0xFFC00000;
  return r | g | b;
  }

vec3 unpackR11G11B10E4F(uint rgb) {
  float r = unpackHalf2x16((rgb << 3)  & 0x3FF8).r;
  float g = unpackHalf2x16((rgb >> 8)  & 0x3FF8).r;
  float b = unpackHalf2x16((rgb >> 18) & 0x3FF0).r;
  return vec3(r, g, b);
  }

uint packUint2x16(uvec2 v){
  return (v.x & 0xFFFF) | ((v.y & 0xFFFF) << 16);
  }

uvec2 unpackUInt2x16(uint v) {
  //NOTE: correct spelling `packUint2x16` is reserved by extensions
  uvec2 ret;
  ret.x = v & 0xFFFF;
  ret.y = v >> 16;
  return ret;
  }

uint packUint4x8(uvec4 v){
  return (v.x & 0xFF) | ((v.y & 0xFF) << 8) | ((v.z & 0xFF) << 16) | ((v.w & 0xFF) << 24);
  }

uvec4 unpackUint4x8(uint v) {
  uvec4 r;
  r.x = (v      ) & 0xFF;
  r.y = (v >>  8) & 0xFF;
  r.z = (v >> 16) & 0xFF;
  r.w = (v >> 24) & 0xFF;
  return r;
  }

uint pack565_16(ivec3 a, uint b) {
  uint x = (a.x & 0x1F)   << 0;
  uint y = (a.y & 0x3F)   << 5;
  uint z = (a.z & 0x1F)   << 11;
  uint w = (  b & 0xFFFF);
  //uint d = uint(z*0xFFFF) << 16;
  return ((x | y | z) << 16) | w;
  }

uvec4 unpack565_16(uint p) {
  uvec4 ret;
  ret.w = p & 0xFFFF;
  p = p >> 16;
  ret.x = (p >>  0) & 0x1F;
  ret.y = (p >>  5) & 0x3F;
  ret.z = (p >> 11) & 0x1F;
  return ret;
  }

uint encodeNormal(vec3 n) {
  return octahedral_32(n);
  }

vec3 decodeNormal(uint n) {
  return i_octahedral_32(n);
  }

vec3 normalFetch(in usampler2D gbufNormal, ivec2 p) {
  const uint n = texelFetch(gbufNormal, p, 0).r;
  return decodeNormal(n);
  }

vec3 normalFetch(in utexture2D gbufNormal, ivec2 p) {
  const uint n = texelFetch(gbufNormal, p, 0).r;
  return decodeNormal(n);
  }

// PCG3D
// https://www.jcgt.org/published/0009/03/02/
// https://www.shadertoy.com/view/XlGcRh
// Produces three random unsigned integers using
// a seed of three unsigned integers
uvec3 pcg3d(uvec3 v) {

    v = v * 1664525u + 1013904223u;

    v.x += v.y*v.z;
    v.y += v.z*v.x;
    v.z += v.x*v.y;

    v ^= v >> 16u;

    v.x += v.y*v.z;
    v.y += v.z*v.x;
    v.z += v.x*v.y;

    return v;
}

// fragCoords: 2D integer pixel position
// the bit depth of the R,G & B output channels
vec3 dither(vec2 fragCoords, uvec3 targetBits)
{
    vec3 divisionSteps = vec3(float((1 << targetBits.x) - 1),
                              float((1 << targetBits.y) - 1),
                              float((1 << targetBits.z) - 1));
    // separate noise per channel had subjectively better visuals than single IGN
    // PCG3D can also be used in case multiple dithers need to be layered
    // vec3 nrnd = fract(pcg3d(uvec3(fragCoords.xyy)) / divisionSteps);
    vec3 nrnd = interleavedGradientNoise(fragCoords.xy).xxx;
    return (nrnd * 2.0 - 1) / divisionSteps;
}

#endif

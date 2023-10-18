#ifndef COMMON_GLSL
#define COMMON_GLSL

const float M_PI = 3.1415926535897932384626433832795;

//Environment
const float RPlanet  = 6360e3;       // Radius of the planet in meters
const float RClouds  = RPlanet+3000; // Clouds height in meters
const float RAtmos   = 6460e3;       // Radius of the atmosphere in meters

const float Ffresnel = 0.02;
const float IorWater = 1.0 / 1.52; // air / water
const float IorAir   = 1.52;       // water /air
const vec3  WaterAlbedo = vec3(0.8,0.9,1.0);

const vec3  GGroundAlbedo = vec3(0.1);
const float Fd_Lambert    = (1.0/M_PI);
const float Fd_LambertInv = (M_PI);

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

bool isGBufWater(float v) {
  bool dummy   = false;
  bool isWater = false;
  decodeBits(v,dummy,dummy,isWater);
  return isWater;
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

#endif

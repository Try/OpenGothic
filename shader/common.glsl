#ifndef COMMON_GLSL
#define COMMON_GLSL

const float M_PI = 3.1415926535897932384626433832795;

//Environment
const float RPlanet  = 6360e3;       // Radius of the planet in meters
const float RClouds  = RPlanet+3000; // Clouds height in meters
const float RAtmos   = 6460e3;       // Radius of the atmosphere in meters

const vec3  GGroundAlbedo = vec3(0.1);
const float GSunIntensity = 20.0;

float reconstructCSZ(float d, vec3 clipInfo) {
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

vec3 jodieReinhardTonemap(vec3 c){
  // From: https://www.shadertoy.com/view/tdSXzD
  float l = dot(c, vec3(0.2126, 0.7152, 0.0722));
  vec3 tc = c / (c + 1.0);
  return mix(c / (l + 1.0), tc, tc);
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

#endif

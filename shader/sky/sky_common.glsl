const float PI = 3.14159265358979323846;
#if defined(FOG)
const int   iSteps = 8;
const int   jSteps = 8;
#else
const int   iSteps = 8;
const int   jSteps = 16;
#endif

//Environment
const float RPlanet  = 6360e3;       // Radius of the planet in meters
const float RClouds  = RPlanet+3000; // Clouds height in meters
const float RAtmos   = 6460e3;       // Radius of the atmosphere in meters
// Rayleigh scattering coefficient
const vec3  RSC = vec3(0.0000038,
                       0.0000135,
                       0.0000331);
const float Hr       = 8000.0; // Reyleight scattering top
const float Hm       = 1000.0; // Mie scattering top
const float kMie     = 21e-6;  // Mie scattering coefficient
const float g        = 0.8;    // light concentration .76 //.45 //.6  .45 is normaL
const float g2       = g * g;
const float sunPower = 10.0;   // sun light power, 10.0 is normal

layout(std140,binding = 0) uniform UniformBufferObject {
  mat4  viewProjectInv;
  vec2  dxy0;
  vec2  dxy1;
  vec3  sunDir;
  float night;
  float plPosY;
  } ubo;

#if defined(FOG)
layout(binding = 1) uniform sampler2D depth;
#else
layout(binding = 1) uniform sampler2D textureDayL0;
layout(binding = 2) uniform sampler2D textureDayL1;

layout(binding = 3) uniform sampler2D textureNightL0;
layout(binding = 4) uniform sampler2D textureNightL1;
#endif

float phase(float alpha, float g) {
  float a = 3.0*(1.0-g*g);
  float b = 2.0*(2.0+g*g);
  float c = 1.0+alpha*alpha;
  float d = pow(1.0+g*g-2.0*g*alpha, 1.5);
  d = max(d, 0.00001);
  return (a/b)*(c/d);
  }

vec3 exposure(vec3 color) {
  float env = 1.0;
  return vec3(env * pow(color, vec3(.7)));
  }

vec2 densities(in vec3 pos) {
  const float haze = 0.2;

  float h        = length(pos) - RPlanet;
  float rayleigh = exp(-h/Hr);
  float cld      = 0.;
  float mie = exp(-h/Hm) + cld + haze;
  return vec2(rayleigh,mie);
  }

float rayIntersect(in vec3 p, in vec3 d, in float R) {
  vec3  v = p;
  float b = dot(v, d);
  float c = dot(v, v) - R*R;
  float det2 = b * b - c;
  if(det2 < 0.0)
    return -1.0;
  float det = sqrt(det2);
  float t1 = -b - det, t2 = -b + det;
  return (t1 >= 0.) ? t1 : t2;
  }

// based on: http://www.scratchapixel.com/lessons/3d-advanced-lessons/simulating-the-colors-of-the-sky/atmospheric-scattering/
// https://www.shadertoy.com/view/ltlSWB
vec3 scatter(vec3 pos, vec3 view, in vec3 sunDir, float rayLength, out float outScat) {
  // pos = vec3(0,RPlanet,0);
  float mu     = dot(view, sunDir);
  float opmu2  = 1.0 + mu*mu;
  float phaseR = 3.0/(16.0*PI) * opmu2;
  float phaseM = 3.0/(8.0 *PI) * ((1.0 - g2) * opmu2) / ((2.0 + g2) * pow(1.0 + g2 - 2.0*g*mu, 1.5));

  float depthR = 0.0, depthM = 0.0;
  vec3  R = vec3(0.0), M = vec3(0.0);

  vec2 depth = vec2(0);
  float dl = rayLength / float(iSteps);
  for (int i=0; i<iSteps; ++i) {
    float l = (float(i)+0.5)*dl;
    vec3  p = pos + view * l;

    vec2 dens = densities(p)*dl;
    depth += dens;

    float Ls = rayIntersect(p, sunDir, RAtmos);
    if(Ls>0.0) {
      float dls = Ls/float(jSteps);
      vec2 depthS = vec2(0);
      for(int j=0; j<jSteps; ++j) {
        float ls = float(j) * dls;
        vec3  ps = p + sunDir * ls;
        depthS  += densities(ps)*dls;
        }

      vec3 A = exp(-(RSC * (depthS.x + depth.x) + kMie * (depthS.y + depth.y)));
      R += A * dens.x;
      M += A * dens.y;
      }
  }

  outScat = 1.0 - clamp(depthM*1e-5,0.,1.);
  return sunPower * (R * RSC * phaseR + M * kMie * phaseM);
  }

vec3 atmosphere(in vec3 pos, vec3 view, vec3 sunDir) {
  // moon
  float att = 1.0;
  if(sunDir.y < -0.0) {
    sunDir.y = -sunDir.y;
    att = 0.25;
    }
  view.y = abs(view.y);
  float scatt     = 0;
  float rayLength = rayIntersect(pos, view, RAtmos);
  return scatter(pos, view, sunDir, rayLength, scatt) * att;
  }

vec3 fogMie(in vec3 pos, vec3 view, vec3 sunDir, float rayLength) {
  // moon
  float att = 1.0;
  if(sunDir.y < -0.0) {
    sunDir.y = -sunDir.y;
    att = 0.25;
    }
  if(view.y <= -0.0)
    view.y = -view.y;
  float scatt = 0;
  vec3  color = scatter(pos, view, sunDir, rayLength, scatt) * att;
  return vec3(color);
  }

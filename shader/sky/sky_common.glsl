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

// Table 1: Coefficients of the different participating media compo-nents constituting the Earth’s atmosphere
// These are per megameter.
const vec3  rayleighScatteringBase = vec3(5.802, 13.558, 33.1) / 1e6;
const float rayleighAbsorptionBase = 0.0;

const float mieScatteringBase      = 3.996 / 1e6;
const float mieAbsorptionBase      = 4.40  / 1e6;

// NOTE: Ozone does not contribute to scattering; it only absorbs light.
const vec3  ozoneAbsorptionBase    = vec3(0.650, 1.881, .085) / 1e6;

const vec3  groundAlbedo           = vec3(0.1);

layout(push_constant, std140) uniform UboPush {
  mat4  viewProjectInv;
  vec2  dxy0;
  vec2  dxy1;
  vec3  sunDir;
  float night;
  float plPosY;
  } push;

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

vec3 exposure(vec3 color) {
  float env = 1.0;
  return vec3(env * pow(color, vec3(.7)));
  }

// based on: https://developer.amd.com/wordpress/media/2012/10/Wenzel-Real-time_Atmospheric_Effects_in_Games.pdf
float volumetricFog(in vec3 pos, in vec3 cameraToWorldPos) {
  // Fog props
  const float fogHeightDensityAtViewer = 0.5;
  const float globalDensity            = 0.005;
  const float heightFalloff            = 0.02;

  // float fogHeightDensityAtViewer = exp(-heightFalloff * pos.z);
  float fogInt = length(cameraToWorldPos) * fogHeightDensityAtViewer;
  if(abs(cameraToWorldPos.y) > 0.01) {
    float t = heightFalloff*cameraToWorldPos.y;
    fogInt *= (1.0-exp(-t))/t;
    }
  return 1.0-exp(-globalDensity*fogInt);
  }

float miePhase(float cosTheta) {
  const float scale = 3.0/(8.0*PI);

  float num   = (1.0-g*g)*(1.0+cosTheta*cosTheta);
  float denom = (2.0+g*g)*pow((1.0 + g*g - 2.0*g*cosTheta), 1.5);

  return scale*num/denom;
  }

float rayleighPhase(float cosTheta) {
  const float k = 3.0/(16.0*PI);
  return k*(1.0+cosTheta*cosTheta);
  }

float phase(float alpha, float g) {
  float a = 3.0*(1.0-g*g);
  float b = 2.0*(2.0+g*g);
  float c = 1.0+alpha*alpha;
  float d = pow(1.0+g*g-2.0*g*alpha, 1.5);
  d = max(d, 0.00001);
  return (a/b)*(c/d);
  }

vec2 densities(in vec3 pos) {
  const float haze = 0.2;

  float h        = length(pos) - RPlanet;
  float rayleigh = exp(-h/Hr);
  float cld      = 0.;
  float mie = exp(-h/Hm) + cld + haze;
  return vec2(rayleigh,mie);
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

// 4. Atmospheric model
void scatteringValues(vec3 pos,
                      out vec3 rayleighScattering,
                      out float mieScattering,
                      out vec3 extinction) {
  float altitudeKM      = (length(pos)-RPlanet) / 1000.0;
  // Note: Paper gets these switched up. See SkyAtmosphereCommon.cpp:SetupEarthAtmosphere in demo app
  float rayleighDensity = exp(-altitudeKM/8.0);
  float mieDensity      = exp(-altitudeKM/1.2);

  rayleighScattering       = rayleighScatteringBase*rayleighDensity;
  float rayleighAbsorption = rayleighAbsorptionBase*rayleighDensity;

  mieScattering = mieScatteringBase*mieDensity;
  float mieAbsorption = mieAbsorptionBase*mieDensity;

  vec3 ozoneAbsorption = ozoneAbsorptionBase*max(0.0, 1.0 - abs(altitudeKM-25.0)/15.0);

  extinction = rayleighScattering + rayleighAbsorption +
               mieScattering + mieAbsorption +
               ozoneAbsorption;
  }

// 5.5.2. LUT parameterization
vec3 textureLUT(sampler2D tex, vec3 pos, vec3 sunDir) {
  float height   = length(pos);
  vec3  up       = pos / height;
  float cosAngle = dot(sunDir, up);

  vec2 uv;
  uv.x = 0.5 + 0.5*cosAngle;
  uv.y = (height - RPlanet)/(RAtmos - RPlanet);
  return texture(tex, uv).rgb;
  }

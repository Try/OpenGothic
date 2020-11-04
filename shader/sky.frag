#version 450
#extension GL_ARB_separate_shader_objects : enable

const float PI = 3.141592;
#if defined(FOG)
const int   iSteps = 8;
const int   jSteps = 8;
#else
const int   iSteps = 16;
const int   jSteps = 8;
#endif

layout(std140,binding = 0) uniform UniformBufferObject {
  mat4  mvpInv;
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

layout(location = 0) in  vec2 inPos;
layout(location = 0) out vec4 outColor;

//Environment
const float RPlanet  = 6360e3; // Radius of the planet in meters
const float RAtmos   = 6380e3; // Radius of the atmosphere in meters
// Rayleigh scattering coefficient
const vec3  RSC      = vec3(0.00000519673,
                            0.0000121427,
                            0.0000296453);
const float Hr       = 8000.0; // Reyleight scattering top
const float Hm       = 1000.0; // Mie scattering top
const float kMie     = 21e-6;  // Mie scattering coefficient
const float g        = 0.45;   // light concentration .76 //.45 //.6  .45 is normaL
const float g2       = g * g;
const float sunPower = 10.0;   // sun light power, 10.0 is normal
// Fog props
const float fogHeightDensityAtViewer = 0.005;
const float globalDensity            = 0.001;
const float heightFalloff            = 0.0001;

#if !defined(FOG)
vec4 clounds(vec2 texc){
  vec4 cloudDL1 = texture(textureDayL1,texc*0.3+ubo.dxy1);
  vec4 cloudDL0 = texture(textureDayL0,texc*0.3+ubo.dxy0);
#ifdef G1
  return mix(cloudDL0,cloudDL1,cloudDL1.a);
#else
  return (cloudDL0+cloudDL1);
#endif
  }

vec4 stars(vec2 texc){
  vec4 cloudNL1 = texture(textureNightL1,texc*0.3+ubo.dxy1);
  vec4 cloudNL0 = texture(textureNightL0,texc*0.6);
#ifdef G1
  vec4 night    = mix(cloudNL0,cloudNL1,cloudNL1.a);
#else
  vec4 night    = cloudNL0+cloudNL0;
#endif
  return vec4(night.rgb,ubo.night);
  }
#endif

// based on: https://developer.amd.com/wordpress/media/2012/10/Wenzel-Real-time_Atmospheric_Effects_in_Games.pdf
float volumetricFog(in vec3 pos, in vec3 cameraToWorldPos) {
  // float fogHeightDensityAtViewer = exp(-heightFalloff * pos.z);
  float fogInt = length(cameraToWorldPos) * fogHeightDensityAtViewer;
  if(abs(cameraToWorldPos.y) > 0.01) {
    float t = heightFalloff*cameraToWorldPos.y;
    fogInt *= (1.0-exp(-t))/t;
    }
  return 1.0-exp(-globalDensity*fogInt);
  }

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

void densities(in vec3 pos, out float rayleigh, out float mie) {
  float h = length(pos) - RPlanet;
  rayleigh =  exp(-h/Hr);
  vec3 d = pos;
  d.y = 0.0;
  float dist = length(d);

  const float haze = 0.2; //0.2
  mie = exp(-h/Hm) + haze;
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
  float mu     = dot(view, sunDir);
  float opmu2  = 1.0 + mu*mu;
  float phaseR = 0.0596831 * opmu2;
  float phaseM = 0.1193662 * (1.0 - g2) * opmu2 / ((2.0 + g2) * pow(1.0 + g2 - 2.0*g*mu, 1.5));

  float depthR = 0.0, depthM = 0.0;
  vec3  R = vec3(0.0), M = vec3(0.0);

  float dl = rayLength / float(iSteps);
  for(int i=0; i<iSteps; ++i) {
    float l = (float(i)+0.5)*dl;
    vec3  p = pos + view * l;

    float dR, dM;
    densities(p, dR, dM);
    dR *= dl; dM *= dl;
    depthR += dR;
    depthM += dM;

    float Ls = rayIntersect(p, sunDir, RAtmos);
    if(Ls<=0.0)
      continue;

    float dls = Ls / float(jSteps);
    float depthRs = 0.0, depthMs = 0.0;
    for(int j = 0; j < jSteps; ++j) {
      float ls = float(j) * dls;
      vec3  ps = p + sunDir * ls;
      float dRs, dMs;
      densities(ps, dRs, dMs);
      depthRs += dRs * dls;
      depthMs += dMs * dls;
      }

    vec3 A = exp(-(RSC * (depthRs + depthR) + kMie * (depthMs + depthM)));
    R += A * dR;
    M += A * dM;
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
  if(view.y <= -0.0)
    view.y = -view.y;
  float scatt     = 0;
  float rayLength = rayIntersect(pos, view, RAtmos);
  return scatter(pos, view, sunDir, rayLength, scatt) * att;
  }

vec3 fogMie(in vec3 pos, vec3 view, vec3 sunDir, float dist) {
  // moon
  float att = 1.0;
  if(sunDir.y < -0.0) {
    sunDir.y = -sunDir.y;
    att = 0.25;
    }
  if(view.y <= -0.0)
    view.y = -view.y;
  float scatt     = 0;
  float rayLength = dist/100.0; //meters
  vec3  color = scatter(pos, view, sunDir, rayLength,scatt) * att;
  color = exposure(color);

  return vec3(color);
  }

void main() {
  vec3  view   = normalize((ubo.mvpInv*vec4(inPos,1.0,1.0)).xyz);
  vec3  sunDir = normalize(ubo.sunDir);
  vec3  pos    = vec3(0,RPlanet+ubo.plPosY,0);

#if defined(FOG)
  vec2  uv       = inPos*0.5+vec2(0.5);
  float z        = texture(depth,uv).r;
  vec4 pos1      = ubo.mvpInv*vec4(inPos,  z,1.0);
  pos1.xyz/=pos1.w;
  vec4 pos0      = ubo.mvpInv*vec4(inPos,0.0,1.0);
  pos0.xyz/=pos0.w;

  float dist     = length(pos1.xyz-pos0.xyz);
  vec3  mie      = fogMie(pos,view,sunDir,dist);
  float fogDens  = volumetricFog(pos0.xyz,pos1.xyz-pos0.xyz);
  const vec3 fogColor = vec3(0.42,0.46,0.53)*fogDens;
  outColor       = vec4(mie+fogColor,fogDens);
#else
  vec3  color    = atmosphere(pos,view,sunDir);
  // Sun
  float alpha    = dot(view,sunDir);
  float spot     = smoothstep(0.0, 1000.0, phase(alpha, 0.9995));
  color += vec3(spot*1000.0);

  // Apply exposure.
  color          = exposure(color);

  float L        = rayIntersect(pos, view, RAtmos);
  vec3  cloudsAt = normalize(pos + view * L);
  vec2  texc     = vec2(cloudsAt.zx)*100.f;
  vec4  day      = clounds(texc);
  vec4  night    = stars(texc);
  vec4  cloud    = mix(day,night,ubo.night);

  color          = mix(color.rgb,cloud.rgb,min(1.0,cloud.a));
  outColor       = vec4(color,1.0);
#endif
  }

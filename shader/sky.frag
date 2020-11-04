#version 450
#extension GL_ARB_separate_shader_objects : enable

const float PI = 3.141592;
const int   iSteps = 16;
const int   jSteps = 8;

layout(std140,binding = 0) uniform UniformBufferObject {
  mat4  mvpInv;
  vec2  dxy0;
  vec2  dxy1;
  vec3  sunDir;
  float night;
  float plPosY;
  } ubo;

layout(binding = 1) uniform sampler2D textureDayL0;
layout(binding = 2) uniform sampler2D textureDayL1;

layout(binding = 3) uniform sampler2D textureNightL0;
layout(binding = 4) uniform sampler2D textureNightL1;

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

vec4 clounds(vec3 texc){
  vec4 cloudDL1 = texture(textureDayL1,texc.zx*0.3+ubo.dxy1);
  vec4 cloudDL0 = texture(textureDayL0,texc.zx*0.3+ubo.dxy0);
#ifdef G1
  return mix(cloudDL0,cloudDL1,cloudDL1.a);
#else
  return (cloudDL0+cloudDL1);
#endif
  }

vec4 stars(vec3 texc){
  vec4 cloudNL1 = texture(textureNightL1,texc.zx*0.3+ubo.dxy1);
  vec4 cloudNL0 = texture(textureNightL0,texc.zx*0.6);
#ifdef G1
  vec4 night    = mix(cloudNL0,cloudNL1,cloudNL1.a);
#else
  vec4 night    = cloudNL0+cloudNL0;
#endif
  return vec4(night.rgb,ubo.night);
  }

float phase(float alpha, float g) {
  float a = 3.0*(1.0-g*g);
  float b = 2.0*(2.0+g*g);
  float c = 1.0+alpha*alpha;
  float d = pow(1.0+g*g-2.0*g*alpha, 1.5);
  d = max(d, 0.00001);
  return (a/b)*(c/d);
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
vec3 scatter(vec3 pos, vec3 view, in vec3 sunDir, out float scat) {
  float L      = rayIntersect(pos, view, RAtmos);
  float mu     = dot(view, sunDir);
  float opmu2  = 1.0 + mu*mu;
  float phaseR = 0.0596831 * opmu2;
  float phaseM = 0.1193662 * (1.0 - g2) * opmu2 / ((2.0 + g2) * pow(1.0 + g2 - 2.0*g*mu, 1.5));

  float depthR = 0.0, depthM = 0.0;
  vec3  R = vec3(0.0), M = vec3(0.0);

  float dl = L / float(iSteps);
  for(int i=0; i<iSteps; ++i) {
    float l = float(i)*dl;
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

  scat = 1.0 - clamp(depthM*1e-5,0.,1.);
  return sunPower * (R * RSC * phaseR + M * kMie * phaseM);
  }

vec3 atmosphere(in vec3 pos, vec3 view, vec3 sunDir) {
  //moon
  float att = 1.0;
  if(sunDir.y < -0.0) {
    sunDir.y = -sunDir.y;
    att = 0.25;
    }
  if(view.y <= -0.0)
    view.y = -view.y;
  float scat  = 0;
  vec3  color = scatter(pos, view, sunDir, scat);
  color *= att;
  return color;
  }

void main() {
  vec4  v      = ubo.mvpInv*vec4(inPos,1.0,1.0);
  vec3  view   = normalize(v.xyz);
  vec3  sunDir = normalize(ubo.sunDir);
  float alpha  = dot(view,sunDir);
  float spot   = smoothstep(0.0, 1000.0, phase(alpha, 0.9995));

  vec3 origin  = vec3(0,RPlanet+ubo.plPosY/100.0,0);
  vec3 color   = atmosphere(origin,view,sunDir);

  color += vec3(spot*1000.0);
  // Apply exposure.
  float env = 1.0;
  color = vec3(env * pow(color, vec3(.7)));

  vec3 texc    = view.xyz/max(abs(view.y*2.0),0.001);
  vec4 day     = clounds(texc);
  //vec3 cloud   = c.rgb*clamp(sunDir.y*2.0+0.1,0.1,1.0);
  vec4 night   = stars(texc);
  vec4 cloud   = mix(day,night,ubo.night);

  color        = mix(color.rgb,cloud.rgb,min(1.0,cloud.a));
  outColor     = vec4(color,1.0);
  }

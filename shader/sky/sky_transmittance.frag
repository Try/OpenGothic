#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#include "sky_common.glsl"

layout(binding = 5) uniform sampler2D textureDayL0;
layout(binding = 6) uniform sampler2D textureDayL1;
layout(binding = 7) uniform sampler2D textureNightL0;
layout(binding = 8) uniform sampler2D textureNightL1;

layout(location = 0) in  vec2 inPos;
layout(location = 0) out vec4 outColor;

const int sunTransmittanceSteps = 40;

vec4 clouds(vec3 at) {
  vec3  cloudsAt = normalize(at);
  vec2  texc     = 2000.0*vec2(atan(cloudsAt.z,cloudsAt.y), atan(cloudsAt.x,cloudsAt.y));

  vec4  cloudDL1 = textureLod(textureDayL1,  texc*0.3+push.dxy1, 10);
  vec4  cloudDL0 = textureLod(textureDayL0,  texc*0.3+push.dxy0, 10);
  vec4  cloudNL1 = textureLod(textureNightL1,texc*0.3+push.dxy1, 10);
  vec4  cloudNL0 = textureLod(textureNightL0,texc*0.6+vec2(0.5), 10); // stars

  vec4 day       = (cloudDL0+cloudDL1)*0.5;
  vec4 night     = (cloudNL0+cloudNL1)*0.5;

  // Clouds (LDR textures from original game) - need to adjust
  day.rgb   = srgbDecode(day.rgb)*push.GSunIntensity;
  day.a     = day.a*(1.0-push.night);

  night.rgb = srgbDecode(night.rgb);
  night.a   = night.a*(push.night);

  vec4 color = mixClr(day,night);
  return color;
  }

vec3 sunTransmittance(vec3 pos, vec3 sunDir) {
  if(rayIntersect(pos, sunDir, RPlanet) > 0.0)
    return vec3(0.0);

  float atmoDist = rayIntersect(pos, sunDir, RAtmos);
  float t = 0.0;

  vec3 transmittance = vec3(1.0);
  for(int i=1; i<=sunTransmittanceSteps; ++i) {
    float t  = (float(i)/sunTransmittanceSteps)*atmoDist;
    float dt = atmoDist/sunTransmittanceSteps;

    vec3 newPos = pos + t*sunDir;

    vec3  rayleighScattering = vec3(0);
    vec3  extinction         = vec3(0);
    float mieScattering      = float(0);
    scatteringValues(newPos, 0, rayleighScattering, mieScattering, extinction);

    transmittance *= exp(-dt*extinction);
    }
  return transmittance;// * (1.0-clouds(sunDir).a);
  }

void main() {
  vec2  uv          = inPos*vec2(0.5)+vec2(0.5);
  float sunCosTheta = 2.0*uv.x - 1.0;
  float sunTheta    = safeacos(sunCosTheta);
  float height      = mix(RPlanet, RAtmos, uv.y);

  vec3  pos         = vec3(0.0, height, 0.0);
  vec3  sunDir      = normalize(vec3(0.0, sunCosTheta, -sin(sunTheta)));

  outColor          = vec4(sunTransmittance(pos, sunDir), 1.0);
  }

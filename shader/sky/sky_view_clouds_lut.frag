#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#include "sky/sky_common.glsl"
#include "sky/clouds.glsl"

#include "scene.glsl"
#include "common.glsl"

// #define SKY_LOD 2

layout(binding = 0, std140) uniform UboScene {
  SceneDesc scene;
  };

layout(binding  = 1) uniform sampler2D skyLUT;
layout(binding  = 2) uniform sampler2D textureDayL0;
layout(binding  = 3) uniform sampler2D textureDayL1;
layout(binding  = 4) uniform sampler2D textureNightL0;
layout(binding  = 5) uniform sampler2D textureNightL1;

layout(location = 0) in  vec2 inPos;
layout(location = 0) out vec4 outColor;

vec3 applyClouds(vec3 skyColor, vec3 refl) {
  float night    = scene.isNight;
  vec3  plPos    = vec3(0,RPlanet,0);

  return applyClouds(skyColor, skyLUT, plPos, scene.sunDir, refl, night,
                     scene.cloudsDir.xy, scene.cloudsDir.zw,
                     textureDayL1,textureDayL0, textureNightL1,textureNightL0);
  }

void main() {
  const vec2  uv      = inPos*vec2(0.5)+vec2(0.5);
  const vec3  viewPos = vec3(0.0, RPlanet + push.plPosY, 0.0);

  float azimuthAngle  = (uv.x - 0.5)*2.0*M_PI;
  // Non-linear mapping of altitude. See Section 5.3 of the paper.
  float adjV;
  if(uv.y < 0.5) {
    float coord = 1.0 - 2.0*uv.y;
    adjV = -coord*coord;
    } else {
    float coord = uv.y*2.0 - 1.0;
    adjV = coord*coord;
    }

  const float height        = length(viewPos);
  const vec3  up            = viewPos / height;
  const float horizonAngle  = safeacos(sqrt(height*height - RPlanet*RPlanet) / height) - 0.5*M_PI;
  const float altitudeAngle = adjV*0.5*M_PI - horizonAngle;

  float cosAltitude = cos(altitudeAngle);
  float sunAltitude = (0.5*M_PI) - acos(dot(push.sunDir, up));

  vec3  sunDir      = vec3(0.0, sin(sunAltitude), -cos(sunAltitude));
  vec3  rayDir      = vec3(cosAltitude*sin(azimuthAngle), sin(altitudeAngle), -cosAltitude*cos(azimuthAngle));

  vec3 sky = textureLod(skyLUT, uv, 0).rgb;
  sky = applyClouds(sky, rayDir);

  outColor = vec4(sky, 1.0);
  }

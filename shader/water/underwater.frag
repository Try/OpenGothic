#version 460
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#include "scene.glsl"
#include "common.glsl"

layout(location = 0) out vec4 outColor;

layout(binding = 0, std140) uniform UboScene {
  SceneDesc scene;
  };
layout(binding = 1) uniform sampler2D zbuffer;
layout(binding = 2) uniform sampler2D underwater;

float hash21(vec2 p) {
  p = fract(p*vec2(123.34,456.21));
  p += dot(p,p+45.32);
  return fract(p.x*p.y);
  }

float noise(vec2 p) {
  vec2 i = floor(p);
  vec2 f = fract(p);
  vec2 u = f*f*(3.0-2.0*f);
  return mix(mix(hash21(i+vec2(0,0)), hash21(i+vec2(1,0)), u.x),
             mix(hash21(i+vec2(0,1)), hash21(i+vec2(1,1)), u.x), u.y);
  }

vec2 barrelUv(vec2 uv) {
  vec2 p = uv*2.0-1.0;
  float r2 = dot(p,p);
  p *= 1.0 + r2*0.035;
  return p*0.5+0.5;
  }

float caustics(vec2 uv, float t) {
  vec2 p = uv*vec2(9.0,5.0);
  float a = noise(p + vec2(t*0.65, t*0.21));
  float b = noise(p*1.7 + vec2(-t*0.31, t*0.57));
  float c = abs(a-b);
  return smoothstep(0.34, 0.03, c) * 0.55;
  }

vec3 underwaterOverlay(vec2 uv, float t) {
  vec2 wobble = vec2(noise(uv*5.0 + t), noise(uv*5.0 - t))*0.008;
  vec2 uv0 = barrelUv(uv + wobble + vec2(t*0.025, -t*0.011));
  vec2 uv1 = barrelUv(uv*1.43 - wobble + vec2(-t*0.017, t*0.031));

  vec3 a = texture(underwater, uv0).rgb;
  vec3 b = texture(underwater, uv1).rgb;
  vec3 m = max(a,b*0.75);
  return smoothstep(vec3(0.22), vec3(0.86), m);
  }

// fixme: copy-paste
vec4 waterScatter(vec3 back, vec3 normal, const float len) {
  const float depth         = len / 7200.0;
  const vec3  waterFloor    = vec3(0.10, 0.24, 0.42);
  const vec3  transmittance = max(exp(-depth * vec3(4.2, 2.05, 0.62)), waterFloor);
#if defined(SCATTERING)
  const float f       = fresnel(scene.sunDir,normal,IorWater);
  const vec3  scatter = vec3(0.004, 0.020, 0.032) * (1.0-exp(-len/8500.0)) +
                        f * scene.sunColor * (1.0-exp(-len/22000.0)) * scene.exposure * 0.018;
  return vec4(scatter*transmittance, 1);
#else
  return vec4(transmittance, 1);
#endif
  }

vec3 unproject(vec4 screen) {
  const vec4 pos4 = scene.viewProjectInv * screen;
  return pos4.xyz/pos4.w;
  }

void main() {
  const vec2  uv        = gl_FragCoord.xy*scene.screenResInv;
  const vec2  fragCoord = uv*2.0-vec2(1.0);
  const float depth     = texelFetch(zbuffer,  ivec2(gl_FragCoord.xy), 0).r;

  const vec3  camPos    = unproject(vec4(0,0,0, 1.0));
  const vec3  wPos      = unproject(vec4(fragCoord.x, fragCoord.y, depth, 1.0));

  const float len = depth>=0.999999 ? 7000.0 : min(length(wPos-camPos), 18000.0);
  vec4 color = waterScatter(vec3(1),vec3(0,1,0),len);
  float t = float(scene.tickCount32) * 0.001;
  float depth01 = clamp(len/9000.0, 0.0, 1.0);
  float vignette = smoothstep(1.35, 0.26, length(uv*2.0-1.0));
  vec3 overlay = underwaterOverlay(uv, t);
  float cst = caustics(uv + overlay.xy*0.018, t);

#if defined(SCATTERING)
  color.rgb += overlay*vec3(0.0015,0.010,0.016);
  color.rgb += cst*vec3(0.0015,0.010,0.014)*(1.0-depth01*0.7);
#else
  vec3 deepBlue = vec3(0.015, 0.14, 0.28);
  vec3 textureShade = vec3(1.0) - overlay*vec3(0.12,0.09,0.04) - cst*vec3(0.03,0.02,0.01);
  color.rgb = mix(color.rgb, deepBlue, depth01*0.18);
  color.rgb *= textureShade;
  color.rgb *= mix(0.55, 1.0, vignette);
  color.rgb = clamp(color.rgb, vec3(0.055,0.16,0.28), vec3(0.86,0.92,1.0));
#endif
  outColor = color;
  }

#version 450

#extension GL_ARB_separate_shader_objects : enable

layout(push_constant, std140) uniform PushConstant {
  float strength;
  float shadowStart;
  float shadowEnd;
  float highlightStart;
  float highlightEnd;
  float shadowR;
  float shadowG;
  float shadowB;
  float highlightR;
  float highlightG;
  float highlightB;
  float vignette;
  float vignetteStart;
  float vignetteEnd;
  } push;

layout(binding = 0) uniform sampler2D src;
layout(location = 0) out vec4 outColor;

void main() {
  const ivec2 fc = ivec2(gl_FragCoord.xy);
  vec4 color = texelFetch(src, fc, 0);

  const float luminance = dot(max(color.rgb, vec3(0.0)), vec3(0.2126, 0.7152, 0.0722));
  const float normalizedLuminance = luminance / (1.0 + luminance);
  const float shadow = 1.0 - smoothstep(push.shadowStart, push.shadowEnd, normalizedLuminance);
  const float highlight = smoothstep(push.highlightStart, push.highlightEnd, normalizedLuminance);

  const vec3 shadowTint = vec3(push.shadowR, push.shadowG, push.shadowB);
  const vec3 highlightTint = vec3(push.highlightR, push.highlightG, push.highlightB);
  vec3 graded = color.rgb * mix(vec3(1.0), shadowTint, shadow);
  graded *= mix(vec3(1.0), highlightTint, highlight);

  const vec2 uv = gl_FragCoord.xy / vec2(textureSize(src, 0));
  const float edge = smoothstep(push.vignetteStart, push.vignetteEnd, length(uv * 2.0 - 1.0));
  graded *= 1.0 - push.vignette * edge;

  color.rgb = mix(color.rgb, graded, clamp(push.strength, 0.0, 1.0));
  outColor = color;
  }

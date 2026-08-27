#version 450

#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0) uniform sampler2D selectionMask;

layout(push_constant, std430) uniform Push {
  vec4  color;
  vec2  viewport;
  float radius;
  float padding;
  } push;

layout(location = 0) out vec4 outColor;

void main() {
  const vec2 uv        = gl_FragCoord.xy / push.viewport;
  // Offsets are measured in final editor pixels even when the renderer uses a
  // reduced internal resolution.
  const vec2 texelSize = 1.0 / push.viewport;
  const float center   = texture(selectionMask,uv).r;

  float expanded = 0.0;
  const int maxRadius = 4;
  for(int y=-maxRadius; y<=maxRadius; ++y) {
    for(int x=-maxRadius; x<=maxRadius; ++x) {
      if(abs(x)>int(push.radius) || abs(y)>int(push.radius))
        continue;
      if(x*x+y*y>int(push.radius*push.radius))
        continue;
      expanded = max(expanded,
                     texture(selectionMask,uv+vec2(x,y)*texelSize).r);
      }
    }

  const float outline = expanded * (1.0-center);
  if(outline<=0.0)
    discard;
  outColor = vec4(push.color.rgb,push.color.a*outline);
  }

#version 450

#extension GL_ARB_separate_shader_objects : enable

#if !defined(POINTS) && !defined(LINES) && !defined(TRIANGLES)
#error im3d primitive type is not defined
#endif

layout(location = 0) in vec4 inPositionSize;
layout(location = 1) in vec4 inColor;

layout(push_constant, std430) uniform Push {
  mat4 viewProject;
  vec2 viewport;
  } push;

#if defined(LINES)
layout(location = 0) out float outSize;
layout(location = 1) out vec4  outColor;
#elif defined(POINTS)
layout(location = 0) out float outSize;
layout(location = 1) out vec4  outColor;
#else
layout(location = 0) out vec4 outColor;
#endif

out gl_PerVertex {
  vec4  gl_Position;
  float gl_PointSize;
  };

const float antialiasing = 2.0;

void main() {
  gl_Position = push.viewProject * vec4(inPositionSize.xyz,1.0);

#if defined(POINTS) || defined(LINES)
  outSize  = max(inPositionSize.w,antialiasing);
  outColor = inColor;
  outColor.a *= smoothstep(0.0,1.0,inPositionSize.w/antialiasing);
#if defined(POINTS)
  gl_PointSize = outSize;
#endif
#else
  outColor = inColor;
#endif
  }

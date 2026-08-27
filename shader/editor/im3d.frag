#version 450

#extension GL_ARB_separate_shader_objects : enable

#if !defined(POINTS) && !defined(LINES) && !defined(TRIANGLES)
#error im3d primitive type is not defined
#endif

#if defined(LINES)
layout(location = 0) in noperspective float inEdgeDistance;
layout(location = 1) in noperspective float inSize;
layout(location = 2) in smooth vec4 inColor;
#elif defined(POINTS)
layout(location = 0) in float inSize;
layout(location = 1) in vec4  inColor;
#else
layout(location = 0) in vec4 inColor;
#endif

layout(location = 0) out vec4 outColor;

const float antialiasing = 2.0;

void main() {
  outColor = inColor;

#if defined(LINES)
  float coverage = abs(inEdgeDistance)/inSize;
  coverage = smoothstep(1.0,1.0-antialiasing/inSize,coverage);
  outColor.a *= coverage;
#elif defined(POINTS)
  float coverage = length(gl_PointCoord.xy-vec2(0.5));
  coverage = smoothstep(0.5,0.5-antialiasing/inSize,coverage);
  outColor.a *= coverage;
#endif
  }

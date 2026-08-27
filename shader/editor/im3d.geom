#version 450

#extension GL_ARB_separate_shader_objects : enable

layout(lines) in;
layout(triangle_strip, max_vertices = 4) out;

layout(push_constant, std430) uniform Push {
  mat4 viewProject;
  vec2 viewport;
  } push;

layout(location = 0) in float inSize[];
layout(location = 1) in vec4  inColor[];

layout(location = 0) out noperspective float outEdgeDistance;
layout(location = 1) out noperspective float outSize;
layout(location = 2) out smooth vec4 outColor;

void emitLineVertex(vec2 position, int endpoint, float edgeDistance) {
  gl_Position     = vec4(position*gl_in[endpoint].gl_Position.w,
                        gl_in[endpoint].gl_Position.zw);
  outEdgeDistance = edgeDistance;
  outSize         = inSize[endpoint];
  outColor        = inColor[endpoint];
  EmitVertex();
  }

void main() {
  vec2 pos0 = gl_in[0].gl_Position.xy/gl_in[0].gl_Position.w;
  vec2 pos1 = gl_in[1].gl_Position.xy/gl_in[1].gl_Position.w;

  vec2 dir = pos0-pos1;
  dir = normalize(vec2(dir.x,dir.y*push.viewport.y/push.viewport.x));
  vec2 tangent = vec2(-dir.y,dir.x);
  vec2 tangent0 = tangent*inSize[0]/push.viewport;
  vec2 tangent1 = tangent*inSize[1]/push.viewport;

  emitLineVertex(pos0-tangent0,0,-inSize[0]);
  emitLineVertex(pos0+tangent0,0, inSize[0]);
  emitLineVertex(pos1-tangent1,1,-inSize[1]);
  emitLineVertex(pos1+tangent1,1, inSize[1]);
  EndPrimitive();
  }

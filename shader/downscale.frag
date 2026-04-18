#version 450

layout(binding  = 0) uniform sampler2D src;
layout(location = 0) out vec4 outColor;

layout(push_constant, std140) uniform Push {
  ivec2 dstSize;
  };

void main() {
  ivec2 srcSize = textureSize(src, 0);

  ivec2 tl = (ivec2(gl_FragCoord.xy+ivec2(0))*srcSize)/dstSize;
  ivec2 br = (ivec2(gl_FragCoord.xy+ivec2(1))*srcSize)/dstSize;

  vec4  color = vec4(0);
  for(int i=tl.x; i<br.x; ++i)
    for(int r=tl.y; r<br.y; ++r) {
      color += texelFetch(src, ivec2(i,r), 0);
      }
  outColor = color/((br.x-tl.x)*(br.y-tl.y));
  }

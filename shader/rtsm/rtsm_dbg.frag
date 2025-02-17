#version 450

#extension GL_GOOGLE_include_directive    : enable
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_samplerless_texture_functions : enable

#include "common.glsl"
#include "scene.glsl"

layout(binding = 0)  uniform utexture2D dbgImage;

layout(location = 0) out vec4 outColor;

float drawInt(in vec2 where, in int n) {
  const float RESOLUTION = 0.75;
  int i=int((where*=RESOLUTION).y);
  if(0<i && i<6) {
    i = 6-i;
    for(int k=1, j=int(where.x); k-->0 || n>0; n/=10)
      if ((j+=4)<3 && j>=0) {
        int x = 0;
        if(i>4)
          x = 972980223;
        else if(i>3)
          x = 690407533;
        else if(i>2)
          x = 704642687;
        else if(i>1)
          x = 696556137;
        else
          x = 972881535;
        return float(x >> (29-j-(n%10)*3)&1);
        }
    }
  return 0;
  }

void main() {
  ivec2 fragCoord = ivec2(gl_FragCoord.xy);
  ivec2 tileSz    = ivec2(32);
  ivec2 tileId    = ivec2(gl_FragCoord.xy)/tileSz;

  uint v = texelFetch(dbgImage, fragCoord, 0).r;

  if(v>0 && drawInt(gl_FragCoord.xy-tileId*tileSz-ivec2(tileSz.x,10), int(v))>0) {
    if(v<=255)
      outColor = vec4(0.9);
    else if(v<=512)
      outColor = vec4(0.9,0.9,0,1);
    else
      outColor = vec4(0.9,0,0,1);
    //return;
    }

  if(fragCoord.x%tileSz.x==0 || fragCoord.y%tileSz.y==0) {
    outColor = vec4(0);
    return;
    }

  if(v==0)
    discard;
  outColor = vec4(0);
  //discard;
  }

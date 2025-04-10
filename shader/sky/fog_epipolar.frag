#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive    : enable
#extension GL_EXT_control_flow_attributes : enable

#include "virtual_shadow/vsm_common.glsl"
#include "virtual_shadow/epipolar_common.glsl"
#include "sky/sky_common.glsl"
#include "scene.glsl"

layout(location = 0) out vec4 outColor;

layout(binding = 0, std140) uniform UboScene    { SceneDesc scene;     };
layout(binding = 1)         uniform texture2D   depth;
layout(binding = 2)         uniform texture2D   rayData;
layout(binding = 3, std430) readonly buffer Ep0 { Epipole   epipole[]; };
layout(binding = 4)         uniform sampler3D   fogLut;

uint  NumSamples   = textureSize(rayData,0).x;
uint  NumSlices    = textureSize(rayData,0).y;
ivec2 fragCoord    = ivec2(gl_FragCoord.xy);
ivec2 viewportSize = textureSize(depth, 0);

int findSampleId(const vec2 pix, uint sliceId) {
  const vec2 rpos    = rayPosition2d(sliceId, viewportSize, NumSlices);
        vec2 sun     = sunPosition2d(scene)*0.5+0.5;

  if(sun.x<0) {
    vec2  dvec = sun - rpos;
    float k    = (0 - rpos.x)/dvec.x;
    sun = rpos + dvec*k;
    }
  if(sun.x>1) {
    vec2  dvec = sun - rpos;
    float k    = (1 - rpos.x)/dvec.x;
    sun = rpos + dvec*k;
    }
  if(sun.y<0) {
    vec2  dvec = sun - rpos;
    float k    = (0 - rpos.y)/dvec.y;
    sun = rpos + dvec*k;
    }
  if(sun.y>1) {
    vec2  dvec = sun - rpos;
    float k    = (1 - rpos.y)/dvec.y;
    sun = rpos + dvec*k;
    }

  //src = rpos
  //const int  steps  = int(max(avec.x, avec.y));
  //const float a   = (fragId+0.5)/float(steps);
  //const vec2  pix = mix(src, dst, a);

  const vec2 src    = rpos;
  const vec2 dst    = sun;
  const vec2 vec    = dst - src;
  const vec2 avec   = abs(vec*viewportSize);
  const int  steps  = int(max(avec.x, avec.y));

  float a = (length(pix-rpos) / length(sun-rpos));
  //const vec2  pix2 = a*steps;
  const vec2  pix3 = mix(src, dst, a);
  return min(int(a*NumSamples + 0.5), int(NumSamples-1));
  // return (a*steps - 0.5)/float(NumSamples);
  }

vec4 fogMst(vec2 uv, float z) {
  float dZ   = linearDepth(      z, scene.clipInfo);
  float d0   = linearDepth(dFogMin, scene.clipInfo);
  float d1   = linearDepth(dFogMax, scene.clipInfo);
  float d    = (dZ-d0)/(d1-d0);
  // return vec4(debugColors[min(int(d*textureSize(fogLut,0).z), textureSize(fogLut,0).z-1)%MAX_DEBUG_COLORS], 0);
  return textureLod(fogLut, vec3(uv,d), 0);
  }

void main() {
  const vec2    pix    = vec2(fragCoord+0.5)/vec2(viewportSize);
  const vec2    inPos  = pix*2.0 - 1.0;
  const uint    id     = sliceId(scene, fragCoord, viewportSize, NumSlices);
  const float   z      = texelFetch(depth, fragCoord, 0).x;

  int   shV = findSampleId(pix, id);
  vec4  val = texelFetch(rayData, ivec2(shV, id), 0);

  vec4  mst = fogMst(pix, z);
  float tr  = mst.a;

  vec3  lum = val.rgb + mst.rgb; // - maxVal;
  lum *= scene.GSunIntensity;
  lum *= scene.exposure;

  outColor = vec4(lum, 1.0-tr);
  // outColor = vec4(dbgColor.rgb, 1);
  // outColor = vec4(inPos*0.5+0.5, 0, 1);
  // outColor = vec4(dbgColor.rgb,1);
  // outColor = vec4(val.rgb,1);

  //outColor = vec4(id/float(NumSlices), shV, 0, 1);
  }

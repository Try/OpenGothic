#version 450

#extension GL_GOOGLE_include_directive    : enable
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_samplerless_texture_functions : enable

#include "virtual_shadow/vsm_common.glsl"
#include "lighting/tonemapping.glsl"
#include "scene.glsl"
#include "common.glsl"

layout(std140, push_constant) uniform Push {
  mat4 viewShadowLwcInv;
  };
layout(binding  = 0, std140) uniform UboScene {
  SceneDesc scene;
  };
layout(binding = 1)         uniform utexture3D  pageTbl;
layout(binding = 2, std430) readonly buffer Pages { VsmHeader header; uint  pageList[]; } vsm;
#if defined(VSM_ATOMIC)
layout(binding = 3) uniform utexture2D  pageData;
#else
layout(binding = 3) uniform texture2D   pageData;
#endif

void main() {
  const vec2 fragCoord            = ((gl_FragCoord.xy+0.5)/2048.0)*2.0 - vec2(1.0);
  //const mat4 viewShadowLwcInv     = inverse(scene.viewShadowLwc[1]);
  const mat4 viewVirtualShadowLwc = scene.viewVirtualShadowLwc;

  vec4  shPos  = viewShadowLwcInv * vec4(fragCoord, 0, 1);
        shPos  = viewVirtualShadowLwc * shPos;
  vec2  shPos0 = shPos.xy/shPos.w;

  int   mip    = VSM_PAGE_MIPS-1;
  vec2  page   = shPos0.xy / (1<<mip);
  if(any(greaterThan(abs(page), vec2(1)))) {
    discard;
    }

  // gl_FragDepth = 0.5;
  gl_FragDepth = shadowTexelFetch(page, mip, pageTbl, pageData);
  }

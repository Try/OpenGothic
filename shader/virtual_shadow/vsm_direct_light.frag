#version 450

#extension GL_GOOGLE_include_directive    : enable
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_samplerless_texture_functions : enable
#extension GL_KHR_memory_scope_semantics : enable

#include "virtual_shadow/vsm_common.glsl"
#include "lighting/tonemapping.glsl"
#include "scene.glsl"
#include "common.glsl"

layout(std140, push_constant) uniform Push {
  float vsmMipBias;
  };
layout(binding  = 0, std140) uniform UboScene {
  SceneDesc scene;
  };
layout(binding = 1)         uniform texture2D   gbufDiffuse;
layout(binding = 2)         uniform utexture2D  gbufNormal;
layout(binding = 3)         uniform texture2D   depth;
layout(binding = 4)         uniform utexture3D  pageTbl;
layout(binding = 5, std430) readonly buffer Pages { VsmHeader header; uint  pageList[]; } vsm;
layout(binding = 6)         uniform texture2D   pageData;
layout(binding = 8, r32ui)  uniform readonly uimage2D dbg;

layout(location = 0) out vec4 outColor;

float drawInt(in vec2 where, in int n) {
  const float RESOLUTION = 0.5;
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

uint hash(uvec3 src) {
  const uint M = 0x5bd1e995u;
  uint h = 1190494759u;
  src *= M; src ^= src>>24u; src *= M;
  h *= M; h ^= src.x; h *= M; h ^= src.y; h *= M; h ^= src.z;
  h ^= h>>13u; h *= M; h ^= h>>15u;
  return h;
  // return (gridPos.x * 18397) + (gridPos.y * 20483) + (gridPos.z * 29303);
  }

int shadowLod(vec2 dx, vec2 dy) {
  float px     = dot(dx, dx);
  float py     = dot(dy, dy);
  float maxLod = 0.5 * log2(max(px, py)); // log2(sqrt()) = 0.5*log2()
  float minLod = 0.5 * log2(min(px, py));

  const float bias = vsmMipBias;
  //return max(0, int((minLod + maxLod)*0.5 + bias + 0.5));
  return max(0, int(minLod + bias));
  }

float shadowTexelFetch(vec2 page, int mip) {
  return shadowTexelFetch(page, mip, pageTbl, pageData);
  }

float shadowTest(vec2 page, int mip, in float refZ) {
  // const float bias = (isATest ? 8 : -1)/(65535.0);
  // const float bias = -(16*LdotN)/(65535.0);
  // const float bias = 2.0/(65535.0); // self-occlusion on trees
  // refZ += bias;
  const float z = shadowTexelFetch(page, mip);
  return (z==0 || z<refZ) ? 1 : 0;
  }

float lambert(vec3 normal) {
  return max(0.0, dot(scene.sunDir,normal));
  }

vec4 worldPos(ivec2 frag, float depth) {
  const vec2 fragCoord = ((frag.xy+0.5)*scene.screenResInv)*2.0 - vec2(1.0);
  const vec4 scr       = vec4(fragCoord.x, fragCoord.y, depth, 1.0);
  return scene.viewProjectLwcInv * scr;
  }

bool planetOcclusion(float viewPos) {
  const float y = RPlanet + max(viewPos*0.1, 0);
  if(rayIntersect(vec3(0,y,0), scene.sunDir, RPlanet)>0)
    return true;
  return false;
  }

vec3 shadowPos(vec4 wpos, vec4 offset) {
  vec4 shPos = scene.viewVirtualShadowLwc * wpos + offset;
  shPos.xyz /= shPos.w;
  return shPos.xyz;
  }

bool calcMipIndex(out vec3 pagePos, out int mip, const float z, const vec3 normal) {
  const vec4 wpos = worldPos(ivec2(gl_FragCoord.xy), z) + vec4(normal*0.002, 0);
  if(planetOcclusion(wpos.y/wpos.w))
    return false;
  vec3 shPos0 = shadowPos(wpos, vec4(0));
  vec2 shPos1 = shadowPos(wpos, scene.vsmDdx).xy;
  vec2 shPos2 = shadowPos(wpos, scene.vsmDdy).xy;

  mip   = shadowLod((shPos1 - shPos0.xy)*VSM_CLIPMAP_SIZE,
                    (shPos2 - shPos0.xy)*VSM_CLIPMAP_SIZE);
  mip   = vsmCalcMipIndex(shPos0.xy, mip);

  pagePos = vec3(shPos0.xy / (1 << mip), shPos0.z);

  return mip<VSM_PAGE_MIPS;
  }

float shadowTest(float z, vec3 normal, out vec3 page, out int mip) {
#if VSM_ENABLE_SUN
  if(!calcMipIndex(page, mip, z, normal))
    return 1;
  return shadowTest(page.xy, mip, page.z);
#else
  return 1;
#endif
  }

void main() {
  outColor = vec4(0,0,0, 1);

#if 1
  if(drawInt(gl_FragCoord.xy-vec2(100,100), int(vsm.header.pageCount))>0) {
    outColor = vec4(1);
    return;
    }
  if(drawInt(gl_FragCoord.xy-vec2(100,150), int(vsm.header.pageOmniCount))>0) {
    outColor = vec4(1);
    return;
    }
  if(drawInt(gl_FragCoord.xy-vec2(100,200), int(vsm.header.meshletCount))>0) {
    outColor = vec4(1);
    return;
    }
#endif

  const ivec2 fragCoord = ivec2(gl_FragCoord.xy);
  const float z         = texelFetch(depth, fragCoord, 0).r;
  if(z==1.0)
    return;

  const vec4  diff   = texelFetch (gbufDiffuse, fragCoord, 0);
  const vec3  normal = normalFetch(gbufNormal,  fragCoord);

  bool isFlat  = false;
  bool isATest = false;
  bool isWater = false;
  decodeBits(diff.a, isFlat, isATest, isWater);

  vec3 page = vec3(0);
  int  mip  = 0;
  const float light  = (isFlat   ? 0 : lambert(normal));
  const float shadow = (light<=0 ? 0 : shadowTest(z, normal, page, mip));

  const vec3  illuminance = scene.sunColor * light * shadow;
  const vec3  linear      = textureAlbedo(diff.rgb);
  const vec3  luminance   = linear * Fd_Lambert * illuminance;

  outColor = vec4(luminance * scene.exposure, 1);

#if 0
  const uint pageId = shadowPageIdFetch(page.xy,mip,pageTbl);
  const uint v = imageLoad(dbg, unpackVsmPageId(pageId)).x;
  outColor.rgb = vec3(v/512.0);
#endif

#if defined(DEBUG)
  const ivec2 pageI = ivec2((page.xy*0.5+0.5)*VSM_PAGE_TBL_SIZE);
  // int  mip   = 0;
  // vec3 color = directLight(page, mip, shPos0.z);
  vec3 color = debugColors[hash(uvec3(pageI,mip)) % debugColors.length()];
  // vec3 color = debugColors[mip % debugColors.length()];
  // color *= (1.0 - shadowTexelFetch(page, mip));
  color *= (shadowTest(page.xy, mip, page.z)*0.9+0.1);
  // vec3 color = vec3(shPos0, 0);
  // vec3 color = vec3(page, 0);
  // vec3 color = vec3(fract(page*VSM_PAGE_TBL_SIZE), 0);
  // vec4 color = shadowTexelFetch(page, mip);
  outColor = vec4(color.xyz,1);
#endif
  }

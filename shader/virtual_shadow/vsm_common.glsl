#ifndef VSM_COMMON_GLSL
#define VSM_COMMON_GLSL

#extension GL_EXT_samplerless_texture_functions : enable

// #define VSM_ATOMIC 1

const int VSM_PAGE_SIZE     = 128;
const int VSM_PAGE_TBL_SIZE = 32;  // small for testing, 64 can be better
const int VSM_PAGE_MIPS     = 16;
const int VSM_PAGE_PER_ROW  = 4096/VSM_PAGE_SIZE;
const int VSM_MAX_PAGES     = VSM_PAGE_PER_ROW * VSM_PAGE_PER_ROW; // 1024;
const int VSM_CLIPMAP_SIZE  = VSM_PAGE_SIZE * VSM_PAGE_TBL_SIZE;

struct VsmHeader {
  uint  pageCount;
  uint  meshletCount;
  uint  counterM;
  uint  counterV;
  ivec4 pageBbox[VSM_PAGE_MIPS];
  };

uint packVsmPageInfo(ivec3 at, ivec2 size) {
  return (at.x & 0xFF) | ((at.y & 0xFF) << 8) | ((at.z & 0xFF) << 16) | ((size.x & 0xF) << 24) | ((size.y & 0xF) << 28);
  }

ivec3 unpackVsmPageInfo(uint p) {
  ivec3 r;
  r.x = int(p      ) & 0xFF;
  r.y = int(p >>  8) & 0xFF;
  r.z = int(p >> 16) & 0xFF;
  return r;
  }

ivec2 unpackVsmPageSize(uint p) {
  ivec2 r;
  r.x = int(p >> 24) & 0xF;
  r.y = int(p >> 28) & 0xF;
  return r;
  }

uint packVsmPageId(uint pageI) {
  return pageI;
  }

uint packVsmPageId(ivec2 at) {
  return (at.x + at.y*VSM_PAGE_PER_ROW);
  }

ivec2 unpackVsmPageId(uint pageId) {
  return ivec2(pageId%VSM_PAGE_PER_ROW, pageId/VSM_PAGE_PER_ROW);
  }

bool vsmPageClip(ivec2 fragCoord, const uint page) {
  ivec4 pg = ivec4(unpackVsmPageId(page) * VSM_PAGE_SIZE, 0, 0);
  pg.zw = pg.xy + ivec2(VSM_PAGE_SIZE);

  ivec2 f = fragCoord;
  if(pg.x <= f.x && f.x<pg.z &&
     pg.y <= f.y && f.y<pg.w)
    return true;
  return false;
  }

float vsmTexelFetch(in utexture2D pageData, const ivec2 pixel) {
  return uintBitsToFloat(texelFetch(pageData, pixel, 0).x);
  }

float vsmTexelFetch(in texture2D pageData, const ivec2 pixel) {
  return texelFetch(pageData, pixel, 0).x;
  }

float shadowTexelFetch(in vec2 page, in int mip, in utexture3D pageTbl,
                       #if defined(VSM_ATOMIC)
                       in utexture2D pageData
                       #else
                       in texture2D pageData
                       #endif
                       ) {
#if 1
  while(mip>=0) {
    if(abs(page.x)>=1 || abs(page.y)>=1)
      break;
    //page-local
    const ivec2 pageI  = ivec2((page*0.5+0.5)*VSM_PAGE_TBL_SIZE);
    const vec2  pageF  = fract((page*0.5+0.5)*VSM_PAGE_TBL_SIZE);
    const ivec2 at     = ivec2(pageF*VSM_PAGE_SIZE);

    //page-global
    const uint  pageId = texelFetch(pageTbl, ivec3(pageI, mip), 0).x >> 16u;
    if(pageId==0xFFFF) {
      page *= 2.0;
      --mip;
      continue;
      }

    const ivec2 pageImageAt = unpackVsmPageId(pageId)*VSM_PAGE_SIZE + at;
    return vsmTexelFetch(pageData, pageImageAt);
    }
  return 0;
#else
  //page-local
  const ivec2 pageI       = ivec2((page*0.5+0.5)*VSM_PAGE_TBL_SIZE);
  const vec2  pageF       = fract((page*0.5+0.5)*VSM_PAGE_TBL_SIZE);
  const ivec2 at          = ivec2(pageF*VSM_PAGE_SIZE);

  //page-global
  const uint  pageId      = texelFetch(pageTbl, ivec3(pageI, mip), 0).x >> 16u;
  if(pageId==0xFFFF)
    return 0;

  const ivec2 pageImageAt = unpackVsmPageId(pageId)*VSM_PAGE_SIZE + at;
  return vsmTexelFetch(pageData, pageImageAt);
#endif
  }

#endif

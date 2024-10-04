#ifndef VSM_COMMON_GLSL
#define VSM_COMMON_GLSL

#extension GL_EXT_samplerless_texture_functions : enable

// #define VSM_ATOMIC 1

const int VSM_PAGE_SIZE     = 128;
const int VSM_PAGE_TBL_SIZE = 32;  // small for testing, 64 can be better
const int VSM_PAGE_MIPS     = 16;
const int VSM_FOG_MIP       = 6;
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

int vsmCalcMipIndex(in vec2 shPos) {
  float x = ceil(log2(max(abs(shPos.x), 1)));
  float y = ceil(log2(max(abs(shPos.y), 1)));
  return int(max(x,y));
  }

int vsmCalcMipIndex(in vec2 shPos, int minMip) {
  float x = ceil(log2(max(abs(shPos.x), 1)));
  float y = ceil(log2(max(abs(shPos.y), 1)));
  return max(minMip, int(max(x,y)));
  }

uint pageIdHash7(ivec3 src) {
  uint x = (src.x & 0x3) << 0;
  uint y = (src.y & 0x3) << 2;
  uint z = (src.z & 0x7) << 4;
  return x | y | z; // 7bit
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
  //page-local
  const ivec2 pageI       = ivec2((page*0.5+0.5)*VSM_PAGE_TBL_SIZE);
  const vec2  pageF       = fract((page*0.5+0.5)*VSM_PAGE_TBL_SIZE);
  const ivec2 at          = ivec2(pageF*VSM_PAGE_SIZE);

  //page-global
  const uint  pageD       = texelFetch(pageTbl, ivec3(pageI, mip), 0).x;
  if(pageD==0)
    return -1;

  const uint  pageId      = pageD >> 16u;
  const ivec2 pageImageAt = unpackVsmPageId(pageId)*VSM_PAGE_SIZE + at;
  return vsmTexelFetch(pageData, pageImageAt);
  }

float shadowTexelFetch_(in vec2 page, in int mip, in utexture3D pageTbl,
                       #if defined(VSM_ATOMIC)
                       in utexture2D pageData
                       #else
                       in texture2D pageData
                       #endif
                       ) {
  while(mip >= 0) {
    float s = shadowTexelFetch(page, mip, pageTbl, pageData);
    if(s>=0)
      return s;
    page *= 2.0;
    mip--;
    }
  return 0;
  }

#endif

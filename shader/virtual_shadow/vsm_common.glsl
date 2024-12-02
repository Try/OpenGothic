#ifndef VSM_COMMON_GLSL
#define VSM_COMMON_GLSL

#extension GL_EXT_samplerless_texture_functions : enable

const int VSM_PAGE_SIZE     = 128;
const int VSM_PAGE_TBL_SIZE = 32;  // small for testing, 64 can be better
const int VSM_PAGE_MIPS     = 16;
//const int VSM_FOG_MIP       = 6;
//const int VSM_PAGE_PER_ROW  = 4096/VSM_PAGE_SIZE;
const int VSM_FOG_MIP       = 5;
const int VSM_PAGE_PER_ROW  = 8192/VSM_PAGE_SIZE;
const int VSM_MAX_PAGES     = VSM_PAGE_PER_ROW * VSM_PAGE_PER_ROW; // 1024;
const int VSM_CLIPMAP_SIZE  = VSM_PAGE_SIZE * VSM_PAGE_TBL_SIZE;

struct VsmHeader {
  uint  pageCount;
  uint  meshletCount;
  uint  counterM;
  uint  counterV;
  uint  pagePerMip[VSM_PAGE_MIPS];
  ivec4 pageBbox[VSM_PAGE_MIPS];
  uint  pageOmniCount;
  };

struct Epipole {
  vec2  rayOrig;
  vec2  rayDir;
  float tMin;
  float tMax;
  float dBegin;
  float dEnd;
  };

uint packVsmPageInfo(ivec3 at, ivec2 size) {
  // 1 : 8 : 8 : 7 : 4 : 4
  return ((at.x & 0xFF) << 1) | ((at.y & 0xFF) << 9) | ((at.z & 0x7F) << 17) | ((size.x & 0xF) << 24) | ((size.y & 0xF) << 28);
  }

ivec3 unpackVsmPageInfo(uint p) {
  ivec3 r;
  r.x = int(p >>  1) & 0xFF;
  r.y = int(p >>  9) & 0xFF;
  r.z = int(p >> 17) & 0x7F;
  return r;
  }

ivec2 unpackVsmPageSize(uint p) {
  ivec2 r;
  r.x = int(p >> 24) & 0xF;
  r.y = int(p >> 28) & 0xF;
  return r;
  }

uint packVsmPageInfo(uint lightId, uint face, ivec2 at, ivec2 size) {
  // 1 : 15 : 4 : 4 : 4 : 4
  uint idx = lightId*6 + face; // 5k omni lights
  return 0x1 | ((idx & 0x7FFF) << 1) | ((at.x & 0xF) << 16) | ((at.y & 0xF) << 20) | ((size.x & 0xF) << 24) | ((size.y & 0xF) << 28);
  }

bool vsmPageIsOmni(uint p) {
  return (p & 0x1)==0x1;
  }

uvec2 unpackLightId(uint p) {
  uint i = uint(p >> 1) & 0x7FFF;
  return uvec2(i/6, i%6);
  }

ivec2 unpackVsmPageInfoProj(uint p) {
  ivec2 r;
  r.x = int(p >> 16) & 0xF;
  r.y = int(p >> 20) & 0xF;
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
#if 1
  float d  = max(abs(shPos.x), abs(shPos.y));
  uint  id = uint(d);
  return findMSB(id)+1;
#else
  float x = ceil(log2(max(abs(shPos.x), 1)));
  float y = ceil(log2(max(abs(shPos.y), 1)));
  return int(max(x,y));
#endif
  }

int vsmCalcMipIndex(in vec2 shPos, int minMip) {
  return max(vsmCalcMipIndex(shPos), minMip);
  }

int vsmCalcMipIndexFog(in vec2 shPos) {
  float d  = max(abs(shPos.x), abs(shPos.y));
  uint  id = uint(d * 16.0);
  return clamp(findMSB(id)+1, VSM_FOG_MIP, VSM_FOG_MIP+4);
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

uint shadowPageIdFetch(in vec2 page, in int mip, in utexture3D pageTbl) {
  //page-local
  const ivec2 pageI       = ivec2((page*0.5+0.5)*VSM_PAGE_TBL_SIZE);
  const vec2  pageF       = fract((page*0.5+0.5)*VSM_PAGE_TBL_SIZE);
  const ivec2 at          = ivec2(pageF*VSM_PAGE_SIZE);

  //page-global
  const uint  pageD       = texelFetch(pageTbl, ivec3(pageI, mip), 0).x;
  if(pageD==0)
    return -1;

  return pageD >> 16u;
  }

float shadowTexelFetch(in vec2 page, in int mip, in utexture3D pageTbl, in texture2D pageData) {
  //page-local
  const ivec2 pageI       = ivec2((page*0.5+0.5)*VSM_PAGE_TBL_SIZE);
  const vec2  pageF       = fract((page*0.5+0.5)*VSM_PAGE_TBL_SIZE);
  const ivec2 at          = ivec2(pageF*VSM_PAGE_SIZE);

  //page-global
  const uint  pageD       = texelFetch(pageTbl, ivec3(pageI, mip), 0).x;
  if(pageD==0)
    return -1;

  const uint  pageId      = pageD >> 16u;

  // const uint  pageId      = shadowPageIdFetch(page, mip, pageTbl);
  const ivec2 pageImageAt = unpackVsmPageId(pageId)*VSM_PAGE_SIZE + at;
  return vsmTexelFetch(pageData, pageImageAt);
  }

float shadowTexelFetch_(in vec2 page, in int mip, in utexture3D pageTbl, in texture2D pageData) {
  while(mip >= 0) {
    float s = shadowTexelFetch(page, mip, pageTbl, pageData);
    if(s>=0)
      return s;
    page *= 2.0;
    mip--;
    }
  return 0;
  }

uint vsmLightDirToFace(vec3 d) {
  const vec3 ad = abs(d);
  if(ad.x > ad.y && ad.x > ad.z)
    return d.x>=0 ? 0 : 1;
  if(ad.y > ad.x && ad.y > ad.z)
    return d.y>=0 ? 2 : 3;
  if(ad.z > ad.x && ad.z > ad.y)
    return d.z>=0 ? 4 : 5;
  return 0;
  }

#endif

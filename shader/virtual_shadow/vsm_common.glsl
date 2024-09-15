#ifndef VSM_COMMON_GLSL
#define VSM_COMMON_GLSL

const int VSM_MAX_PAGES     = 1024;
const int VSM_PAGE_SIZE     = 128;
const int VSM_PAGE_TBL_SIZE = 32;   // small for testing, 64 can be better
const int VSM_PAGE_PER_ROW  = 32;
const int VSM_CLIPMAP_SIZE  = VSM_PAGE_SIZE * VSM_PAGE_TBL_SIZE;

struct VsmHeader {
  uint pageCount;
  uint meshletCount;
  uint counterM;
  uint counterV;
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

#endif

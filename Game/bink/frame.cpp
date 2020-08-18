#include "frame.h"

using namespace Bink;

void Frame::Plane::setSize(uint32_t iw, uint32_t ih) {
  dat.resize(iw*ih);
  w = iw;
  h = ih;
  }

void Frame::Plane::getPixels8x8(uint32_t rx, uint32_t ry, uint8_t* out) const {
  const uint8_t* d = dat.data();
  uint32_t yMax = std::min<uint32_t>(8,h-ry);
  uint32_t xMax = std::min<uint32_t>(8,w-rx);

  for(uint32_t y=0; y<yMax; ++y) {
    for(uint32_t x=0; x<xMax; ++x) {
      out[x+y*8] = d[(x+rx) + (y+ry)*w];
      }
    }
  }

void Frame::Plane::getBlock8x8(uint32_t bx, uint32_t by, uint8_t* out) const {
  getPixels8x8(bx*8,by*8,out);
  }

void Frame::Plane::putBlock8x8(uint32_t bx, uint32_t by, const uint8_t* in) {
  uint8_t* d    = dat.data();
  uint32_t yMax = std::min<uint32_t>(8,h-by*8);
  uint32_t xMax = std::min<uint32_t>(8,w-bx*8);

  for(uint32_t y=0; y<yMax; ++y) {
    for(uint32_t x=0; x<xMax; ++x) {
      d[(x+bx*8) + (y+by*8)*w] = in[x+y*8];
      }
    }
  }

void Frame::Plane::putScaledBlock(uint32_t bx, uint32_t by, const uint8_t* in) {
  uint8_t* d    = dat.data();
  uint32_t yMax = std::min<uint32_t>(16,h-by*8);
  uint32_t xMax = std::min<uint32_t>(16,w-bx*8);

  for(uint32_t y=0; y<yMax; ++y) {
    uint32_t y2 = y/2;
    for(uint32_t x=0; x<xMax; ++x) {
      d[(x+bx*8) + (y+by*8)*w] = in[(x/2)+y2*8];
      }
    }
  }

uint8_t Frame::Plane::at(uint32_t x, uint32_t y) const {
  return dat[x + y*w];
  }


Frame::Frame() {
  }

void Frame::setSize(uint32_t w, uint32_t h) {
  for(auto& i:planes)
    i.setSize(w,h);
  }

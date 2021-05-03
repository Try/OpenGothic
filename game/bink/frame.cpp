#include "frame.h"

#include <algorithm>
#include <cstring>

using namespace Bink;

void Frame::Plane::setSize(uint32_t iw, uint32_t ih) {
  uint32_t w16 = ((iw+15)/16)*16; // align to largest block size
  uint32_t h16 = ((ih+15)/16)*16;

  dat.resize(w16*h16);
  w = iw;
  h = ih;
  stride = w16;
  }

void Frame::Plane::getPixels8x8(uint32_t rx, uint32_t ry, uint8_t* out) const {
  const uint8_t* d = dat.data();

  for(uint32_t y=0; y<8; ++y) {
    for(uint32_t x=0; x<8; ++x) {
      out[x+y*8] = d[(x+rx) + (y+ry)*stride];
      }
    }
  }

void Frame::Plane::getBlock8x8(uint32_t bx, uint32_t by, uint8_t* out) const {
  getPixels8x8(bx*8,by*8,out);
  }

void Frame::Plane::putBlock8x8(uint32_t bx, uint32_t by, const uint8_t* in) {
  uint8_t* d = dat.data();

  for(uint32_t y=0; y<8; ++y) {
    for(uint32_t x=0; x<8; ++x) {
      d[(x+bx*8) + (y+by*8)*stride] = in[x+y*8];
      }
    }
  }

void Frame::Plane::putScaledBlock(uint32_t bx, uint32_t by, const uint8_t* in) {
  uint8_t* d = dat.data();
  for(uint32_t y=0; y<16; ++y) {
    uint32_t y2 = y/2;
    for(uint32_t x=0; x<16; ++x) {
      d[(x+bx*8) + (y+by*8)*stride] = in[(x/2)+y2*8];
      }
    }
  }

void Frame::Plane::fill(uint8_t v) {
  std::memset(dat.data(),v,dat.size());
  }

uint8_t Frame::Plane::at(uint32_t x, uint32_t y) const {
  return dat[x + y*stride];
  }


Frame::Frame() {
  aud.reserve(4096);
  }

const Frame::Audio& Frame::audio(uint8_t id) const {
  return aud[id];
  }

void Frame::setSize(uint32_t w, uint32_t h) {
  planes[0].setSize(w,h);
  planes[1].setSize(w/2,h/2);
  planes[2].setSize(w/2,h/2);
  planes[3].setSize(w,h);
  }

void Frame::setAudioChannels(uint8_t count) {
  aud.resize(count);
  }

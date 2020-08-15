#include "binkvideo.h"

#include <stdexcept>
#include <iostream>
#include <cmath>
#include <cstring>

static uint32_t AV_RL32(const char* v) {
  uint32_t ret=0;
  while(*v) {
    ret *=256;
    ret += *v;
    ++v;
    }
  return ret;
  }

static int av_log2(unsigned v) {
  return int(std::log2(v));
  }

struct BinkVideo::BitStream {
  BitStream(const uint8_t* data, size_t bitCount):data(data),bitCount(bitCount),byteCount(bitCount >> 3){}

  void skip(size_t n) {
    if(at+n>bitCount)
      throw std::runtime_error("io error");
    at+=n;
    }

  uint32_t getBit() {
    if(at>=bitCount)
      throw std::runtime_error("io error");
    uint8_t d    = data[at >> 3];
    size_t  mask = 1 << (at & 7);
    at++;
    return (d & mask) ? 1 : 0;
    }

  uint32_t getBits(int n) {
    if(at+n>=bitCount)
      throw std::runtime_error("io error");
    uint32_t v = fetch32();
    at+=n;
    return v & ((1<<n)-1);
    }

  uint32_t fetch32() {
    size_t byteAt = at >> 3;
    size_t offset = at & 7;

    uint8_t buf[8]={};
    for(size_t i=0; i<5; ++i) {
      if(i+byteAt>=byteCount)
        break;
      buf[i] = data[byteAt+i];
      }
    uint64_t& v64 = *reinterpret_cast<uint64_t*>(buf);
    v64 = v64 >> offset;
    return v64 & uint32_t(-1);
    }

  size_t position() const { return at; }
  size_t bitsLeft() const { return bitCount-at; };

  const uint8_t* data      = nullptr;
  size_t         at        = 0;
  size_t         bitCount  = 0;
  size_t         byteCount = 0;
  };

BinkVideo::BinkVideo(const char* file) {
  packet.reserve(4*1024*1024);
  fin = fopen(file,"rb");

  uint32_t codec = rl32();
  if(codec!=BINK_TAG)
    throw std::runtime_error("invalid codec");

  uint32_t file_size = rl32() + 8;
  uint32_t duration  = rl32();
  if(rl32() > file_size)
    throw std::runtime_error("invalid header: largest frame size greater than file size");
  (void)rl32();

  width  = rl32();
  height = rl32();

  uint32_t fps_num = rl32();
  uint32_t fps_den = rl32();
  if(fps_num == 0 || fps_den == 0) {
    char buf[256]={};
    std::snprintf(buf, sizeof(buf), "invalid header: invalid fps (%d / %d)", fps_num, fps_den);
    throw std::runtime_error(buf);
    }

  flags = BinkVidFlags(rl32());

  uint32_t num_audio_tracks = rl32(); // 1

  const uint32_t signature = (codec & 0xFFFFFF);
  revision = uint8_t((codec >> 24) % 0xFF);

  if((signature == AV_RL32("BIK") && (revision == 'k')) ||
     (signature == AV_RL32("KB2") && (revision == 'i' || revision == 'j' || revision == 'k')))
    (void)rl32(); // unknown new field

  if(num_audio_tracks>0) {
    skip(4*num_audio_tracks); /* max decoded size */
    audio.resize(num_audio_tracks);

    for(uint32_t i=0; i<num_audio_tracks; i++) {
      audio[i].sample_rate = rl16();
      //avpriv_set_pts_info(ast, 64, 1, ast->codecpar->sample_rate);
      uint16_t flags = rl16();
      //ast->codecpar->codec_id = flags & BINK_AUD_USEDCT ?  AV_CODEC_ID_BINKAUDIO_DCT : AV_CODEC_ID_BINKAUDIO_RDFT;
      if(flags & BINK_AUD_STEREO)
        audio[i].channels = 2; else
        audio[i].channels = 1;
      }

    for(uint32_t i=0; i<num_audio_tracks; i++)
      audio[i].id = rl32();
    }

  // frame index table
  uint32_t next_pos     = 0;
  uint32_t pos          = 0;
  bool     nextKeyframe = true;
  bool     keyframe     = false;

  next_pos = rl32();
  for(uint32_t i = 0; i<duration; i++) {
    pos      = next_pos;
    keyframe = nextKeyframe;
    if(i+1==duration) {
      next_pos      = file_size;
      nextKeyframe = false;
      } else {
      next_pos     = rl32();
      nextKeyframe = next_pos & 1;
      }
    pos      &= ~1;
    next_pos &= ~1;

    if(next_pos <= pos)
      throw std::runtime_error("invalid frame index table");

    Index id;
    id.pos      = pos;
    id.size     = next_pos - pos;
    id.keyFrame = keyframe;
    index.push_back(id);
    }

  if(index.size()>0)
    seek(index[0].pos + smush_size); else
    skip(4);
  }

BinkVideo::~BinkVideo() {
  fclose(fin);
  }

void BinkVideo::nextFrame() {
  readPacket();
  frameCounter++;
  }

uint32_t BinkVideo::rl32() {
  uint32_t ret = 0;
  read(&ret,4);
  return ret;
  }

uint16_t BinkVideo::rl16() {
  uint16_t ret = 0;
  read(&ret,2);
  return ret;
  }

void BinkVideo::read(void* v, size_t cnt) {
  if(1!=fread(v,cnt,1,fin))
    throw std::runtime_error("io error");
  }

void BinkVideo::skip(size_t cnt) {
  fseek(fin,cnt,SEEK_CUR);
  }

void BinkVideo::seek(size_t cnt) {
  fseek(fin,cnt,SEEK_SET);
  }

void BinkVideo::readPacket() {
  const Index& id = index[frameCounter];

  uint32_t videoSize = id.size;
  for(size_t i=0; i<audio.size(); ++i) {
    uint32_t audioSize = rl32();
    if(audioSize+4 > videoSize) {
      char buf[256] = {};
      std::snprintf(buf,sizeof(buf),"audio size in header (%u) > size of packet left (%u)", audioSize, videoSize);
      throw std::runtime_error(buf);
      }
    if(audioSize >= 4) { // This doesn't look good
      packet.resize(audioSize);
      read(packet.data(),packet.size());
      //TODO: parse packet
      } else {
      skip(audioSize);
      }
    videoSize -= (audioSize+4);
    }

  packet.resize(videoSize);
  read(packet.data(),packet.size()); //84
  parseFrame(id,packet);
  }

void BinkVideo::merge(BitStream& gb, uint8_t *dst, uint8_t *src, int size) {
  uint8_t *src2 = src + size;
  int size2 = size;

  do {
    if(!gb.getBit()) {
      *dst++ = *src++;
      size--;
      } else {
      *dst++ = *src2++;
      size2--;
      }
    } while (size && size2);

  while (size--)
    *dst++ = *src++;
  while (size2--)
    *dst++ = *src2++;
  }

void BinkVideo::initLengths(int width, int bw) {
  width = ((width+7)/8)*8;

  bundle[BINK_SRC_BLOCK_TYPES].len     = av_log2((width >> 3) + 511) + 1;
  bundle[BINK_SRC_SUB_BLOCK_TYPES].len = av_log2((width >> 4) + 511) + 1;
  bundle[BINK_SRC_COLORS].len          = av_log2(bw*64 + 511) + 1;
  bundle[BINK_SRC_INTRA_DC].len =
      bundle[BINK_SRC_INTER_DC].len =
      bundle[BINK_SRC_X_OFF].len =
      bundle[BINK_SRC_Y_OFF].len = av_log2((width >> 3) + 511) + 1;

  bundle[BINK_SRC_PATTERN].len = av_log2((bw << 3) + 511) + 1;
  bundle[BINK_SRC_RUN].len     = av_log2(bw*48 + 511) + 1;
  }

void BinkVideo::parseFrame(const BinkVideo::Index& id, const std::vector<uint8_t>& data) {
  const bool   swap_planes = (revision >= 'h');
  const size_t bits_count  = data.size()<<3;

  BitStream gb(data.data(),bits_count);

  if((flags&BINK_FLAG_ALPHA) == BINK_FLAG_ALPHA) {
    if(revision >= 'i')
      gb.skip(32);
    decodePlane(gb,3,false);
    }
  if(revision >= 'i')
    gb.skip(32);

  for(int plane=0; plane<3; plane++) {
    const int planeId = (!plane || !swap_planes) ? plane : (plane ^ 3);

    if(revision>'b') {
      decodePlane(gb, planeId, plane!=0);
      } else {
      //decodePlaneB(gb, planeId, frameCounter==0, plane!=0);
      }

    if(gb.position()>=bits_count)
      break;
    }
  }

void BinkVideo::decodePlane(BitStream& gb, int planeId, bool chroma) {
  int bw     = chroma ? (this->width  + 15) >> 4 : (this->width  + 7) >> 3;
  //int bh     = chroma ? (this->height + 15) >> 4 : (this->height + 7) >> 3;
  int width  = this->width  >> (chroma ? 1 : 0);
  //int height = this->height >> (chroma ? 1 : 0);

  if(revision == 'k' && gb.getBit()) {
    /*
    int fill = get_bits(gb, 8);

    dst = frame->data[plane_idx];

    for (i = 0; i < height; i++)
      memset(dst + i * stride, fill, width);
    goto end;
    */
    }

  initLengths(std::max(width,8),bw);
  for(int i=0; i<BINK_NB_SRC; i++)
    readBundle(gb,i);
  }

void BinkVideo::readBundle(BitStream& gb, int bundle_num) {
  if(bundle_num == BINK_SRC_COLORS) {
    for(int i=0; i<16; i++)
      readTree(gb, col_high[i]);
    col_lastval = 0;
    }

  if(bundle_num != BINK_SRC_INTRA_DC && bundle_num != BINK_SRC_INTER_DC)
    readTree(gb, bundle[bundle_num].tree);

  bundle[bundle_num].cur_dec =
      bundle[bundle_num].cur_ptr = bundle[bundle_num].data;
  }

void BinkVideo::readTree(BitStream& gb, Tree& tree) {
  uint8_t tmp1[16] = { 0 }, tmp2[16], *in = tmp1, *out = tmp2;

  tree.vlc_num = gb.getBits(4);
  if(0==tree.vlc_num) {
    for(uint8_t i=0; i<16; i++)
      tree.syms[i] = i;
    return;
    }

  if(gb.getBit()) {
    uint32_t len = gb.getBits(3);
    for(uint32_t i=0; i<=len; i++) {
      tree.syms[i]       = uint8_t(gb.getBits(4));
      tmp1[tree.syms[i]] = 1;
      }
    for(uint8_t i=0; i<16 && len<16-1; i++)
      if(!tmp1[i])
        tree.syms[++len] = i;
    } else {
    uint32_t len = gb.getBits(2);
    for(uint32_t i=0; i<16; i++)
      in[i] = uint8_t(i);
    for(uint32_t i=0; i<=len; i++) {
      int size = 1 << i;
      for(int t=0; t<16; t+=(size<<1))
        merge(gb, out + t, in + t, size);
      std::swap(in, out);
      }
    std::memcpy(tree.syms, in, 16);
    }
  }

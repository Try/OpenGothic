#pragma once

#include <cstdint>
#include <stdexcept>
#include <vector>

#include "frame.h"

namespace Bink {

class Video;

class SoundDecodingException : public std::runtime_error {
  SoundDecodingException(const char* msg):std::runtime_error(msg){};
  friend class Video;
  };

class VideoDecodingException : public std::runtime_error {
  VideoDecodingException(const char* msg):std::runtime_error(msg){};
  friend class Video;
  };

class Video final {
  public:
    enum {
      BINK_TAG = 1766541634
      };

    enum Format {
      AV_PIX_FMT_YUVA420P,
      AV_PIX_FMT_YUV420P
      };

    class Input {
      public:
        virtual ~Input()=default;
        virtual void read(void* dest, size_t count) = 0;
        virtual void seek(size_t pos)               = 0;
        virtual void skip(size_t count)             = 0;
      };

    struct FrameRate final {
      uint32_t num = 1;
      uint32_t den = 1;
      };

    struct Audio {
      uint16_t sampleRate = 44100;
      bool     isMono     = false;
      };

    explicit Video(Input* file);
    Video(const Video&) = delete;
    ~Video();

    const Frame& nextFrame();
    size_t       frameCount() const;
    size_t       currentFrame() const { return frameCounter; }

    const FrameRate& fps() const { return fRate; }

    size_t       audioCount()     const { return aud.size(); }
    const Audio& audio(uint8_t i) const { return audProp[i]; }

    struct FFTComplex final {
      float re, im;
      };

  private:
    enum BinkVidFlags : uint32_t {
      BINK_FLAG_NONE  = 0,
      BINK_FLAG_ALPHA = 0x00100000,
      BINK_FLAG_GRAY  = 0x00020000,
      };
    enum BinkAudFlags : uint16_t {
      BINK_AUD_16BITS = 0x4000,
      BINK_AUD_STEREO = 0x2000,
      BINK_AUD_USEDCT = 0x1000,
      };

    enum Sources {
      BINK_SRC_BLOCK_TYPES = 0, // 8x8 block types
      BINK_SRC_SUB_BLOCK_TYPES, // 16x16 block types (a subset of 8x8 block types)
      BINK_SRC_COLORS,          // pixel values used for different block types
      BINK_SRC_PATTERN,         // 8-bit values for 2-colour pattern fill
      BINK_SRC_X_OFF,           // X components of motion value
      BINK_SRC_Y_OFF,           // Y components of motion value
      BINK_SRC_INTRA_DC,        // DC values for intrablocks with DCT
      BINK_SRC_INTER_DC,        // DC values for interblocks with DCT
      BINK_SRC_RUN,             // run lengths for special fill block

      BINK_NB_SRC
      };

    enum BlockTypes {
      SKIP_BLOCK = 0, // skipped block
      SCALED_BLOCK,   // block has size 16x16
      MOTION_BLOCK,   // block is copied from previous frame with some offset
      RUN_BLOCK,      // block is composed from runs of colours with custom scan order
      RESIDUE_BLOCK,  // motion block with some difference added
      INTRA_BLOCK,    // intra DCT block
      FILL_BLOCK,     // block is filled with single colour
      INTER_BLOCK,    // motion block with DCT applied to the difference
      PATTERN_BLOCK,  // block is filled with two colours following custom pattern
      RAW_BLOCK,      // uncoded 8x8 block
      };

    enum {
      DC_START_BITS       = 11,
      MAX_CHANNELS        = 2,
      BINK_BLOCK_MAX_SIZE = (MAX_CHANNELS << 11)
      };

    struct AudioTrack {
      uint32_t id        =0;
      uint16_t sampleRate=0;
      uint8_t  channels  =0;
      };

    struct Index {
      uint32_t pos      = 0;
      uint32_t size     = 0;
      bool     keyFrame = false;
      };

    struct Tree final {
      int     vlc_num  = 0;  // tree number (in bink_trees[])
      uint8_t syms[16] = {}; // leaf value to symbol mapping
      };

    struct Bundle final {
      int                  len = 0;            // length of number of entries to decode (in bits)
      Tree                 tree;               // Huffman tree-related data
      std::vector<uint8_t> data;               // buffer for decoded symbols
      uint8_t*             data_end = nullptr; // buffer end
      uint8_t*             cur_dec  = nullptr; // pointer to the not yet decoded part of the buffer
      uint8_t*             cur_ptr  = nullptr; // pointer to the data that is not read from buffer yet
      };

    struct AudioCtx final {
      AudioCtx(uint16_t sampleRate, uint8_t channelsCnt, bool isDct);

      std::vector<float> samples[MAX_CHANNELS];

      uint32_t   sampleRate = 44100;
      uint8_t    channelsCnt = 1;
      float      root        = 0.f;
      uint32_t   frameLen    = 0;
      uint32_t   overlapLen  = 0;
      int        nbits       = 0;
      const bool isDct       = false; // discrete cosine transforms

      uint32_t   bands[26]  = {};
      uint32_t   numBands   = 0;

      const float* tcos = nullptr;
      const float* tsin = nullptr;

      std::vector<uint16_t> revtab;
      std::vector<uint32_t> revtab32;
      std::vector<float>    csc2;

      std::vector<FFTComplex> tmpBuf;
      float                   previous[MAX_CHANNELS][BINK_BLOCK_MAX_SIZE/16];  // coeffs from previous audio block

      bool                    first = true;
      };

    struct BitStream;

    uint32_t rl32();
    uint16_t rl16();
    void     merge(BitStream& gb, uint8_t *dst, uint8_t *src, int size);

    void     decodeInit();
    void     decodeAudioInit(AudioCtx& s);

    int      setIdx (BitStream& gb, int code, int& n, int& nb_bits, const int16_t (*table)[2]);
    uint8_t  getHuff(BitStream& gb, const Tree& tree);
    int      getVlc2(BitStream& gb, int16_t (*table)[2], int bits, int max_depth);
    void     readPacket();
    void     parseFrame(const std::vector<uint8_t>& data);
    void     decodePlane(BitStream& gb, int planeId, bool chroma);
    void     initLengths(int width, int bw);
    void     readBundle(BitStream& gb, int bundle_num);
    void     readTree(BitStream& gb, Tree& tree);

    void     readBlockTypes  (BitStream& gb, Bundle& b);
    void     readColors      (BitStream& gb, Bundle& b);
    void     readPatterns    (BitStream& gb, Bundle& b);
    void     readMotionValues(BitStream& gb, Bundle& b);
    void     readDcs         (BitStream& gb, Bundle& b, int start_bits, int has_sign);
    void     readRuns        (BitStream& gb, Bundle& b);
    int      readDctCoeffs   (BitStream& gb, int32_t block[], const uint8_t* scan,
                              int& coef_count_, int coef_idx[], int q);
    void     unquantizeDctCoeffs(int32_t block[], const uint32_t quant[],
                                 int coef_count, int coef_idx[], const uint8_t* scan);
    void     readResidue     (BitStream& gb, int16_t block[], int masks_count);
    int      getValue(Sources bundle);
    template<class T>
    static bool checkReadVal(BitStream& gb, Bundle& b, T& t);

    void     initFfCosTabs(size_t index);
    void     parseAudio(const std::vector<uint8_t>& data, size_t id);
    void     parseAudioBlock(BitStream& gb, AudioCtx& track);
    void     dctCalc3C (AudioCtx& aud, float* data);
    void     rdftCalcC (AudioCtx& aud, float* data, bool negativeSign);

    void     fftPermute(AudioCtx& aud, FFTComplex* z);
    void     fftCalc   (const AudioCtx& aud, FFTComplex* z);

    Input*       fin            = nullptr;
    uint8_t      revision       = 0;
    BinkVidFlags flags          = BINK_FLAG_NONE;
    uint32_t     width          = 0;
    uint32_t     height         = 0;
    uint32_t     smush_size     = 0;
    bool         swap_planes    = false;

    std::vector<Audio>      audProp;
    std::vector<AudioCtx>   aud;
    std::vector<Index>      index;

    FrameRate               fRate;
    Frame                   frames[2] = {};

    std::vector<uint8_t>    packet;
    uint32_t                frameCounter = 0;

    // video
    Bundle                  bundle[BINK_NB_SRC] = {};
    Tree                    col_high[16];         // trees for decoding high nibble in "colours" data type
    int                     col_lastval = 0;      // value of last decoded high nibble in "colours" data type

    // sound
    float                   quantTable[96] = {};
  };

}

#pragma once

#include <cstdint>
#include <vector>

#include "frame.h"

namespace Bink {

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

    explicit Video(Input* file);
    Video(const Video&) = delete;
    ~Video();

    const Frame& nextFrame();
    size_t       frameCount() const;
    size_t       currentFrame() const { return frameCounter; }

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
      DC_START_BITS = 11,
      };

    struct AudioTrack {
      uint32_t id         =0;
      uint16_t sample_rate=0;
      uint8_t  channels   =0;
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

    struct BitStream;

    uint32_t rl32();
    uint16_t rl16();
    void     merge(BitStream& gb, uint8_t *dst, uint8_t *src, int size);

    void     decodeInit();
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

    Input*       fin            = nullptr;
    uint8_t      revision       = 0;
    BinkVidFlags flags          = BINK_FLAG_NONE;
    uint32_t     width          = 0;
    uint32_t     height         = 0;
    uint32_t     smush_size     = 0;
    bool         swap_planes    = false;

    std::vector<AudioTrack> audio;
    std::vector<Index>      index;

    std::vector<uint8_t>    packet;
    uint32_t                frameCounter = 0;
    Bundle                  bundle[BINK_NB_SRC] = {};
    Tree                    col_high[16];         // trees for decoding high nibble in "colours" data type
    int                     col_lastval = 0;      // value of last decoded high nibble in "colours" data type

    Frame                   frames[2] = {};
  };

}

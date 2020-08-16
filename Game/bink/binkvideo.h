#pragma once

#include <cstdint>
#include <cstdio>
#include <vector>

class BinkVideo {
  public:
    enum {
      BINK_TAG = 1766541634
      };

    BinkVideo(const char* file);
    BinkVideo(const BinkVideo&) = delete;
    ~BinkVideo();

    void nextFrame();

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
      int     len;       // length of number of entries to decode (in bits)
      Tree    tree;      // Huffman tree-related data
      uint8_t *data;     // buffer for decoded symbols
      uint8_t *data_end; // buffer end
      uint8_t *cur_dec;  // pointer to the not yet decoded part of the buffer
      uint8_t *cur_ptr;  // pointer to the data that is not read from buffer yet
      };

    struct BitStream;

    uint32_t rl32();
    uint16_t rl16();
    void     read(void* v, size_t cnt);
    void     skip(size_t cnt);
    void     seek(size_t cnt);
    void     merge(BitStream& gb, uint8_t *dst, uint8_t *src, int size);

    void     readPacket();
    void     parseFrame(const Index& id, const std::vector<uint8_t>& data);
    void     decodePlane(BitStream& gb, int planeId, bool chroma);
    void     initLengths(int width, int bw);
    void     readBundle(BitStream& gb, int bundle_num);
    void     readTree(BitStream& gb, Tree& tree);

    FILE*        fin            = nullptr;
    uint8_t      revision       = 0;
    BinkVidFlags flags          = BINK_FLAG_NONE;
    uint32_t     width          = 0;
    uint32_t     height         = 0;
    uint32_t     smush_size     = 0;

    std::vector<AudioTrack> audio;
    std::vector<Index>      index;

    std::vector<uint8_t>    packet;
    uint32_t                frameCounter = 0;
    Bundle                  bundle[BINK_NB_SRC] = {};
    Tree                    col_high[16];         // trees for decoding high nibble in "colours" data type
    int                     col_lastval = 0;      // value of last decoded high nibble in "colours" data type
  };


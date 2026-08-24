#pragma once

#include <vector>
#include <cstdint>
#include <cstddef>

namespace Bink {

class Video;

class Frame final {
  public:
    Frame();
    Frame(Frame&&) = default;
    Frame& operator = (Frame&&) = default;

    class Plane final {
      public:
        void getPixels8x8  (uint32_t rx, uint32_t ry, uint8_t* out) const;

        void getBlock8x8   (uint32_t x, uint32_t y, uint8_t* out) const;
        void putBlock8x8   (uint32_t x, uint32_t y, const uint8_t* in);
        void putScaledBlock(uint32_t x, uint32_t y, const uint8_t* in);
        void fill          (uint8_t v);

        uint8_t        at(uint32_t x, uint32_t y) const;
        uint32_t       stride() const { return strd; }
        const uint8_t* data() const { return dat.data(); }

      private:
        void setSize(uint32_t w, uint32_t h);

        std::vector<uint8_t> dat;
        uint32_t             w = 0;
        uint32_t             h = 0;
        uint32_t             strd = 0;

      friend class Frame;
      };

    class Audio final {
      public:
        std::vector<float> samples;
      };

    uint32_t width()  const { return planes[0].w; }
    uint32_t height() const { return planes[0].h; }

    const Plane& plane(uint8_t id) const { return planes[id]; }
    const Audio& audio(uint8_t id) const;
    size_t       audioCount()      const { return aud.size(); }

  private:
    Plane              planes[4];
    std::vector<Audio> aud;

    void  setSize(uint32_t w, uint32_t h);
    void  setAudioChannels(uint8_t count);

  friend class Video;
  };

}


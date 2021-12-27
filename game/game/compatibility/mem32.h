#pragma once

#include <vector>
#include <memory>

class Mem32 {
  public:
    Mem32();
    ~Mem32();

    using                     ptr32_t  = uint32_t;
    static constexpr uint32_t memAlign = 8;

    ptr32_t pin  (void* mem, ptr32_t address, uint32_t size, const char* comment = nullptr);
    ptr32_t alloc(uint32_t size);
    void    free (ptr32_t at);

    void    writeInt(ptr32_t address, int32_t v);
    int32_t readInt (ptr32_t address);
    void    copyBytes(ptr32_t src, ptr32_t dst, uint32_t size);

  private:
    enum Status:uint8_t {
      S_Unused,
      S_Allocated,
      S_Pin,
      };

    struct Region {
      Region();
      Region(ptr32_t b, uint32_t sz):address(b),size(sz){}
      ptr32_t     address = 0;
      uint32_t    size    = 0;
      void*       real    = nullptr;
      const char* comment = nullptr;
      Status      status  = S_Unused;
      };

    Region*  translate(ptr32_t address);
    void     compactage();

    std::vector<Region> region;
  };


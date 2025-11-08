#pragma once

#include <vector>
#include <cstdint>
#include <functional>

#include "game/compatibility/mem32instances.h"

class Mem32 {
  private:
    struct Region;

  public:
    Mem32();
    ~Mem32();

    enum class Type : uint32_t {
      plain,
      zCParser,
      ZCParer_variables,
      };

    class Mapping {
      public:
        Mapping() = default;
        Mapping(Mem32& owner, Region& rgn);
        Mapping(Mapping&& other);
        ~Mapping();

        Mapping& operator = (Mapping&& other);

        bool operator == (std::nullptr_t) const { return rgn==nullptr; }
        bool operator != (std::nullptr_t) const { return rgn!=nullptr; }

      private:
        Mem32*  owner = nullptr;
        Region* rgn   = nullptr;

      friend class Mem32;
      };

    using                     ptr32_t  = Compatibility::ptr32_t;
    static constexpr uint32_t memAlign = 8;

    std::function<void(Type,void*,size_t,ptr32_t,std::memory_order)> memoryCallback;

    ptr32_t pin  (void* mem, ptr32_t address, uint32_t size, const char* comment = nullptr);
    ptr32_t pin  (void* mem, uint32_t size, const char* comment = nullptr);
    ptr32_t pin  (ptr32_t address, uint32_t size, Type type);

    ptr32_t alloc(uint32_t size, const char* comment = nullptr);
    ptr32_t alloc(ptr32_t address, uint32_t size, const char* comment = nullptr);
    ptr32_t alloc(uint32_t size, Type type, const char* comment = nullptr);

    void    free (ptr32_t at);

    ptr32_t realloc(ptr32_t address, uint32_t size);

    Mapping map(ptr32_t address);
    void*   deref(ptr32_t address, uint32_t size);
    auto    deref(ptr32_t address) -> std::tuple<void*, uint32_t>;

    template<class T>
    T*      deref(ptr32_t address) { return reinterpret_cast<T*>(deref(address, sizeof(T))); }

    void    writeInt (ptr32_t address, int32_t v);
    int32_t readInt  (ptr32_t address);
    void    copyBytes(ptr32_t src, ptr32_t dst, uint32_t size);

  private:
    enum Status:uint8_t {
      S_Unused,
      S_Allocated,
      S_Pin,
      S_Callback,
      };

    struct Region {
      Region();
      Region(ptr32_t b, uint32_t sz):address(b),size(sz){}

      Type        type    = Type::plain;
      ptr32_t     address = 0;
      uint32_t    size    = 0;
      void*       real    = nullptr;
      const char* comment = nullptr;
      Status      status  = S_Unused;
      };

    Region*  implAlloc(uint32_t size);
    Region*  implAllocAt(ptr32_t address, uint32_t size);
    bool     implRealloc(ptr32_t address, uint32_t size);
    Region*  implTranslate(ptr32_t address);
    Region*  translate(ptr32_t address);
    void     compactage();

    std::vector<Region> region;
  };


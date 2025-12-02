#pragma once

#include <vector>
#include <cstdint>
#include <functional>
#include <type_traits>

#include <Tempest/Log>

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
      zCParser_variables,
      zCPar_Symbol
      };

    using                     ptr32_t  = Compatibility::ptr32_t;
    static constexpr uint32_t memAlign = 8;

    template<class T>
    void    setCallbackR(Type t, const T& fn) { setCallbackR(t, std::function{fn}); }

    template<class T>
    void    setCallbackW(Type t, const T& fn) { setCallbackW(t, std::function{fn}); }

    template<class T>
    void    setCallbackR(Type t, std::function<void(T& t, uint32_t)> fn) {
      implSetCallbackR(t, [f = std::move(fn)](void* ptr, uint32_t id) {
        f(*reinterpret_cast<T*>(ptr), id);
        }, sizeof(T));
      }

    template<class T>
    void    setCallbackW(Type t, std::function<void(T& t, uint32_t)> fn) {
      implSetCallbackW(t, [f = std::move(fn)](void* ptr, uint32_t id) {
        f(*reinterpret_cast<T*>(ptr), id);
        }, sizeof(T));
      }

    ptr32_t pin  (void* mem, ptr32_t address, uint32_t size, const char* comment = nullptr);
    ptr32_t pin  (void* mem, uint32_t size, const char* comment = nullptr);
    ptr32_t pin  (ptr32_t address, uint32_t size, Type type);

    ptr32_t alloc(uint32_t size, const char* comment = nullptr);
    ptr32_t alloc(ptr32_t address, uint32_t size, const char* comment = nullptr);
    ptr32_t alloc(uint32_t size, Type type, const char* comment = nullptr);

    void    free (ptr32_t at);

    ptr32_t realloc(ptr32_t address, uint32_t size);

    auto        deref (ptr32_t address) -> std::tuple<void*, uint32_t>;
    const void* deref (ptr32_t address, uint32_t size);
    void*       derefv(ptr32_t address, uint32_t size);

    template<class T>
    T*          deref(ptr32_t address) {
      if constexpr(std::is_const<T>::value) {
        return reinterpret_cast<const T*>(deref(address, sizeof(T)));
        } else {
        return reinterpret_cast<T*>(derefv(address, sizeof(T)));
        }
      }

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

    struct Callback {
      std::function<void(void*,uint32_t)> read;
      std::function<void(void*,uint32_t)> write;
      ptr32_t                             elementSize = 0;
      };

    void     implSetCallbackR(Type t, std::function<void (void*, uint32_t)> fn, size_t elt);
    void     implSetCallbackW(Type t, std::function<void (void*, uint32_t)> fn, size_t elt);

    Region*  implAlloc(uint32_t size);
    Region*  implAllocAt(ptr32_t address, uint32_t size);
    bool     implRealloc(ptr32_t address, uint32_t size);
    Region*  implTranslate(ptr32_t address);
    Region*  translate(ptr32_t address);
    void     compactage();

    void     memSyncRead(Type       t, const Region& rgn, ptr32_t address, uint32_t size);
    void     memSyncRead(Callback& cb, const Region& rgn, ptr32_t address, uint32_t size);

    void     memSyncWrite(Type       t, const Region& rgn, ptr32_t address, uint32_t size);
    void     memSyncWrite(Callback& cb, const Region& rgn, ptr32_t address, uint32_t size);

    std::vector<Region> region;
    std::unordered_map<Type, Callback> memMap;
  };


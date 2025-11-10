#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <functional>
#include <string>

#include "mem32instances.h"

class Ikarus;
class Mem32;

class Cpu32 {
  public:
    using ptr32_t = Compatibility::ptr32_t;

    Cpu32(Ikarus& ikarus, Mem32& mem32);

    void exec(const ptr32_t basePc, const uint8_t* code, size_t len);

    template<class R, class...Args>
    void register_stdcall(ptr32_t addr, const std::function<R(Args...)>& callback) {
      register_stdcall_inner(addr, [callback](Cpu32& cpu32) {
        cpu32.callExternal<R,Args...>(callback);
        });
      }

    template<class R, class...Args>
    void register_thiscall(ptr32_t addr, const std::function<R(Args...)>& callback) {
      register_stdcall_inner(addr, [callback](Cpu32& cpu32) {
        cpu32.stack.push_back(cpu32.ecx);
        cpu32.callExternal<R,Args...>(callback);
        });
      }

    template <typename T>
    void register_stdcall(ptr32_t addr, const T& cb) {
      register_stdcall(addr, std::function {cb});
      }

    template <typename T>
    void register_thiscall(ptr32_t addr, const T& cb) {
      register_thiscall(addr, std::function {cb});
      }

  private:
    Ikarus&               ikarus;
    Mem32&                mem32;
    std::vector<uint32_t> stack;
    uint32_t              eax = 0;
    uint32_t              ecx = 0;
    uint32_t              edx = 0;
    uint32_t              esp = 0;

    std::unordered_map<ptr32_t, std::function<void(Cpu32&)>> stdcall_Overrides;

    bool     exec1Byte(const ptr32_t basePc, ptr32_t& pc, const uint8_t* code, size_t len);
    bool     exec2Byte(const ptr32_t basePc, ptr32_t& pc, const uint8_t* code, size_t len);
    bool     exec3Byte(const ptr32_t basePc, ptr32_t& pc, const uint8_t* code, size_t len);

    uint8_t  readUInt8 (const uint8_t* code, size_t offset, size_t len);
    uint16_t readUInt16(const uint8_t* code, size_t offset, size_t len);
    uint32_t readUInt24(const uint8_t* code, size_t offset, size_t len);
    uint32_t readUInt32(const uint8_t* code, size_t offset, size_t len);

    void     register_stdcall_inner(ptr32_t addr, std::function<void(Cpu32&)> f);

    void     callFunction(ptr32_t addr);

    template<class R, class...Args>
    void     callExternal(const std::function<R(Args...)>& callback) {
      auto v = std::make_tuple(this->pop<Args>()...);
      if constexpr(std::is_same<void,R>::value) {
        std::apply(callback, std::move(v));
        } else {
        auto r = std::apply(callback, std::move(v));
        this->eax = uint32_t(r);
        }
      }

    template<class T>
    T           pop();
    std::string popString();
  };

template<>
inline int32_t Cpu32::pop<int>() {
  if(stack.size()==0)
    return 0;
  auto ret = stack.back();
  stack.pop_back();
  return int32_t(ret);
  }

template<>
inline Cpu32::ptr32_t Cpu32::pop<Cpu32::ptr32_t>() {
  if(stack.size()==0)
    return 0;
  auto ret = stack.back();
  stack.pop_back();
  return Cpu32::ptr32_t(ret);
  }

template<>
inline std::string Cpu32::pop<std::string>() {
  return popString();
  }

#include "mem32.h"

#include <Tempest/Log>
#include <cassert>
#include <cstring>
#include <algorithm>
#include <cstddef>

using namespace Tempest;

Mem32::Mem32() {
  /*
   *  [0x00001000 .. 0x80000000] - (2GB) user space
   *  [0x80000000 .. 0xc0000000] - (1GB) extra space(reserved for opengothic use; pinned memory)
   *  [0xc0000000 .. 0xffffffff] - (1GB) kernel space
   */
  region.emplace_back(Region(0x1000,0x80000000));
  }

Mem32::~Mem32() {
  for(auto& rgn:region) {
    if(rgn.status==S_Allocated && rgn.real!=nullptr) {
      std::free(rgn.real);
      rgn.real = nullptr;
      }
    }
  }

void Mem32::implSetCallbackR(Type t, std::function<void(void*, uint32_t)> fn, size_t elt) {
  auto& m       = memMap[t];
  m.read        = std::move(fn);
  m.elementSize = uint32_t(elt);
  }

void Mem32::implSetCallbackW(Type t, std::function<void(void*, uint32_t)> fn, size_t elt) {
  auto& m       = memMap[t];
  m.write       = std::move(fn);
  m.elementSize = uint32_t(elt);
  }

Mem32::ptr32_t Mem32::pin(void* mem, ptr32_t address, uint32_t size, const char* comment) {
  if(auto rgn = implAllocAt(address,size)) {
    rgn->size    = size;
    rgn->real    = mem;
    rgn->status  = S_Pin;
    rgn->comment = comment;
    return address;
    }
  throw std::bad_alloc();
  }

Mem32::ptr32_t Mem32::pin(void* mem, uint32_t size, const char* comment) {
  if(auto rgn = implAlloc(size)) {
    rgn->size    = size;
    rgn->real    = mem;
    rgn->status  = S_Pin;
    rgn->comment = comment;
    return rgn->address;
    }
  throw std::bad_alloc();
  }

Mem32::ptr32_t Mem32::pin(ptr32_t address, uint32_t size, Type type) {
  if(auto rgn = implAllocAt(address,size)) {
    rgn->type   = type;
    rgn->size   = size;
    rgn->real   = nullptr; // allocated on demand
    rgn->status = S_Callback;
    return address;
    }
  throw std::bad_alloc();
  }

Mem32::ptr32_t Mem32::alloc(ptr32_t address, uint32_t size, const char* comment) {
  if(auto rgn = implAllocAt(address,size)) {
    rgn->real = std::calloc(rgn->size,1);
    if(rgn->real==nullptr) {
      compactage();
      return 0;
      }
    rgn->size    = size;
    rgn->status  = S_Allocated;
    rgn->comment = comment;
    return address;
    }
  throw std::bad_alloc();
  }

Mem32::ptr32_t Mem32::alloc(uint32_t size, const char* comment) {
  if(auto rgn = implAlloc(size)) {
    rgn->real = std::calloc(rgn->size,1);
    if(rgn->real==nullptr) {
      compactage();
      return 0;
      }
    rgn->status  = S_Allocated;
    rgn->comment = comment;
    return rgn->address;
    }
  return 0;
  }

Mem32::ptr32_t Mem32::alloc(uint32_t size, Type type, const char* comment) {
  if(auto rgn = implAlloc(size)) {
    rgn->type    = type;
    rgn->status  = S_Callback;
    rgn->comment = comment;
    return rgn->address;
    }
  return 0;
  }

void Mem32::free(ptr32_t address) {
  if(address==0)
    return;
  for(size_t i=0; i<region.size(); ++i) {
    if(region[i].address!=address)
      continue;
    std::free(region[i].real);
    region[i].real   = nullptr;
    region[i].status = S_Unused;
    compactage();
    return;
    }
  Log::e("mem_free: heap block wan't allocated by script: ", reinterpret_cast<void*>(uint64_t(address)));
  }

const void* Mem32::deref(ptr32_t address, uint32_t size) {
  auto r = translate(address);
  if(r==nullptr) {
    Log::e("deref: address translation failure: ", reinterpret_cast<void*>(uint64_t(address)));
    return nullptr;
    }
  if(r->address+r->size < address+size) {
    Log::e("deref: memmory block is too small: ", reinterpret_cast<void*>(uint64_t(address)), " ", size);
    return nullptr;
    }
  const auto rgn = *r;
  memSyncRead(rgn.type, rgn, address, size);
  address -= rgn.address;
  return reinterpret_cast<uint8_t*>(rgn.real)+address;
  }

void* Mem32::derefv(ptr32_t address, uint32_t size) {
  auto rgn = translate(address);
  if(rgn==nullptr) {
    Log::e("deref: address translation failure: ", reinterpret_cast<void*>(uint64_t(address)));
    return nullptr;
    }
  if(rgn->address+rgn->size < address+size) {
    Log::e("deref: memmory block is too small: ", reinterpret_cast<void*>(uint64_t(address)), " ", size);
    return nullptr;
    }
  if(rgn->status==S_Callback) {
    Log::e("deref: unable to deref callback-memory: ", reinterpret_cast<void*>(uint64_t(address)));
    return nullptr;
    }
  // memSyncRead(rgn->type, rgn, address, size);
  address -= rgn->address;
  return reinterpret_cast<uint8_t*>(rgn->real)+address;
  }

auto Mem32::deref(ptr32_t address) -> std::tuple<void*, uint32_t> {
  auto r = translate(address);
  if(r==nullptr) {
    Log::e("deref: address translation failure: ", reinterpret_cast<void*>(uint64_t(address)));
    return {nullptr,0};
    }

  const auto     rgn   = *r;
  const ptr32_t  dAddr = address - rgn.address;
  const uint32_t size  = rgn.size - dAddr;
  memSyncRead(rgn.type, rgn, address, size);
  return {reinterpret_cast<uint8_t*>(rgn.real)+dAddr, size};
  }

void Mem32::compactage() {
  for(size_t i=0; i+1<region.size(); ) {
    auto& a = region[i+0];
    auto& b = region[i+1];
    if(a.status==S_Unused && a.status==b.status && a.address+a.size==b.address) {
      a.size += b.size;
      b.size = 0;
      region.erase(region.begin()+ptrdiff_t(i+1));
      } else {
      ++i;
      }
    }
  }

void Mem32::writeInt(ptr32_t address, int32_t v) {
  auto rgn = translate(address);
  if(rgn==nullptr) {
    Log::e("mem_writeint: address translation failure: ", reinterpret_cast<void*>(uint64_t(address)));
    return;
    }

  const ptr32_t dAddr = address-rgn->address;
  auto ptr = reinterpret_cast<uint8_t*>(rgn->real)+dAddr;
  std::memcpy(ptr,&v,4);
  memSyncWrite(rgn->type, *rgn, address, 4);
  }

int32_t Mem32::readInt(ptr32_t address) {
  const auto r = translate(address);
  if(r==nullptr) {
    Log::e("mem_readint:  address translation failure: ", reinterpret_cast<void*>(uint64_t(address)));
    return 0;
    }

  const auto    rgn   = *r;
  const ptr32_t dAddr = address-rgn.address;
  memSyncRead(rgn.type, rgn, address, 4);

  auto ptr = reinterpret_cast<uint8_t*>(rgn.real)+dAddr;
  int32_t ret = 0;
  std::memcpy(&ret,ptr,4);
  return ret;
  }

void Mem32::copyBytes(ptr32_t psrc, ptr32_t pdst, uint32_t size) {
  auto s = translate(psrc);
  auto d = translate(pdst);
  if(s==nullptr || s->status==S_Unused) {
    Log::e("mem_copybytes: address translation failure: ", reinterpret_cast<void*>(uint64_t(psrc)));
    return;
    }
  if(d==nullptr || d->status==S_Unused) {
    Log::e("mem_copybytes: address translation failure: ", reinterpret_cast<void*>(uint64_t(pdst)));
    return;
    }
  const auto    src   = *s;
  const auto    dst   = *d;

  size_t sOff = psrc - src.address;
  size_t dOff = pdst - dst.address;
  size_t sz   = size;
  if(src.size<sOff+size) {
    Log::e("mem_copybytes: copy-size exceed source block size: ", size);
    sz = std::min(src.size-sOff,sz);
    }
  if(dst.size<dOff+size) {
    Log::e("mem_copybytes: copy-size exceed destination block size: ", size);
    sz = std::min(dst.size-dOff,sz);
    }
  memSyncRead(src.type, src, ptr32_t(src.address+sOff), uint32_t(sz));
  std::memcpy(reinterpret_cast<uint8_t*>(dst.real)+sOff,
              reinterpret_cast<uint8_t*>(src.real)+dOff,
              sz);
  memSyncWrite(dst.type, dst, ptr32_t(dst.address+dOff), uint32_t(sz));
  }

Mem32::Region* Mem32::implAlloc(uint32_t size) {
  size = ((size+memAlign-1)/memAlign)*memAlign;

  for(size_t i=0; i<region.size(); ++i) {
    if(region[i].status!=S_Unused)
      continue;
    const uint32_t off = region[i].address % memAlign;
    if(region[i].size<size+off)
      continue;
    if(size!=region[i].size) {
      auto p2 = region[i];
      p2.address+=size;
      p2.size   -=size;
      region.insert(region.begin()+ptrdiff_t(i+1),p2);
      }
    region[i].size = size;
    return &region[i];
    }

  return nullptr;
  }

Mem32::ptr32_t Mem32::realloc(ptr32_t address, uint32_t size) {
  size = ((size+memAlign-1)/memAlign)*memAlign;
  if(implRealloc(address,size))
    return address;

  auto next = implAlloc(size);
  if(next==nullptr)
    return 0;

  auto src = translate(address);
  if(src==nullptr) {
    if(address!=0) {
      Log::e("realloc: address translation failure: ", reinterpret_cast<void*>(uint64_t(address)));
      compactage();
      return 0;
      }
    }

  auto ret = next->address;
  next->status = S_Allocated;

  if(src!=nullptr) {
    next->type    = src->type;
    next->real    = std::realloc(src->real, next->size);
    next->comment = src->comment;

    src->real     = nullptr;
    src->status   = S_Unused;
    } else {
    next->real    = std::calloc(next->size, 1);
    }

  compactage();
  return ret;
  }

Mem32::Region* Mem32::implAllocAt(ptr32_t address, uint32_t size) {
  for(size_t i=0; i<region.size(); ++i) {
    auto& rgn = region[i];

    if(address!=0) {
      if(!(rgn.address<=address && address+size<=rgn.address+rgn.size))
        continue;
      } else {
      if(rgn.size<size)
        continue;
      }

    if(rgn.status!=S_Unused) {
      Log::e("DMA: failed to pin a ",size," bytes of memory: block is in use");
      return 0;
      }

    if(address==0)
      address = region[i].address;

    if(region[i].address<address) {
      uint32_t off = (address-region[i].address);
      auto p2 = region[i];
      p2.address+=off;
      p2.size   -=off;
      region.insert(region.begin()+ptrdiff_t(i+1),p2);
      region[i].size = off;
      ++i;
      }
    if(size!=region[i].size) {
      auto p2 = region[i];
      p2.address+=size;
      p2.size   -=size;
      region.insert(region.begin()+ptrdiff_t(i+1),p2);
      }

    return &region[i];
    }
  return nullptr;
  }

bool Mem32::implRealloc(ptr32_t address, uint32_t nsize) {
  // NOTE: in place only
  for(size_t i=0; i<region.size(); ++i) {
    auto& rgn = region[i];
    if(rgn.address!=address)
      continue; //TODO: binary-search

    if(nsize==rgn.size)
      return true;

    if(nsize<rgn.size) {
      if(auto next = std::realloc(rgn.real, nsize))
        rgn.real = next;
      Region frgn(address+nsize, rgn.size-nsize);
      rgn.size = nsize;
      region.insert(region.begin() + intptr_t(i + 1), frgn);
      return true;
      }

    if(i+1==region.size())
      return false; // can't expand

    auto& rgn2 = region[i+1];
    if(rgn2.status==S_Unused && rgn.size + rgn2.size>=nsize) {
      auto next = std::realloc(rgn.real, nsize);
      if(next==nullptr)
        return false;
      rgn2.address += (nsize-rgn.size);
      rgn2.size    -= (nsize-rgn.size);
      rgn.real      = next;
      rgn.size      = nsize;
      if(rgn2.size==0)
        compactage();
      return true;
      }

    return false;
    }
  return false;
  }

Mem32::Region* Mem32::implTranslate(ptr32_t address) {
  // TODO: binary-search
  for(auto& rgn:region) {
    if(rgn.address<=address && address<rgn.address+rgn.size && rgn.status!=S_Unused)
      return &rgn;
    }
  return nullptr;
  }

Mem32::Region* Mem32::translate(ptr32_t address) {
  auto ret = implTranslate(address);
  if(ret==nullptr)
    return nullptr;

  auto& rgn = *ret;
  if(rgn.status!=S_Callback)
    return ret;

  if(rgn.real==nullptr) {
    rgn.real = std::calloc(rgn.size, 1);
    if(rgn.real==nullptr)
      throw std::bad_alloc();
    // memSyncRead(rgn.type, rgn, rgn.address, rgn.size);
    // return implTranslate(address); // sync may call allocatemore blocks internally
    }
  return ret;
  }

void Mem32::memSyncRead(Type type, const Region& rgn, ptr32_t address, uint32_t size) {
  if(type==Type::plain)
    return;
  auto m = memMap.find(type);
  if(m==memMap.end()) {
    throw std::logic_error("DMA: callback is not provided for callback memory!");
    }
  memSyncRead(m->second, rgn, address, size);
  }

void Mem32::memSyncRead(Callback& cb, const Region& rgn, ptr32_t address, uint32_t size) {
  address -= rgn.address;

  const uint32_t begin = address/cb.elementSize;
  const uint32_t end   = (address+size+cb.elementSize-1)/cb.elementSize;

  auto ptr = reinterpret_cast<uint8_t*>(rgn.real);
  for(size_t i=begin; i<end; ++i) {
    cb.read(ptr+i*cb.elementSize, uint32_t(i));
    }
  }

void Mem32::memSyncWrite(Type type, const Region& rgn, ptr32_t address, uint32_t size) {
  if(type==Type::plain)
    return;
  auto m = memMap.find(type);
  if(m==memMap.end()) {
    Log::e("memSyncWrite: unable to write to mapped memory: ", reinterpret_cast<void*>(uint64_t(address)));
    return;
    }
  memSyncWrite(m->second, rgn, address, size);
  }

void Mem32::memSyncWrite(Callback& cb, const Region& rgn, ptr32_t address, uint32_t size) {
  if(!cb.write) {
    Log::e("memSyncWrite: unable to write to mapped memory: ", reinterpret_cast<void*>(uint64_t(address)));
    return;
    }

  address -= rgn.address;
  const uint32_t begin = address/cb.elementSize;
  const uint32_t end   = (address+size+cb.elementSize-1)/cb.elementSize;

  auto ptr = reinterpret_cast<uint8_t*>(rgn.real);
  for(size_t i=begin; i<end; ++i) {
    cb.write(ptr+i*cb.elementSize, uint32_t(i));
    }
  }

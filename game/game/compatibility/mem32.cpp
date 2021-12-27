#include "mem32.h"

#include <Tempest/Log>
#include <cstring>
#include <algorithm>

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

Mem32::ptr32_t Mem32::pin(void* mem, ptr32_t address, uint32_t size, const char* comment) {
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
      Log::e("failed to pin a ",size," bytes of memory: block is in use");
      return 0;
      }

    if(address==0)
      address = region[i].address;

    if(region[i].address<address) {
      uint32_t off = (address-region[i].address);
      auto p2 = region[i];
      p2.address+=off;
      p2.size   -=off;
      region.insert(region.begin()+i+1,p2);
      region[i].size = off;
      ++i;
      }
    if(size!=region[i].size) {
      auto p2 = region[i];
      p2.address+=size;
      p2.size   -=size;
      region.insert(region.begin()+i+1,p2);
      }

    region[i].size    = size;
    region[i].real    = mem;
    region[i].status  = S_Pin;
    region[i].comment = comment;
    }
  return 0;
  }

Mem32::ptr32_t Mem32::alloc(uint32_t size) {
  size = ((size+memAlign-1)/memAlign)*memAlign;

  for(size_t i=0; i<region.size(); ++i) {
    if(region[i].status!=S_Unused || region[i].size<size)
      continue;
    if(size!=region[i].size) {
      auto p2 = region[i];
      p2.address+=size;
      p2.size   -=size;
      region.insert(region.begin()+i+1,p2);
      }
    region[i].size   = size;
    region[i].real   = std::calloc(size,1);
    if(region[i].real!=nullptr) {
      region[i].status = S_Allocated;
      return region[i].address;
      } else {
      compactage();
      return 0;
      }
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

void Mem32::compactage() {
  for(size_t i=0; i+1<region.size(); ) {
    auto& a = region[i+0];
    auto& b = region[i+1];
    if(a.status==S_Unused && a.status==b.status && a.address+a.size==b.address) {
      a.size += b.size;
      b.size = 0;
      region.erase(region.begin()+i+1);
      } else {
      ++i;
      }
    }
  }

void Mem32::writeInt(ptr32_t address, int32_t v) {
  auto rgn = translate(address);
  if(rgn==nullptr || rgn->status==S_Unused) {
    Log::e("mem_writeint: address translation failure: ", reinterpret_cast<void*>(uint64_t(address)));
    return;
    }
  address -= rgn->address;
  auto ptr = reinterpret_cast<uint8_t*>(rgn->real)+address;
  std::memcpy(ptr,&v,4);
  }

int32_t Mem32::readInt(ptr32_t address) {
  auto rgn = translate(address);
  if(rgn==nullptr || rgn->status==S_Unused) {
    Log::e("mem_readint: address translation failure: ", reinterpret_cast<void*>(uint64_t(address)));
    return 0;
    }
  address -= rgn->address;
  auto ptr = reinterpret_cast<uint8_t*>(rgn->real)+address;
  int32_t ret = 0;
  std::memcpy(&ret,ptr,4);
  return ret;
  }

void Mem32::copyBytes(ptr32_t psrc, ptr32_t pdst, uint32_t size) {
  auto src = translate(psrc);
  auto dst = translate(pdst);
  if(src==nullptr || src->status==S_Unused) {
    Log::e("mem_copybytes: address translation failure: ", reinterpret_cast<void*>(uint64_t(psrc)));
    return;
    }
  if(dst==nullptr || dst->status==S_Unused) {
    Log::e("mem_copybytes: address translation failure: ", reinterpret_cast<void*>(uint64_t(pdst)));
    return;
    }

  size_t sOff = psrc - src->address;
  size_t dOff = pdst - dst->address;
  size_t sz   = size;
  if(src->size<sOff+size) {
    Log::e("mem_copybytes: copy-size exceed source block size: ", size);
    sz = std::min(src->size-sOff,sz);
    }
  if(dst->size<dOff+size) {
    Log::e("mem_copybytes: copy-size exceed destination block size: ", size);
    sz = std::min(dst->size-dOff,sz);
    }
  std::memcpy(reinterpret_cast<uint8_t*>(dst->real)+sOff,
              reinterpret_cast<uint8_t*>(src->real)+dOff,
              sz);
  }

Mem32::Region* Mem32::translate(ptr32_t address) {
  for(auto& rgn:region) {
    if(rgn.address<=address && address<rgn.address+rgn.size)
      return &rgn;
    }
  return nullptr;
  }

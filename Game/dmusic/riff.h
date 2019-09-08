#pragma once

#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <stdexcept>

namespace Dx8 {

class Riff final {
  public:
    using Callback = void(*)(void* ctx,Riff& chunk);

    Riff(const uint8_t* data,size_t sz);

    const char* id() const { return head.id; }
    bool is(const char* idx) const { return head.is(idx); }
    bool hasData() const { return at<sz; }

    size_t remaning() const { return sz-at; }

    void readListId();
    void readListId(const char* id);
    bool isListId(const char* id);

    void read(std::u16string& str);
    void read(std::string& str);
    void read(std::vector<uint8_t>& vec);
    void read(void* dest,size_t sz);
    void skip(size_t sz);
    void read(void* ctx, Callback cb);
    template<class F>
    void read(F f){
      Callback cb = [](void* pf,Riff& chunk){
        F& f = *reinterpret_cast<F*>(pf);
        f(chunk);
        };
      read(&f,cb);
      }

    template<class T>
    void readAll(std::vector<T>& all){
      uint32_t sz=0;
      read(&sz,4);
      if(sz>sizeof(T))
        throw std::runtime_error("inconsistent struct size");
      size_t cnt = remaning()/sz;
      all.resize(cnt);
      for(size_t i=0;i<cnt;++i) {
        read(&all[i],sz);
        }
      }

  private:
    struct ChunkHeader final {
      char          id[5]={};
      std::uint32_t size =0;
      bool is(const char* idx) const { return std::memcmp(id,idx,4)==0; }
      };

    void readHdr(ChunkHeader& h);

    [[noreturn]]
    void onError(const char* msg);

    const uint8_t* data;
    size_t         sz=0;
    size_t         at=0;

    ChunkHeader    head;
    char           listId[4]={};
  };

}

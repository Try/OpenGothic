#pragma once

#include <Tempest/IDevice>
#include <functional>
#include <cstring>

namespace Dx8 {

class RiffFile : public Tempest::IDevice {
  public:
    uint8_t peek() override { return dev.peek(); }
    size_t  read(void* to,size_t sz) override;
    size_t  size() const override;
    size_t  seek(size_t advance) override;

    std::u16string readStr();

    template<class Fn,class T>
    static void readChunk(Tempest::IDevice &dev,T& t, Fn callback) {
      RiffFile file(dev);
      callback(file,t);
      file.seek(file.head.size);
      }

    const char* id() const { return head.id; }
    bool is(const char* ch4) const { return std::memcmp(head.id,ch4,4)==0; }

    bool hasData() const { return head.size>0; }

  private:
    RiffFile(Tempest::IDevice& dev);

    struct ChunkHeader final {
      char          id[4]={};
      std::uint32_t size =0;
      bool          is(const char* idx) const { return std::memcmp(id,idx,4)==0; }
      };

    Tempest::IDevice& dev;
    ChunkHeader       head;
  };

}

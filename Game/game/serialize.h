#pragma once

#include <Tempest/Pixmap>
#include <Tempest/Matrix4x4>
#include <Tempest/IDevice>
#include <Tempest/ODevice>

#include <stdexcept>
#include <vector>
#include <array>

#include <daedalus/ZString.h>

#include "gametime.h"
#include "constants.h"

class WayPoint;
class Npc;
class World;
class FpLock;
class SaveGameHeader;

class Serialize final {
  public:
    enum {
      MinVersion = 0,
      Version    = 5
      };

    Serialize(Tempest::ODevice& fout);
    Serialize(Tempest::IDevice&  fin);
    Serialize(Serialize&&)=default;

    static Serialize empty();

    uint16_t version() const { return ver; }
    void setContext(World* ctx) { this->ctx=ctx; }

    template<class T>
    T read(){ T t; read(t); return t; }

    template<class T,class ... Arg>
    void read(T& t,Arg& ... a){
      read(t);
      read(a...);
      }

    template<class T,class ... Arg>
    void write(const T& t,const Arg& ... a){
      write(t);
      write(a...);
      }

    void write(WalkBit   i) { writeBytes(&i,sizeof(i)); }
    void read (WalkBit&  i) { readBytes (&i,sizeof(i)); }

    void write(BodyState i) { writeBytes(&i,sizeof(i)); }
    void read (BodyState&i) { readBytes (&i,sizeof(i)); }

    void write(WeaponState  w);
    void read (WeaponState &w);

    void write(bool      i) { write(uint8_t(i ? 1 : 0)); }
    void read (bool&     i) { uint8_t x=0; read(x); i=(x!=0); }

    void write(char      i) { writeBytes(&i,sizeof(i)); }
    void read (char&     i) { readBytes (&i,sizeof(i)); }

    void write(uint8_t   i) { writeBytes(&i,sizeof(i)); }
    void read (uint8_t&  i) { readBytes (&i,sizeof(i)); }

    void write(uint16_t  i) { writeBytes(&i,sizeof(i)); }
    void read (uint16_t& i) { readBytes (&i,sizeof(i)); }

    void write(int32_t   i) { writeBytes(&i,sizeof(i)); }
    void read (int32_t&  i) { readBytes (&i,sizeof(i)); }

    void write(uint32_t  i) { writeBytes(&i,sizeof(i)); }
    void read (uint32_t& i) { readBytes (&i,sizeof(i)); }

    void write(uint64_t  i) { writeBytes(&i,sizeof(i)); }
    void read (uint64_t& i) { readBytes (&i,sizeof(i)); }

    void write(gtime  i)    { writeBytes(&i,sizeof(i)); }
    void read (gtime& i)    { readBytes (&i,sizeof(i)); }

    void write(float  i)    { writeBytes(&i,sizeof(i)); }
    void read (float& i)    { readBytes (&i,sizeof(i)); }

    void write(const Tempest::Matrix4x4& i) { writeBytes(&i,sizeof(i)); }
    void read (Tempest::Matrix4x4& i)       { readBytes (&i,sizeof(i)); }

    void write(const std::string&              s);
    void read (std::string&                    s);

    void write(const Daedalus::ZString&        s);
    void read (Daedalus::ZString&              s);

    void write(const SaveGameHeader&           p);
    void read (SaveGameHeader&                 p);

    void write(const Tempest::Pixmap&          p);
    void read (Tempest::Pixmap&                p);

    void write(const WayPoint*  wptr);
    void read (const WayPoint*& wptr);

    void write(const Npc*  npc);
    void read (const Npc*& npc);

    void write(Npc*        npc);
    void read (Npc*&       npc);

    void write(const FpLock& fp);
    void read (FpLock& fp);

    template<class T>
    void write(const std::vector<T>& s) {
      uint32_t sz=uint32_t(s.size());
      write(sz);
      for(auto& i:s)
        write(i);
      }

    template<class T>
    void read (std::vector<T>& s){
      uint32_t sz=0;
      read(sz);
      s.resize(sz);
      for(auto& i:s)
        read(i);
      }

    template<size_t sz>
    void write(const Daedalus::ZString (&s)[sz]) { writeArr(s); }

    template<size_t sz>
    void read (Daedalus::ZString (&s)[sz]) { readArr(s); }

    template<size_t sz>
    void write(const std::string (&s)[sz]) { writeArr(s); }

    template<size_t sz>
    void read (std::string (&s)[sz]) { readArr(s); }

    template<size_t sz>
    void write(const int32_t (&s)[sz]) { writeArr(s); }

    template<size_t sz>
    void read (int32_t (&s)[sz]) { readArr(s); }

    template<size_t sz>
    void write(const uint32_t (&s)[sz]) { writeArr(s); }

    template<size_t sz>
    void read (uint32_t (&s)[sz]) { readArr(s); }

    template<size_t sz>
    void write(const float (&s)[sz]) { writeArr(s); }

    template<size_t sz>
    void read (float (&s)[sz]) { readArr(s); }


    template<size_t sz>
    void write(const std::array<float,sz>& v) {
      writeBytes(&v[0],sz);
      }

    template<size_t sz>
    void read (std::array<float,sz>& v) {
      readBytes(&v[0],sz);
      }

  private:
    Serialize();

    void readBytes(void* v,size_t sz){
      if(in->read(v,sz)!=sz)
        throw std::runtime_error("unable to read save-game file");
      }

    void writeBytes(const void* v,size_t sz){
      if(out->write(v,sz)!=sz)
        throw std::runtime_error("unable to write save-game file");
      }

    template<class T,size_t sz>
    void writeArr(const T (&s)[sz]) {
      for(size_t i=0;i<sz;++i) write(s[i]);
      }

    template<class T,size_t sz>
    void readArr (T (&s)[sz])  {
      for(size_t i=0;i<sz;++i) read(s[i]);
      }

    static const char tag[];
    Tempest::ODevice* out=nullptr;
    Tempest::IDevice* in =nullptr;
    uint16_t          ver=Version;
    World*            ctx=nullptr;
    std::string       tmpStr;
  };

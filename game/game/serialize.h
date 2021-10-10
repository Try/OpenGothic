#pragma once

#include <Tempest/Pixmap>
#include <Tempest/Matrix4x4>
#include <Tempest/IDevice>
#include <Tempest/ODevice>
#include <Tempest/Point>

#include <stdexcept>
#include <vector>
#include <array>
#include <type_traits>

#include <daedalus/DATFile.h>
#include <daedalus/ZString.h>

#include "gametime.h"
#include "constants.h"

class WayPoint;
class Npc;
class Interactive;
class World;
class FpLock;
class ScriptFn;
class SaveGameHeader;

namespace Daedalus {
namespace GEngineClasses {
struct C_Npc;
}
}

class Serialize final {
  public:
    enum {
      MinVersion = 0,
      Version    = 35
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
      implRead(t);
      read(a...);
      }

    template<class T>
    void read(T& t){
      implRead(t);
      }

    template<class T,class ... Arg>
    void write(const T& t,const Arg& ... a){
      implWrite(t);
      write(a...);
      }

    template<class T>
    void write(const T& t){
      implWrite(t);
      }

  private:
    void implWrite(WalkBit   i) { writeBytes(&i,sizeof(i)); }
    void implRead (WalkBit&  i) { readBytes (&i,sizeof(i)); }

    void implWrite(BodyState i) { writeBytes(&i,sizeof(i)); }
    void implRead (BodyState&i) { readBytes (&i,sizeof(i)); }

    void implWrite(WeaponState  w);
    void implRead (WeaponState &w);

    void implWrite(bool      i) { implWrite(uint8_t(i ? 1 : 0)); }
    void implRead (bool&     i) { uint8_t x=0; read(x); i=(x!=0); }

    void implWrite(char      i) { writeBytes(&i,sizeof(i)); }
    void implRead (char&     i) { readBytes (&i,sizeof(i)); }

    void implWrite(uint8_t   i) { writeBytes(&i,sizeof(i)); }
    void implRead (uint8_t&  i) { readBytes (&i,sizeof(i)); }

    void implWrite(uint16_t  i) { writeBytes(&i,sizeof(i)); }
    void implRead (uint16_t& i) { readBytes (&i,sizeof(i)); }

    void implWrite(int32_t   i) { writeBytes(&i,sizeof(i)); }
    void implRead (int32_t&  i) { readBytes (&i,sizeof(i)); }

    void implWrite(uint32_t  i) { writeBytes(&i,sizeof(i)); }
    void implRead (uint32_t& i) { readBytes (&i,sizeof(i)); }

    void implWrite(uint64_t  i) { writeBytes(&i,sizeof(i)); }
    void implRead (uint64_t& i) { readBytes (&i,sizeof(i)); }

    void implWrite(gtime  i)    { writeBytes(&i,sizeof(i)); }
    void implRead (gtime& i)    { readBytes (&i,sizeof(i)); }

    void implWrite(float  i)    { writeBytes(&i,sizeof(i)); }
    void implRead (float& i)    { readBytes (&i,sizeof(i)); }

    template<class T, std::enable_if_t<!std::is_same<T,uint32_t>::value && !std::is_same<T,uint64_t>::value && std::is_same<T,size_t>::value,bool> = true>
    void implWrite(T ) = delete;
    template<class T, std::enable_if_t<!std::is_same<T,uint32_t>::value && !std::is_same<T,uint64_t>::value && std::is_same<T,size_t>::value,bool> = true>
    void implRead(T&) = delete;

    void implWrite(const Tempest::Matrix4x4& i) { writeBytes(&i,sizeof(i)); }
    void implRead (Tempest::Matrix4x4& i)       { readBytes (&i,sizeof(i)); }

    void implWrite(const std::string&              s);
    void implRead (std::string&                    s);

    void implWrite(std::string_view                s);

    void implWrite(const Daedalus::ZString&        s);
    void implRead (Daedalus::ZString&              s);

    void implWrite(const SaveGameHeader&           p);
    void implRead (SaveGameHeader&                 p);

    void implWrite(const Tempest::Pixmap&          p);
    void implRead (Tempest::Pixmap&                p);

    void implWrite(const WayPoint*  wptr);
    void implRead (const WayPoint*& wptr);

    void implWrite(const ScriptFn& fn);
    void implRead (ScriptFn&       fn);

    void implWrite(const Npc*  npc);
    void implRead (const Npc*& npc);

    void implWrite(Npc*        npc);
    void implRead (Npc*&       npc);

    void implWrite(Interactive*  mobsi);
    void implRead (Interactive*& mobsi);

    void implWrite(const FpLock& fp);
    void implRead (FpLock& fp);

    template<class T>
    void implWrite(const std::vector<T>& s) {
      implWriteVec(s,std::is_trivial<T>());
      }

    template<class T>
    void implRead (std::vector<T>& s){
      implReadVec(s,std::is_trivial<T>());
      }

    void implWrite(const std::vector<bool>& s) {
      uint32_t sz=uint32_t(s.size());
      write(sz);
      for(size_t i=0; i<s.size(); ++i)
        write(s[i]);
      }

    void implRead (std::vector<bool>& s) {
      uint32_t sz=0;
      read(sz);
      s.resize(sz);
      for(size_t i=0; i<s.size(); ++i) {
        bool b=false;
        read(b);
        s[i] = b;
        }
      }

    template<size_t sz>
    void implWrite(const Daedalus::ZString (&s)[sz]) { writeArr(s); }

    template<size_t sz>
    void implRead (Daedalus::ZString (&s)[sz]) { readArr(s); }

    template<size_t sz>
    void implWrite(const std::string (&s)[sz]) { writeArr(s); }

    template<size_t sz>
    void implRead (std::string (&s)[sz]) { readArr(s); }

    template<size_t sz>
    void implWrite(const uint8_t (&s)[sz]) { writeBytes(s,sz); }

    template<size_t sz>
    void implRead (uint8_t (&s)[sz]) { readBytes(s,sz); }

    template<size_t sz>
    void implWrite(const int32_t (&s)[sz]) { writeArr(s); }

    template<size_t sz>
    void implRead (int32_t (&s)[sz]) { readArr(s); }

    template<size_t sz>
    void implWrite(const uint32_t (&s)[sz]) { writeArr(s); }

    template<size_t sz>
    void implRead (uint32_t (&s)[sz]) { readArr(s); }

    template<size_t sz>
    void implWrite(const float (&s)[sz]) { writeArr(s); }

    template<size_t sz>
    void implRead (float (&s)[sz]) { readArr(s); }

    void implWrite(const Tempest::Vec3& s) { writeBytes(&s,sizeof(s)); }
    void implRead (Tempest::Vec3& s)       { readBytes (&s,sizeof(s)); }

    template<size_t sz>
    void implWrite(const std::array<float,sz>& v) {
      writeBytes(&v[0],sz*sizeof(float));
      }

    template<size_t sz>
    void implRead (std::array<float,sz>& v) {
      readBytes(&v[0],sz*sizeof(float));
      }

    void implWrite(const Daedalus::GEngineClasses::C_Npc& h);
    void implRead (Daedalus::GEngineClasses::C_Npc&       h);

    void implWrite(const Daedalus::DataContainer<int>&               c) { implWriteDat<int>  (c); }
    void implRead (Daedalus::DataContainer<int>&                     c) { implReadDat <int>  (c); }
    void implWrite(const Daedalus::DataContainer<float>&             c) { implWriteDat<float>(c); }
    void implRead (Daedalus::DataContainer<float>&                   c) { implReadDat <float>(c); }
    void implWrite(const Daedalus::DataContainer<Daedalus::ZString>& c) { implWriteDat<Daedalus::ZString>(c); }
    void implRead (Daedalus::DataContainer<Daedalus::ZString>&       c) { implReadDat <Daedalus::ZString>(c); }

    Serialize();

    template<class T>
    void implWriteDat(const Daedalus::DataContainer<T>& s) {
      uint32_t sz=uint32_t(s.size());
      write(sz);
      for(size_t i=0; i<sz; ++i)
        write(s[i]);
      }

    template<class T>
    void implReadDat(Daedalus::DataContainer<T>& s) {
      uint32_t sz=0;
      read(sz);
      s.resize(sz);
      for(size_t i=0; i<sz; ++i)
        read(s[i]);
      }

    template<class T>
    void implWriteVec(const std::vector<T>& s,std::false_type) {
      uint32_t sz=uint32_t(s.size());
      write(sz);
      for(auto& i:s)
        write(i);
      }

    template<class T>
    void implWriteVec(const std::vector<T>& s,std::true_type) {
      uint32_t sz=uint32_t(s.size());
      write(sz);
      writeBytes(s.data(),sz*sizeof(T));
      }

    template<class T>
    void implReadVec(std::vector<T>& s,std::false_type) {
      uint32_t sz=0;
      read(sz);
      s.resize(sz);
      for(auto& i:s)
        read(i);
      }

    template<class T>
    void implReadVec(std::vector<T>& s,std::true_type) {
      uint32_t sz=0;
      read(sz);
      s.resize(sz);
      readBytes(s.data(),sz*sizeof(T));
      }

    void readBytes(void* v,size_t sz) {
      if(in->read(v,sz)!=sz)
        throw std::runtime_error("unable to read save-game file");
      }

    void writeBytes(const void* v,size_t sz) {
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

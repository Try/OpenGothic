#pragma once

#include <Tempest/IDevice>
#include <Tempest/ODevice>
#include <Tempest/Pixmap>

#include <Tempest/Matrix4x4>

#include <vector>
#include <cstdint>
#include <array>
#include <type_traits>
#include <sstream>
#include <ctime>

#include <daedalus/DATFile.h>
#include <daedalus/ZString.h>

#include <miniz.h>

#include <zenload/zTypes.h>

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

class Serialize {
  public:
    enum Version : uint16_t {
      Current = 38
      };
    Serialize(Tempest::ODevice& fout);
    Serialize(Tempest::IDevice&  fin);
    Serialize(Serialize&&)=default;
    ~Serialize();

    uint16_t version()              const { return wldVer; }
    void     setVersion(uint16_t v)       { wldVer = v;    }
    uint16_t globalVersion()        const { return curVer; }
    void     setGlobalVersion(uint16_t v) { curVer = v;    }

    template<class ... Args>
    bool setEntry(const Args& ... args) {
      std::stringstream s;
      int dummy[] = {(s << args, 0)...};
      (void)dummy;
      return implSetEntry(s.str());
      }

    template<class ... Args>
    uint32_t directorySize(const Args& ... args) {
      std::stringstream s;
      int dummy[] = {(s << args, 0)...};
      (void)dummy;
      return implDirectorySize(s.str());
      }

    void setContext(World* ctx) { this->ctx=ctx; }
    std::string_view worldName() const;

    // raw
    void writeBytes(const void* v,size_t sz);
    void readBytes (void* v,size_t sz);

    template<class ... Arg>
    void write(const Arg& ... a){
      int dummy[sizeof...(Arg)] = { (implWrite(a), 0)... };
      (void)dummy;
      }

    template<class ... Arg>
    void read(Arg& ... a){
      int dummy[sizeof...(Arg)] = { (implRead(a), 0)... };
      (void)dummy;
      }

  private:
    Serialize();

    // trivial types
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

    void implWrite(float  i)    { writeBytes(&i,sizeof(i)); }
    void implRead (float& i)    { readBytes (&i,sizeof(i)); }

    template<class T, std::enable_if_t<!std::is_same<T,uint32_t>::value && !std::is_same<T,uint64_t>::value && std::is_same<T,size_t>::value,bool> = true>
    void implWrite(T ) = delete;
    template<class T, std::enable_if_t<!std::is_same<T,uint32_t>::value && !std::is_same<T,uint64_t>::value && std::is_same<T,size_t>::value,bool> = true>
    void implRead(T&) = delete;

    // typedef
    void implWrite(gtime  i)    { writeBytes(&i,sizeof(i)); }
    void implRead (gtime& i)    { readBytes (&i,sizeof(i)); }

    void implWrite(WalkBit   i) { writeBytes(&i,sizeof(i)); }
    void implRead (WalkBit&  i) { readBytes (&i,sizeof(i)); }

    void implWrite(BodyState i) { writeBytes(&i,sizeof(i)); }
    void implRead (BodyState&i) { readBytes (&i,sizeof(i)); }

    void implWrite(Attitude  i) { writeBytes(&i,sizeof(i));}
    void implRead (Attitude& i) { readBytes (&i,sizeof(i)); }

    void implWrite(WeaponState  w);
    void implRead (WeaponState &w);

    // composite types
    void implWrite(const std::tm& i)       { writeBytes(&i,sizeof(i)); }
    void implRead (std::tm&       i)       { readBytes (&i,sizeof(i)); }

    void implWrite(const Tempest::Vec3& s) { writeBytes(&s,sizeof(s)); }
    void implRead (Tempest::Vec3& s)       { readBytes (&s,sizeof(s)); }

    void implWrite(const Tempest::Matrix4x4& i) { writeBytes(&i,sizeof(i)); }
    void implRead (Tempest::Matrix4x4& i)       { readBytes (&i,sizeof(i)); }

    void implWrite(const ZenLoad::zCModelAniSample& i) { writeBytes(&i,sizeof(i)); }
    void implRead (ZenLoad::zCModelAniSample& i)       { readBytes (&i,sizeof(i)); }

    // strings
    void implWrite(const std::string&              s);
    void implRead (std::string&                    s);

    void implWrite(std::string_view                s);

    void implWrite(const Daedalus::ZString&        s);
    void implRead (Daedalus::ZString&              s);

    // vectors
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

    // c-arrays
    template<class T, size_t sz>
    void implWrite(const T (&s)[sz]) {
      implWriteArr(s,std::is_trivial<T>());
      }

    template<class T, size_t sz>
    void implWriteArr(const T (&s)[sz],std::false_type) {
      for(size_t i=0; i<sz; ++i)
        write(s[i]);
      }

    template<class T, size_t sz>
    void implWriteArr(const T (&s)[sz],std::true_type) {
      writeBytes(s,sz*sizeof(T));
      }

    template<class T, size_t sz>
    void implRead(T (&s)[sz]) {
      implReadArr(s,std::is_trivial<T>());
      }

    template<class T, size_t sz>
    void implReadArr(T (&s)[sz],std::false_type) {
      for(size_t i=0; i<sz; ++i)
        read(s[i]);
      }

    template<class T, size_t sz>
    void implReadArr(T (&s)[sz],std::true_type) {
      readBytes(s,sz*sizeof(T));
      }

    // classes
    void implWrite(const Daedalus::DataContainer<int>&               c) { implWriteDat<int>  (c); }
    void implRead (Daedalus::DataContainer<int>&                     c) { implReadDat <int>  (c); }
    void implWrite(const Daedalus::DataContainer<float>&             c) { implWriteDat<float>(c); }
    void implRead (Daedalus::DataContainer<float>&                   c) { implReadDat <float>(c); }
    void implWrite(const Daedalus::DataContainer<Daedalus::ZString>& c) { implWriteDat<Daedalus::ZString>(c); }
    void implRead (Daedalus::DataContainer<Daedalus::ZString>&       c) { implReadDat <Daedalus::ZString>(c); }
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

    void implWrite(const SaveGameHeader& p);
    void implRead (SaveGameHeader&       p);

    void implWrite(const Tempest::Pixmap& p);
    void implRead (Tempest::Pixmap&       p);

    void implWrite(const Daedalus::GEngineClasses::C_Npc& h);
    void implRead (Daedalus::GEngineClasses::C_Npc&       h);

    void implWrite(const FpLock& fp);
    void implRead (FpLock& fp);

    // pointers
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

    static size_t writeFunc(void *pOpaque, uint64_t file_ofs, const void *pBuf, size_t n);
    static size_t readFunc (void *pOpaque, uint64_t file_ofs, void *pBuf, size_t n);

    void   closeEntry();
    bool   implSetEntry(std::string e);
    uint32_t implDirectorySize(std::string e);

    uint16_t                 curVer = Version::Current;
    uint16_t                 wldVer = Version::Current;

    std::string              tmpStr;
    World*                   ctx       = nullptr;

    mz_zip_archive           impl      = {};
    std::string              entryName;
    std::vector<uint8_t>     entryBuf;
    uint64_t                 curOffset = 0;
    uint64_t                 readOffset = 0;
    Tempest::ODevice*        fout      = nullptr;
    Tempest::IDevice*        fin       = nullptr;
  };


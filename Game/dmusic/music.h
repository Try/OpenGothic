#pragma once

#include <Tempest/Sound>
#include <unordered_map>

#include "segment.h"
#include "style.h"

namespace Dx8 {

class DirectMusic;
class DlsCollection;
class Wave;

class Music final {
  public:
    Music()=default;
    Music(Music&&)=default;

    Music& operator = (Music&&)=default;

    void exec(const size_t patternId) const;

  private:
    Music(const Segment& s,DirectMusic& owner);

    struct Instrument final {
      const DlsCollection* dls=nullptr;
      float                volume=0.f;
      float                pan=0.f;
      };

    struct Record final {
      uint64_t              at      =0;
      uint64_t              duration=0;
      const Tempest::Sound* wave    =nullptr;
      float                 volume  =0.f;
      float                 pitch   =1.f;
      float                 pan     =0.f;
      };

    struct Internal {
      uint64_t            timeTotal=0;
      std::vector<Record> idxWaves;
      };

    void index();
    void index(const Pattern::PartRef& pref, const Style::Part &part, uint64_t timeOffset);

    void exec(const Style &stl, const Pattern::PartRef &pref, const Style::Part &part) const;

    DirectMusic*                  owner=nullptr;

    uint32_t                      cordHeader=0;
    std::vector<DMUS_IO_SUBCHORD> subchord;
    const Style*                  style=nullptr;

    std::unordered_map<uint32_t,Instrument> instruments;
    std::shared_ptr<Internal>     intern;


    friend class DirectMusic;
    friend class Mixer;
  };

}

#pragma once

#include <Tempest/Sound>
#include <unordered_map>
#include <atomic>

#include "segment.h"
#include "soundfont.h"
#include "style.h"

namespace Dx8 {

class DirectMusic;
class DlsCollection;
class SoundFont;
class Music;

class PatternList final {
  public:
    struct Pattern {
      std::string name;
      };

    PatternList()=default;
    PatternList(PatternList&&)=default;

    PatternList& operator = (PatternList&&)=default;

    auto   operator[](size_t i) const -> const Pattern& { return intern->pptn[i]; }
    size_t size() const;

    void dbgDumpPatternList() const;
    void dbgDump(const size_t patternId) const;

  private:
    PatternList(const Segment& s,DirectMusic& owner);

    struct Instrument final {
      const DlsCollection*  dls=nullptr;
      float                 volume=0.f;
      float                 pan=0.f;
      uint32_t              dwPatch=0;
      };

    struct InsInternal final {
      uint32_t              key;
      Dx8::SoundFont        font;
      float                 volume  =0.f;
      float                 pan     =0.f;

      uint32_t              dwVariationChoices[32]={};
      uint8_t               dwVarCount=0;
      };

    struct Note final {
      uint64_t              at         =0;
      uint64_t              duration   =0;
      uint8_t               note       =0;
      uint8_t               velosity   =127;
      uint32_t              dwVariation=0;
      InsInternal*          inst       =nullptr;
      };

    struct Curve final {
      uint64_t              at         =0;
      uint64_t              duration   =0;
      Shape                 shape      =DMUS_CURVES_LINEAR;
      float                 startV     =0.f;
      float                 endV       =0.f;
      Control               ctrl       =BankSelect;
      uint32_t              dwVariation=0;
      InsInternal*          inst       =nullptr;
      };

    struct Groove final {
      uint64_t at           = 0;
      uint8_t  bGrooveLevel = 0;
      uint8_t  bGrooveRange = 0;
      };

    struct PatternInternal final : Pattern {
      DMUS_IO_PATTERN    ptnh;
      DMUS_IO_STYLE      styh;

      uint64_t           timeTotal=0;

      std::vector<InsInternal> instruments;
      std::vector<Note>   waves;
      std::vector<Curve>  volume;
      std::vector<Groove> groove;
      };

    struct Internal final {
      std::vector<PatternInternal> pptn;
      };

    void index();
    void index(const Style &stl, PatternInternal& inst, const Dx8::Pattern &pattern);
    void index(PatternInternal &idx, InsInternal *inst, const Style &stl, const Style::Part &part);

    void dbgDump(const Style &stl, const Dx8::Pattern::PartRef &pref, const Style::Part &part) const;

    DirectMusic*                  owner=nullptr;

    uint32_t                      cordHeader=0;
    std::vector<DMUS_IO_SUBCHORD> subchord;
    const Style*                  style=nullptr;
    std::vector<DMUS_IO_COMMAND>  commands;

    std::unordered_map<uint32_t,Instrument> instruments;
    std::shared_ptr<Internal>     intern;


    friend class DirectMusic;
    friend class Mixer;
    friend class Music;
  };

}

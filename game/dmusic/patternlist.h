#pragma once

#include <Tempest/Sound>
#include <unordered_map>
#include <atomic>
#include <string>
#include <vector>

#include "segment.h"
#include "soundfont.h"
#include "style.h"

namespace Dx8 {

class DirectMusic;
class DlsCollection;
class SoundFont;
class Music;
class ChordMap;

class PatternList final {
  public:
    struct Pattern {
      std::string name;
      };

    struct DebugNote final {
      uint64_t             at         = 0;
      uint64_t             duration   = 0;
      uint8_t              note       = 0;
      uint8_t              velocity   = 127;
      uint32_t             variation  = 0;
      uint32_t             pChannel   = 0;
      uint32_t             patch      = 0;
      const DlsCollection* dls        = nullptr;
      std::string          instrumentName;
      };

    struct DebugPattern final {
      std::string            name;
      DMUS_IO_PATTERN        header;
      DMUS_IO_STYLE          style;
      uint64_t               timeTotal = 0;
      std::vector<DebugNote> notes;
      };

    struct ChordDebug final {
      size_t      styleChordMaps    = 0;
      size_t      timelineChordMaps = 0;
      size_t      chordEvents       = 0;
      size_t      explicitEvents    = 0;
      size_t      namedAllEvents    = 0;
      size_t      namedEvents       = 0;
      size_t      resolvedEvents    = 0;
      size_t      noteTotal         = 0;
      size_t      noteDropped       = 0;
      size_t      noteFixed         = 0;
      size_t      noteFixedToKey    = 0;
      size_t      noteFixedToChord  = 0;
      size_t      noteKeyRoot       = 0;
      size_t      noteChordRoot     = 0;
      size_t      noteScaleInt      = 0;
      size_t      noteChordInt      = 0;
      bool        enhancedMapping   = true;
      std::string firstChordName;
      uint32_t    firstChordPattern = 0;
      uint32_t    firstScalePattern = 0;
      uint8_t     firstChordRoot    = 0;
      uint8_t     firstScaleRoot    = 0;
      std::string unresolvedExample;
      std::vector<std::string> chordTrace;
      std::vector<std::string> noteTrace;
      };

    PatternList()=default;
    PatternList(PatternList&&)=default;

    PatternList& operator = (PatternList&&)=default;

    auto   operator[](size_t i) const -> const Pattern& { return intern->pptn[i]; }
    size_t size() const;

    void getDebugPatterns(std::vector<DebugPattern>& out) const;
    void getChordDebug(ChordDebug& out) const;

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
      const DlsCollection*  dls     =nullptr;
      uint32_t              dwPatch =0;
      std::string           instrumentName;

      uint32_t              dwVariationChoices[32]={};
      uint8_t               variationIds[32]={};
      uint8_t               dwVarCount=0;
      uint8_t               variationLockId=0;
      uint8_t               subChordLevel=0;
      uint8_t               randomVariation=0;
      uint8_t               activeVariation=0;
      };

    struct Note final {
      uint64_t              at         =0;
      uint64_t              duration   =0;
      uint64_t              preRoll    =0;
      uint8_t               note       =0;
      uint8_t               velosity   =127;
      uint16_t              timeRange  =0;
      uint16_t              durRange   =0;
      uint8_t               velRange   =0;
      DMUS_NOTEF_FLAGS      noteFlags  = DMUS_NOTEF_FLAGS(0);
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
      DMUS_PATTERNT_TYPES repeatMode = DMUS_PATTERNT_RANDOM;
      };

    struct TempoEvent final {
      uint64_t at    = 0;
      double   tempo = 0.0;
      };

    struct ChordEvent final {
      uint32_t                       mtTime  = 0;
      uint16_t                       wMeasure = 0;
      uint8_t                        bBeat = 0;
      uint64_t                       at      = 0;
      uint32_t                       header  = 0;
      bool                           explicitSubchord = false;
      std::u16string                 name;
      std::vector<DMUS_IO_SUBCHORD>  subchord;
      };

    struct ChordMapEvent final {
      uint32_t         mtTime = 0;
      uint64_t         at     = 0;
      const ChordMap*  map    = nullptr;
      };

    struct PatternInternal final : Pattern {
      DMUS_IO_PATTERN    ptnh;
      DMUS_IO_STYLE      styh;

      uint64_t           timeTotal=0;

      std::vector<InsInternal> instruments;
      std::vector<Note>   waves;
      std::vector<Curve>  volume;     // CC7 ChannelVolume + CC11 Expression curves
      std::vector<Curve>  pan;        // CC10 Pan curves — applied in Mixer::panFromCurve
      std::vector<Curve>  pitchBend;  // DMUS_CURVET_PBCURVE — applied via SoundFont::setPitchBendNormalized
      std::vector<Curve>  aftertouch; // DMUS_CURVET_MATCURVE Channel Aftertouch — parsed; not yet applied
      };

    struct Internal final {
      std::vector<PatternInternal> pptn;
      std::vector<Groove>          groove;
      std::vector<TempoEvent>      tempo;
      uint64_t                     segmentLoopStart=0;
      uint64_t                     segmentLoopEnd=0;
      uint64_t                     segmentTimeTotal=0;
      double                       baseTempo=100.0;
      };

    void index();
    void index(const Style &stl, PatternInternal& inst, const Dx8::Pattern &pattern);
    void index(PatternInternal &idx,
               InsInternal *inst,
               const Style &stl,
               const Style::Part &part,
               uint64_t patternLengthMs);

    void dbgDump(const Style &stl, const Dx8::Pattern::PartRef &pref, const Style::Part &part) const;

    DirectMusic*                  owner=nullptr;

    uint32_t                      cordHeader=0;
    std::vector<DMUS_IO_SUBCHORD> subchord;
    std::vector<ChordEvent>       chordEvents;
    std::vector<const ChordMap*>  styleChordMaps;
    std::vector<ChordMapEvent>    chordMapEvents;
    std::vector<TempoEvent>       tempoEventsRaw;
    double                        tempoOverride=0.0;
    const Style*                  style=nullptr;
    std::vector<DMUS_IO_COMMAND>  commands;
    bool                          hasSegmentHeader=false;
    DMUS_IO_SEGMENT_HEADER        segmentHeader={};
    bool                          useEnhancedChordMapping = true;

    std::unordered_map<uint32_t,Instrument> instruments;
    std::shared_ptr<Internal>     intern;

    ChordDebug                    chordDebug;


    friend class DirectMusic;
    friend class Mixer;
    friend class Music;
  };

}

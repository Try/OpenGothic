#include "patternlist.h"

#include <Tempest/Log>
#include <fstream>
#include <algorithm>
#include <cstdio>
#include <limits>
#ifdef DM_CURVE_AUDIT
#include <unordered_map>
#endif

#include "dlscollection.h"
#include "directmusic.h"
#include "music.h"
#include "style.h"

using namespace Dx8;
using namespace Tempest;

// from DX8 SDK docs: https://docs.microsoft.com/en-us/previous-versions/ms808181(v%3Dmsdn.10)
static int64_t musicOffset(uint32_t mtGridStart, int16_t nTimeOffset, const DMUS_IO_TIMESIG& timeSig, double tempo) {
  // const uint32_t ppq  = 768;

  // t=100 ->  600ms per grid
  // t=85  ->  705ms per grid
  // t=50  -> 1200ms per grid
  // t=25  -> 2400ms per grid
  const uint32_t ppq  = uint32_t(600*(100.0/tempo));
  const uint32_t mult = (ppq*4)/timeSig.bBeat;
  return int64_t(nTimeOffset) +
         int64_t((mtGridStart / timeSig.wGridsPerBeat) * (mult)) +
         int64_t((mtGridStart % timeSig.wGridsPerBeat) * (mult/timeSig.wGridsPerBeat));
  }

static uint32_t musicDuration(uint32_t duration, double tempo) {
  return uint32_t(duration*100.0/tempo);
  }

static uint64_t musicTimeToScaledUnits(uint32_t mtTime, double tempo) {
  if(mtTime==0u)
    return 0u;
  const uint64_t ppq = uint64_t(600*(100.0/tempo));
  return (uint64_t(mtTime)*ppq + 384u)/768u;
  }

static uint8_t keyRootFromChordHeader(const uint32_t chordHeader) {
  return uint8_t((chordHeader >> 24u) & 0xFFu);
  }

static DMUS_PLAYMODE_FLAGS resolvePlayMode(const DMUS_IO_STYLENOTE& note,
                                           const DMUS_PLAYMODE_FLAGS partPlayMode) {
  return note.bPlayModeFlags == DMUS_PLAYMODE_NONE
      ? partPlayMode
      : note.bPlayModeFlags;
  }

static uint32_t storedRangeToActualRange(uint8_t range) {
  // DirectMusic stores wide random ranges in a compressed byte representation.
  if(range<=190u)
    return range;
  if(range<=212u)
    return (uint32_t(range)-190u)*5u + 190u;
  if(range<=232u)
    return (uint32_t(range)-212u)*10u + 300u;
  return (uint32_t(range)-232u)*50u + 500u;
  }

static uint64_t musicCommandTime(const DMUS_IO_COMMAND& cmd, const DMUS_IO_TIMESIG& timeSig, double tempo) {
  if(cmd.mtTime!=0u)
    return musicTimeToScaledUnits(cmd.mtTime, tempo);
  if(cmd.wMeasure!=0u || cmd.bBeat!=0u) {
    const uint32_t ppq = uint32_t(600*(100.0/tempo));
    const uint32_t beatLen = (ppq*4u)/std::max<uint32_t>(timeSig.bBeat,1u);
    const uint64_t pulses = uint64_t(cmd.wMeasure)*uint64_t(std::max<uint32_t>(timeSig.bBeatsPerMeasure,1u))*uint64_t(beatLen) +
                            uint64_t(cmd.bBeat)*uint64_t(beatLen);
    return pulses;
    }
  return 0;
  }

static uint8_t normalizeGrooveRange(uint8_t range) {
  // DirectMusic: values above 100 are invalid, odd ranges are rounded down.
  if(range>100u)
    return 0u;
  if((range & 1u)!=0u)
    return uint8_t(range-1u);
  return range;
  }

static uint64_t musicChordTime(uint32_t mtTime, uint16_t wMeasure, uint8_t bBeat,
                               const DMUS_IO_TIMESIG& timeSig, double tempo) {
  if(wMeasure!=0u || bBeat!=0u) {
    const uint32_t ppq = uint32_t(600*(100.0/tempo));
    const uint32_t beatLen = (ppq*4u)/std::max<uint32_t>(timeSig.bBeat,1u);
    const uint64_t pulses = uint64_t(wMeasure)*uint64_t(std::max<uint32_t>(timeSig.bBeatsPerMeasure,1u))*uint64_t(beatLen) +
                            uint64_t(bBeat)*uint64_t(beatLen);
    return pulses;
    }
  if(mtTime!=0u)
    return musicTimeToScaledUnits(mtTime, tempo);
  return 0;
  }

static uint32_t bitCount(uint32_t value) {
  uint32_t count = 0;
  while(value!=0u) {
    value &= (value - 1u);
    ++count;
    }
  return count;
  }

static uint32_t fixupScale(uint32_t scale, uint8_t scaleRoot) {
  static constexpr uint32_t FallbackScales[12] = {
    0xab5ab5,
    0x6ad6ad,
    0x5ab5ab,
    0xad5ad5,
    0x6b56b5,
    0x5ad5ad,
    0x56b56b,
    0xd5ad5a,
    0xb56b56,
    0xd6ad6a,
    0xb5ab5a,
    0xad6ad6,
    };

  // Force the scale to two octaves, then apply the scale root.
  scale = (scale & 0x0FFFu) | (scale << 12u);
  const uint32_t normalizedRoot = uint32_t(scaleRoot) % 12u;
  scale = scale >> (12u - normalizedRoot);
  scale = (scale & 0x0FFFu) | (scale << 12u);

  if(bitCount(scale & 0x0FFFu) <= 4u) {
    uint32_t bestScale = FallbackScales[0];
    uint32_t bestScore = 0;
    for(uint32_t i=0; i<12u; ++i) {
      const uint32_t score = bitCount((FallbackScales[i] & scale) & 0x0FFFu);
      if(score > bestScore) {
        bestScale = FallbackScales[i];
        bestScore = score;
        }
      }
    scale = bestScale;
    }

  if((scale & 0xFF000000u)==0u)
    scale |= (scale & 0x00FFF000u) << 12u;
  return scale;
  }

// Sentinel returned by musicValueToMidiAdvanced() to distinguish
// "this note is unmappable under the current playMode/chord" from
// a legitimately low computed noteValue (which can be slightly
// negative before the caller's octave-wrap normalises it). Previously
// both cases were signalled by a bare "< 0" return, which silently
// dropped valid bass notes when octave=0 and CHORD_ROOT mode subtracted
// 12. Using an extreme sentinel lets the caller's octave-wrap loop
// rescue low-but-valid values.
static constexpr int kMusicValueUnmappable = std::numeric_limits<int>::min();

static int musicValueToMidiAdvanced(
    const DMUS_IO_STYLENOTE& note,
    const DMUS_PLAYMODE_FLAGS playMode,
    const uint32_t chordHeader,
    const DMUS_IO_SUBCHORD* chord,
    const bool useEnhancedChordMapping) {
  uint16_t value = note.wMusicValue;
  int32_t octaveOffset = 0;

  while(value >= 0xE000u) {
    value = uint16_t(value + 0x1000u);
    octaveOffset -= 12;
    }

  const uint16_t musicTmp = uint16_t((value & 0x00F0u) + 0x0070u);
  if((musicTmp & 0x0F00u) != 0u) {
    value = uint16_t((value & 0xFF0Fu) | (musicTmp & 0x00F0u));
    octaveOffset -= 12;
    }

  const uint32_t chordPattern = (chord && chord->dwChordPattern != 0u) ? chord->dwChordPattern : 1u;
  uint32_t scalePattern = (chord && chord->dwScalePattern != 0u) ? chord->dwScalePattern : 0x00AB5AB5u;
  const uint8_t keyRoot = keyRootFromChordHeader(chordHeader);
  const uint8_t chordRoot = chord ? chord->bChordRoot : keyRoot;
  const uint8_t scaleRoot = chord ? chord->bScaleRoot : chordRoot;
  scalePattern = fixupScale(scalePattern, scaleRoot);

  uint16_t root = 0;
  if((playMode & DMUS_PLAYMODE_CHORD_ROOT) != 0)
    root = chordRoot;
  else if((playMode & DMUS_PLAYMODE_KEY_ROOT) != 0) {
    if(!useEnhancedChordMapping)
      return kMusicValueUnmappable;
    root = keyRoot;
    }

  if((playMode & (DMUS_PLAYMODE_CHORD_INTERVALS | DMUS_PLAYMODE_SCALE_INTERVALS)) == 0)
    return kMusicValueUnmappable;

  const uint16_t chordPosition = (value & 0x0F00u) >> 8u;
  const uint16_t scalePosition = (value & 0x0070u) >> 4u;
  int16_t noteAccidentals = int16_t(value & 0x000Fu);
  if(noteAccidentals > 8)
    noteAccidentals = int16_t(noteAccidentals - 16);

  int32_t noteValue = 0;
  int32_t noteOffset = 0;
  uint32_t notePattern = 0;
  uint16_t notePosition = 0;
  const uint16_t rootOctave = uint16_t(root % 12u);
  const uint16_t chordBits = uint16_t(bitCount(chordPattern));

  if((playMode & DMUS_PLAYMODE_CHORD_INTERVALS) && scalePosition==0u && chordPosition<chordBits) {
    noteOffset = int32_t(root) + noteAccidentals;
    notePattern = chordPattern;
    notePosition = chordPosition;
    }
  else if((playMode & DMUS_PLAYMODE_CHORD_INTERVALS) && chordPosition<chordBits) {
    notePattern = chordPattern;
    notePosition = chordPosition;

    if(notePattern != 0u) {
      while((notePattern & 1u)==0u) {
        notePattern >>= 1u;
        noteValue += 1;
        }
      }

    if(notePosition > 0u) {
      do {
        notePattern >>= 1u;
        noteValue += 1;
        if((notePattern & 1u) != 0u)
          notePosition = uint16_t(notePosition - 1u);

        if(notePattern == 0u) {
          noteValue += notePosition;
          break;
          }
        } while(notePosition > 0u);
      }

    noteValue += rootOctave;
    noteOffset = noteAccidentals + int32_t(root) - int32_t(rootOctave);
    notePattern = scalePattern >> (uint32_t(noteValue) % 12u);
    notePosition = scalePosition;
    }
  else if((playMode & DMUS_PLAYMODE_SCALE_INTERVALS) != 0) {
    noteValue = rootOctave;
    noteOffset = noteAccidentals + int32_t(root) - int32_t(rootOctave);
    notePattern = scalePattern >> rootOctave;
    notePosition = uint16_t(chordPosition * 2u + scalePosition);
    }
  else {
    return kMusicValueUnmappable;
    }

  notePosition = uint16_t(notePosition + 1u);
  while(notePosition > 0u) {
    noteValue += 1;
    if((notePattern & 1u) != 0u)
      notePosition = uint16_t(notePosition - 1u);

    if(notePattern == 0u) {
      noteValue += notePosition;
      break;
      }
    notePattern >>= 1u;
    }

  noteValue -= 1;
  noteValue += noteOffset;
  noteValue += octaveOffset;

  const int32_t octave = int32_t((value >> 12u) & 0x0Fu);
  if((playMode & DMUS_PLAYMODE_CHORD_ROOT) != 0)
    noteValue = octave * 12 + noteValue - 12;
  else
    noteValue = octave * 12 + noteValue;

  return noteValue;
  }

static bool musicValueToMIDI(const DMUS_IO_STYLENOTE&             note,
                             const DMUS_PLAYMODE_FLAGS            partPlayMode,
                             const uint32_t&                      chord,
                             const DMUS_IO_SUBCHORD*              subchord,
                             uint8_t&                             value,
                             const bool                            useEnhancedChordMapping) {
  const DMUS_PLAYMODE_FLAGS flags = resolvePlayMode(note,partPlayMode);

  if(flags == DMUS_PLAYMODE_FIXED) {
    value = uint8_t(note.wMusicValue);
    return true;
    }

  if((flags & (DMUS_PLAYMODE_CHORD_INTERVALS | DMUS_PLAYMODE_SCALE_INTERVALS)) == 0) {
    if(!useEnhancedChordMapping)
      return false;

    int32_t noteValue = uint8_t(note.wMusicValue);
    const uint8_t keyRoot = keyRootFromChordHeader(chord);
    const uint8_t chordRoot = subchord ? subchord->bChordRoot : keyRoot;
    const int32_t keyOffset = int32_t(keyRoot % 12u);
    const int32_t chordOffset = int32_t(chordRoot % 12u);
    if((flags & DMUS_PLAYMODE_CHORD_ROOT) != 0)
      noteValue += chordOffset;
    else if((flags & DMUS_PLAYMODE_KEY_ROOT) != 0)
      noteValue += keyOffset;

    while(noteValue<0)
      noteValue += 12;
    while(noteValue>127)
      noteValue -= 12;

    value = uint8_t(noteValue);
    return true;
    }

  int32_t noteValue = musicValueToMidiAdvanced(note, flags, chord, subchord, useEnhancedChordMapping);
  if(noteValue == kMusicValueUnmappable)
    return false;

  // Normalise to MIDI [0,127] via octave-wrap. Legitimate low values
  // (e.g. octave=0 + CHORD_ROOT subtracts 12 → noteValue=-12) are
  // rescued here instead of being discarded as "errors".
  while(noteValue<0)
    noteValue += 12;
  while(noteValue>127)
    noteValue -= 12;

  value = uint8_t(noteValue);
  return true;
  }

static const DMUS_IO_SUBCHORD* selectSubchord(const std::vector<DMUS_IO_SUBCHORD>& all, uint8_t level) {
  if(all.empty())
    return nullptr;
  const uint32_t bit = (level<32u ? (1u<<level) : 0u);
  if(bit!=0u) {
    for(const auto& sc : all)
      if((sc.dwLevels & bit)!=0u)
        return &sc;
    }
  return &all[0];
  }

static std::u16string readChordName(const char16_t (&name)[16]) {
  std::u16string ret;
  for(size_t i=0;i<16;++i) {
    if(name[i]==u'\0')
      break;
    ret.push_back(name[i]);
    }
  while(!ret.empty() && (ret.back()==u' ' || ret.back()==u'\t'))
    ret.pop_back();
  return ret;
  }

static std::string toAscii(const std::u16string& in) {
  std::string out;
  out.reserve(in.size());
  for(char16_t c : in)
    out.push_back(c<128 ? char(c) : '?');
  return out;
  }

static std::string playModeToString(const DMUS_PLAYMODE_FLAGS mode) {
  if(mode==DMUS_PLAYMODE_FIXED)
    return "FIXED";

  std::string out;
  const auto append = [&out](const char* token) {
    if(!out.empty())
      out.push_back('|');
    out += token;
    };

  if((mode & DMUS_PLAYMODE_KEY_ROOT)!=0)
    append("KEY");
  if((mode & DMUS_PLAYMODE_CHORD_ROOT)!=0)
    append("CHORD");
  if((mode & DMUS_PLAYMODE_SCALE_INTERVALS)!=0)
    append("SCALE_INT");
  if((mode & DMUS_PLAYMODE_CHORD_INTERVALS)!=0)
    append("CHORD_INT");
  if((mode & DMUS_PLAYMODE_NONE)!=0)
    append("NONE");

  if(out.empty())
    return "0";
  return out;
  }

PatternList::PatternList(const Segment &s, DirectMusic &owner)
  :owner(&owner), intern(std::make_shared<Internal>()) {
  hasSegmentHeader = s.hasHeader;
  if(hasSegmentHeader)
    segmentHeader = s.header;
  useEnhancedChordMapping = owner.isEnhancedChordMappingEnabled();
  chordDebug = ChordDebug();
  styleChordMaps.clear();
  chordMapEvents.clear();
  for(const auto& track : s.track) {
    if(track.sttr!=nullptr) {
      auto& sttr = *track.sttr;
      for(const auto& styleRef : sttr.styles){
        auto& stl = owner.style(styleRef.reference);
        this->style = &stl;
        for(const auto& chordRef : stl.chordMaps) {
          try {
            const ChordMap* map = &owner.chordMap(chordRef);
            if(std::find(styleChordMaps.begin(),styleChordMaps.end(),map)==styleChordMaps.end())
              styleChordMaps.push_back(map);
            }
          catch(const std::exception&) {
            }
          }
        for(auto& band:stl.band) {
          for(auto& r:band.intrument){
            if(r.reference.file.empty())
              continue;
            auto& dls = owner.dlsCollection(r.reference);
            Instrument ins;
            ins.dls     = &dls;
            if((r.header.dwFlags&DMUS_IO_INST_VOLUME)!=0)
              ins.volume = r.header.bVolume/127.f; else
              ins.volume = 1.f;
            if((r.header.dwFlags&DMUS_IO_INST_PAN)!=0)
              ins.pan = r.header.bPan/127.f; else
              ins.pan = 0.5f;

            ins.dwPatch = r.header.dwPatch;

            if(ins.volume<0.f)
              ins.volume=0;

            instruments[r.header.dwPChannel] = ins;
            }
          }
        }
      }
    else if(track.pftr!=nullptr) {
      for(const auto& mapRef : track.pftr->chordMaps) {
        try {
          const ChordMap* map = &owner.chordMap(mapRef.reference);
          ChordMapEvent ev;
          ev.mtTime = mapRef.stmp;
          ev.at     = 0;
          ev.map    = map;
          chordMapEvents.push_back(ev);
          }
        catch(const std::exception&) {
          }
        }
      }
    else if(track.cord!=nullptr) {
      cordHeader = track.cord->header;
      subchord   = track.cord->subchord;
      chordEvents.clear();
      chordEvents.reserve(track.cord->events.size());
      for(const auto& e : track.cord->events) {
        if((e.ioChord.bFlags & DMUS_CHORDKEYF_SILENT)!=0)
          continue;

        ChordEvent ce;
        ce.mtTime  = e.ioChord.mtTime;
        ce.wMeasure= e.ioChord.wMeasure;
        ce.bBeat   = e.ioChord.bBeat;
        ce.at      = 0;
        ce.header  = e.header;
        ce.explicitSubchord = !e.subchord.empty();
        ce.name    = readChordName(e.ioChord.wszName);
        ce.subchord = e.subchord;
        chordEvents.push_back(std::move(ce));
        }
      }
    else if(track.cmnd!=nullptr) {
      commands = track.cmnd->commands;
      }
    else if(track.tetr!=nullptr) {
      for(const auto& e : track.tetr->events)
        if(e.tempo>0.0) {
          TempoEvent te;
          te.at    = e.time;
          te.tempo = e.tempo;
          tempoEventsRaw.push_back(te);
          }
      }
    }

  if(!styleChordMaps.empty() || !chordMapEvents.empty())
    Log::i("chordmaps attached: style=",styleChordMaps.size(),
           " timeline=",chordMapEvents.size());
  chordDebug.styleChordMaps    = styleChordMaps.size();
  chordDebug.timelineChordMaps = chordMapEvents.size();

  index();
  }

void PatternList::index() {
  if(!style)
    return;
  chordDebug = ChordDebug();
  chordDebug.styleChordMaps    = styleChordMaps.size();
  chordDebug.timelineChordMaps = chordMapEvents.size();
  chordDebug.enhancedMapping   = useEnhancedChordMapping;

  const Style& stl = *style;
  tempoOverride = 0.0;
  for(const auto& e : tempoEventsRaw) {
    if(e.at==0u) {
      tempoOverride = e.tempo;
      break;
      }
    }
  const double tempo = tempoOverride>0.0 ? tempoOverride : stl.styh.dblTempo;
  intern->baseTempo = tempo;
  intern->segmentLoopStart = 0;
  intern->segmentLoopEnd   = 0;
  intern->segmentTimeTotal = 0;

  if(hasSegmentHeader) {
    intern->segmentTimeTotal = musicTimeToScaledUnits(segmentHeader.mtLength, tempo);
    intern->segmentLoopStart = musicTimeToScaledUnits(segmentHeader.mtLoopStart, tempo);
    intern->segmentLoopEnd   = musicTimeToScaledUnits(segmentHeader.mtLoopEnd, tempo);
    if(intern->segmentLoopEnd<=intern->segmentLoopStart) {
      intern->segmentLoopStart = 0;
      intern->segmentLoopEnd   = 0;
      }
    }

  if(intern->segmentTimeTotal==0u) {
    uint64_t lastGrooveAt = 0u;
    for(const auto& cmd : commands) {
      if(cmd.bCommand!=DMUS_COMMANDT_GROOVE)
        continue;
      lastGrooveAt = std::max(lastGrooveAt, musicCommandTime(cmd, stl.styh.timeSig, tempo));
      }
    if(lastGrooveAt>0u) {
      intern->segmentTimeTotal = lastGrooveAt;
      intern->segmentLoopStart = 0;
      intern->segmentLoopEnd   = lastGrooveAt;
      }
    }

  intern->tempo.clear();
  intern->tempo.reserve(tempoEventsRaw.size()+1);
  for(const auto& e : tempoEventsRaw) {
    TempoEvent te;
    te.at    = musicTimeToScaledUnits(uint32_t(e.at), tempo);
    te.tempo = e.tempo;
    intern->tempo.push_back(te);
    }
  std::sort(intern->tempo.begin(),intern->tempo.end(),[](const TempoEvent& a, const TempoEvent& b) {
    return a.at<b.at;
    });
  {
  std::vector<TempoEvent> uniq;
  uniq.reserve(intern->tempo.size()+1);
  for(const auto& e : intern->tempo) {
    if(!uniq.empty() && uniq.back().at==e.at)
      uniq.back() = e;
    else
      uniq.push_back(e);
    }
  if(uniq.empty() || uniq.front().at!=0u) {
    TempoEvent te;
    te.at    = 0;
    te.tempo = tempo;
    uniq.insert(uniq.begin(),te);
    }
  intern->tempo.swap(uniq);
  }

  for(auto& e : chordMapEvents)
    e.at = musicTimeToScaledUnits(e.mtTime, tempo);
  std::sort(chordMapEvents.begin(),chordMapEvents.end(),[](const ChordMapEvent& a, const ChordMapEvent& b) {
    return a.at<b.at;
    });

  for(auto& e : chordEvents)
    e.at = musicChordTime(e.mtTime, e.wMeasure, e.bBeat, stl.styh.timeSig, tempo);
  std::sort(chordEvents.begin(),chordEvents.end(),[](const ChordEvent& a, const ChordEvent& b) {
    return a.at<b.at;
    });

  if(!chordEvents.empty()) {
    static constexpr size_t kChordTraceLimit = 24;
    std::vector<DMUS_IO_SUBCHORD> activeSubchord = subchord;
    const ChordMap* activeChordMap = nullptr;
    size_t mapPos = 0;
    size_t namedCount = 0;
    size_t namedAllCount = 0;
    size_t resolvedCount = 0;
    size_t explicitCount = 0;
    std::vector<std::string> unresolved;

    for(auto& e : chordEvents) {
      if(!e.name.empty())
        namedAllCount++;

      while(mapPos<chordMapEvents.size() && chordMapEvents[mapPos].at<=e.at) {
        if(chordMapEvents[mapPos].map!=nullptr)
          activeChordMap = chordMapEvents[mapPos].map;
        ++mapPos;
        }

      std::vector<DMUS_IO_SUBCHORD> eventSubchord;
      bool eventResolvedByName = false;

      // Named chords take priority: resolve by name first even when an explicit
      // inline subchord is also present — the .cmp palette entry carries the
      // authoritative chord voicing that the segment was composed against.
      if(!e.name.empty()) {
        namedCount++;
        bool resolved = false;

        if(activeChordMap!=nullptr && activeChordMap->resolve(e.name,eventSubchord))
          resolved = true;
        else {
          for(const ChordMap* map : styleChordMaps) {
            if(map==nullptr || map==activeChordMap)
              continue;
            if(map->resolve(e.name,eventSubchord)) {
              resolved = true;
              break;
              }
            }
          // Global fallback: search every loaded chord map via the DirectMusic owner.
          // Covers .cmp files that are loaded into memory but not linked from the
          // style's chordMaps list (styleChordMaps can be empty for such tracks).
          // Only attempt when the event has no inline subchord — if it does, the
          // inline data is the best fallback we have.
          if(!resolved && !e.explicitSubchord && owner!=nullptr) {
            for(const auto& loadedMap : owner->chordMapList()) {
              if(loadedMap.second.resolve(e.name,eventSubchord)) {
                resolved = true;
                break;
                }
              }
            }
          }

        if(resolved) {
          resolvedCount++;
          eventResolvedByName = true;
          }
        else if(unresolved.size()<4)
          unresolved.push_back(toAscii(e.name));
        }

      // Use the explicit inline subchord only as a last resort when name resolution
      // produced nothing (avoids overriding a correct palette lookup with stale data).
      if(eventSubchord.empty() && e.explicitSubchord && !e.subchord.empty()) {
        explicitCount++;
        eventSubchord = e.subchord;
        }

      if(!eventSubchord.empty())
        activeSubchord = eventSubchord;
      e.subchord = activeSubchord;

      if(chordDebug.chordTrace.size()<kChordTraceLimit) {
        const bool named = !e.name.empty();
        const std::string chordName = named ? toAscii(e.name) : std::string();
        uint32_t chordPattern = 0;
        uint32_t scalePattern = 0;
        int chordRoot = -1;
        int scaleRoot = -1;
        if(!e.subchord.empty()) {
          chordPattern = e.subchord[0].dwChordPattern;
          scalePattern = e.subchord[0].dwScalePattern;
          chordRoot = e.subchord[0].bChordRoot;
          scaleRoot = e.subchord[0].bScaleRoot;
          }

        char line[512] = {};
        std::snprintf(line,
                      sizeof(line),
                      "evt t=%llu mt=%u m=%u b=%u name='%s' explicit=%d named=%d resolved=%d cp=0x%08x sp=0x%08x cr=%d sr=%d",
                      static_cast<unsigned long long>(e.at),
                      static_cast<unsigned>(e.mtTime),
                      static_cast<unsigned>(e.wMeasure),
                      static_cast<unsigned>(e.bBeat),
                      chordName.c_str(),
                      e.explicitSubchord ? 1 : 0,
                      named ? 1 : 0,
                      eventResolvedByName ? 1 : 0,
                      chordPattern,
                      scalePattern,
                      chordRoot,
                      scaleRoot);
        chordDebug.chordTrace.emplace_back(line);
        }
      }

    chordDebug.chordEvents       = chordEvents.size();
    chordDebug.explicitEvents    = explicitCount;
    chordDebug.namedAllEvents    = namedAllCount;
    chordDebug.namedEvents       = namedCount;
    chordDebug.resolvedEvents    = resolvedCount;
    chordDebug.unresolvedExample = unresolved.empty() ? std::string() : unresolved[0];
    chordDebug.firstChordName    = chordEvents[0].name.empty() ? std::string() : toAscii(chordEvents[0].name);
    if(!chordEvents[0].subchord.empty()) {
      const auto& first = chordEvents[0].subchord[0];
      chordDebug.firstChordPattern = first.dwChordPattern;
      chordDebug.firstScalePattern = first.dwScalePattern;
      chordDebug.firstChordRoot    = first.bChordRoot;
      chordDebug.firstScaleRoot    = first.bScaleRoot;
      }

    if(namedCount>0)
      Log::i("chordmap resolve: named=",namedCount,
             " resolved=",resolvedCount,
             " styleMaps=",styleChordMaps.size(),
             " timelineMaps=",chordMapEvents.size());
    if(!unresolved.empty())
      Log::i("chordmap unresolved example: ", unresolved[0]);
    }

  intern->pptn.resize(stl.patterns.size());
  for(size_t i=0;i<stl.patterns.size();++i){
    index(stl,intern->pptn[i],stl.patterns[i]);
    intern->pptn[i].styh = stl.styh;
    if(tempoOverride>0.0)
      intern->pptn[i].styh.dblTempo = tempoOverride;
    }

  for(auto& i:commands) {
    if(i.bCommand==DMUS_COMMANDT_GROOVE) {
      Groove gr;
      gr.at           = musicCommandTime(i, stl.styh.timeSig, tempo);
      gr.bGrooveLevel = i.bGrooveLevel;
      gr.bGrooveRange = normalizeGrooveRange(i.bGrooveRange);
      gr.repeatMode   = (i.bRepeatMode<=DMUS_PATTERNT_RANDOM_ROW)
                      ? i.bRepeatMode
                      : DMUS_PATTERNT_RANDOM;
      intern->groove.push_back(gr);
      }
    }
  std::sort(intern->groove.begin(), intern->groove.end(), [](const Groove& a, const Groove& b) {
    return a.at < b.at;
    });
  }

void PatternList::getChordDebug(ChordDebug& out) const {
  out = chordDebug;
  }

void PatternList::index(const Style& stl,PatternInternal &inst, const Dx8::Pattern &pattern) {
  inst.waves.clear();
  inst.volume.clear();
  inst.name.assign(pattern.info.unam.begin(),pattern.info.unam.end());

  inst.ptnh = pattern.header;

  auto& instument = inst.instruments;
  instument.resize(pattern.partref.size());

  // fill instruments upfront
  const double tempo = tempoOverride>0.0 ? tempoOverride : stl.styh.dblTempo;
  const uint64_t patternLengthMs = pattern.timeLength(tempo);
  // Helper: find instrument by trying dwPChannel (full, low word, high word) then
  // wLogicalPartID.  Gothic segments store the channel in dwPChannel; older/simpler
  // segments use wLogicalPartID.  Trying both avoids silent voices on mixed content.
  auto findInst = [&](const Dx8::Pattern::PartRef& pref) {
    auto it = instruments.end();
    if(pref.io.dwPChannel != 0u) {
      it = instruments.find(pref.io.dwPChannel);
      if(it == instruments.end())
        it = instruments.find(pref.io.dwPChannel & 0x0000FFFFu);
      if(it == instruments.end())
        it = instruments.find((pref.io.dwPChannel >> 16u) & 0x0000FFFFu);
      }
    if(it == instruments.end())
      it = instruments.find(uint32_t(pref.io.wLogicalPartID));
    return it;
    };

  for(size_t i=0;i<pattern.partref.size();++i) {
    const auto& pref = pattern.partref[i];
    auto pinst = findInst(pref);
    if(pinst==instruments.end())
      continue;
    auto& pr = instument[i];
    const Instrument& ins = ((*pinst).second);
    pr.key    = pinst->first;
    pr.font   = ins.dls->toSoundfont(ins.dwPatch);
    pr.dls    = ins.dls;
    pr.dwPatch = ins.dwPatch;

    pr.font.setVolume(ins.volume);
    pr.font.setPan(ins.pan);

    pr.volume = ins.volume;
    pr.pan    = ins.pan;
    pr.instrumentName.clear();
    if(const DlsCollection::Instrument* dlsInstrument = ins.dls->findInstrument(ins.dwPatch)) {
      pr.instrumentName = dlsInstrument->info.inam;
      }

    auto part = stl.findPart(pref.io.guidPartID);
    if(part!=nullptr) {
      std::memcpy(pr.dwVariationChoices,part->header.dwVariationChoices,sizeof(pr.dwVariationChoices));
      pr.variationLockId = pref.io.bVariationLockID;
      pr.subChordLevel   = pref.io.bSubChordLevel;
      pr.randomVariation = pref.io.bRandomVariation;
      pr.dwVarCount = 0;
      pr.activeVariation = 0;
      for(uint8_t vid=0; vid<32; ++vid) {
        if((pr.dwVariationChoices[vid] & 0x0FFFFFFFu)==0u)
          break;
        pr.variationIds[pr.dwVarCount] = vid;
        pr.dwVarCount++;
        }
      if(pr.dwVarCount>0)
        pr.activeVariation = pr.variationIds[0];
      }
    }

  for(size_t i=0;i<pattern.partref.size();++i) {
    const auto& pref = pattern.partref[i];
    auto        part = stl.findPart(pref.io.guidPartID);
    if(part==nullptr)
      continue;
    index(inst,&instument[i],stl,*part,patternLengthMs);
    }

  std::sort(inst.waves.begin(),inst.waves.end(),[](const Note& a,const Note& b){
    return a.at<b.at;
    });
  std::sort(inst.volume.begin(),inst.volume.end(),[](const Curve& a,const Curve& b){
    return a.at<b.at;
    });

  inst.timeTotal = inst.waves.size()>0 ? patternLengthMs : 0;
  }

void PatternList::index(PatternInternal &idx, InsInternal* inst,
                        const Style& stl, const Style::Part &part, uint64_t patternLengthMs) {
  static constexpr size_t kNoteTraceLimit = 48;
  const double tempo = tempoOverride>0.0 ? tempoOverride : stl.styh.dblTempo;
  auto findChordEvent = [this](uint64_t timeMs) -> const ChordEvent* {
    if(chordEvents.empty())
      return nullptr;
    const ChordEvent* current = &chordEvents[0];
    for(const auto& e : chordEvents) {
      if(e.at>timeMs)
        break;
      current = &e;
      }
    return current;
    };

  auto normalizePatternTime = [patternLengthMs](int64_t inTime) -> uint64_t {
    if(patternLengthMs==0)
      return uint64_t(std::max<int64_t>(0,inTime));
    int64_t t = inTime % int64_t(patternLengthMs);
    if(t<0)
      t += int64_t(patternLengthMs);
    return uint64_t(t);
    };

  for(auto& i:part.notes) {
    const int64_t timeRaw = musicOffset(i.mtGridStart, i.nTimeOffset, part.header.timeSig, tempo);
    const uint64_t time = normalizePatternTime(timeRaw);
    const ChordEvent* chordEv = findChordEvent(time);
    const auto& allSubchords = chordEv ? chordEv->subchord : subchord;
    const DMUS_IO_SUBCHORD* chord = selectSubchord(allSubchords, inst ? inst->subChordLevel : 0u);
    const DMUS_PLAYMODE_FLAGS playMode = resolvePlayMode(i,part.header.bPlayModeFlags);
    const uint32_t dur = musicDuration(i.mtDuration, tempo);

    chordDebug.noteTotal++;
    if(playMode==DMUS_PLAYMODE_FIXED)
      chordDebug.noteFixed++;
    else if(playMode==DMUS_PLAYMODE_KEY_ROOT)
      chordDebug.noteFixedToKey++;
    else if(playMode==DMUS_PLAYMODE_CHORD_ROOT)
      chordDebug.noteFixedToChord++;
    if((playMode & DMUS_PLAYMODE_KEY_ROOT)!=0)
      chordDebug.noteKeyRoot++;
    if((playMode & DMUS_PLAYMODE_CHORD_ROOT)!=0)
      chordDebug.noteChordRoot++;
    if((playMode & DMUS_PLAYMODE_SCALE_INTERVALS)!=0)
      chordDebug.noteScaleInt++;
    if((playMode & DMUS_PLAYMODE_CHORD_INTERVALS)!=0)
      chordDebug.noteChordInt++;

    auto appendNoteTrace = [&](const bool dropped, const uint8_t midiNote, const uint64_t durationMs, const uint64_t preRollMs) {
      if(chordDebug.noteTrace.size()>=kNoteTraceLimit)
        return;

      const std::string chordName = (chordEv!=nullptr && !chordEv->name.empty()) ? toAscii(chordEv->name) : std::string();
      uint32_t chordPattern = 0;
      uint32_t scalePattern = 0;
      int chordRoot = -1;
      int scaleRoot = -1;
      if(chord!=nullptr) {
        chordPattern = chord->dwChordPattern;
        scalePattern = chord->dwScalePattern;
        chordRoot = chord->bChordRoot;
        scaleRoot = chord->bScaleRoot;
        }

      char line[640] = {};
      std::snprintf(
        line,
        sizeof(line),
        "note tRaw=%lld t=%llu dur=%llu pre=%llu mv=0x%04x midi=%d drop=%d pm=%s vel=%u vr=%u tr=%u dr=%u var=0x%08x pch=%u patch=0x%08x chord='%s' cp=0x%08x sp=0x%08x cr=%d sr=%d",
        static_cast<long long>(timeRaw),
        static_cast<unsigned long long>(time),
        static_cast<unsigned long long>(durationMs),
        static_cast<unsigned long long>(preRollMs),
        static_cast<unsigned>(i.wMusicValue),
        static_cast<int>(midiNote),
        dropped ? 1 : 0,
        playModeToString(playMode).c_str(),
        static_cast<unsigned>(i.bVelocity),
        static_cast<unsigned>(i.bVelRange),
        static_cast<unsigned>(i.bTimeRange),
        static_cast<unsigned>(i.bDurRange),
        static_cast<unsigned>(i.dwVariation),
        inst!=nullptr ? static_cast<unsigned>(inst->key) : 0u,
        inst!=nullptr ? static_cast<unsigned>(inst->dwPatch) : 0u,
        chordName.c_str(),
        chordPattern,
        scalePattern,
        chordRoot,
        scaleRoot);
      chordDebug.noteTrace.emplace_back(line);
      };

    uint8_t  note = 0;
    if(!musicValueToMIDI(i,
                         part.header.bPlayModeFlags,
                         chordEv ? chordEv->header : cordHeader,
                         chord,
                         note,
                         useEnhancedChordMapping)) {
      // Fallback: use raw wMusicValue clamped to valid MIDI range instead of
      // silently dropping the note.  Keeps melodic content audible even when
      // chord mapping fails (e.g. unresolved chord name, missing .cmp).
      note = uint8_t(i.wMusicValue & 0x7Fu);
      chordDebug.noteDropped++;
      appendNoteTrace(true, note, dur, timeRaw<0 ? uint64_t(-timeRaw) : 0u);
      }

    Note rec;
    rec.at          = time;
    rec.duration    = dur;
    rec.preRoll     = timeRaw<0 ? uint64_t(-timeRaw) : 0;
    rec.note        = note;
    rec.velosity    = i.bVelocity;
    rec.timeRange   = uint16_t(std::min<uint32_t>(musicDuration(storedRangeToActualRange(i.bTimeRange), tempo),
                                                  std::numeric_limits<uint16_t>::max()));
    rec.durRange    = uint16_t(std::min<uint32_t>(musicDuration(storedRangeToActualRange(i.bDurRange), tempo),
                                                  std::numeric_limits<uint16_t>::max()));
    rec.velRange    = i.bVelRange;
    rec.noteFlags   = i.bNoteFlags;
    rec.inst        = inst;
    rec.dwVariation = i.dwVariation;

    idx.waves.push_back(rec);
    appendNoteTrace(false, note, rec.duration, rec.preRoll);
    }

  // One-shot diagnostic histogram — tells us empirically which curve types
  // Gothic actually authored so we don't implement features that are never used.
  // Enable by defining DM_CURVE_AUDIT at compile time (off by default).
#ifdef DM_CURVE_AUDIT
  static std::unordered_map<uint16_t,uint32_t> curveHist;
  static bool curveHistShown = false;
  static uint32_t curveCallCount = 0;
  curveCallCount++;
#endif

  for(auto& i:part.curves) {
    const int64_t timeRaw = musicOffset(i.mtGridStart, i.nTimeOffset, part.header.timeSig, tempo);
    const uint64_t time = normalizePatternTime(timeRaw);
    uint32_t dur  = musicDuration(i.mtDuration, tempo);

    // Type-aware range validation.
    // Per NT5 dmime/dmperf.cpp (ComputeCurve) values are in native MIDI units:
    //   CC / MAT / PAT  → 0..127      (7-bit MIDI)
    //   PB / RPN / NRPN → 0..16383    (14-bit MIDI, center = 0x2000)
    // The old unconditional  (> 127) filter silently dropped 100% of PB
    // curves — Gothic has 76 of them (range 204..11058), all rejected.
    uint16_t maxVal = 127;
    if(i.bEventType==DMUS_CURVET_PBCURVE ||
       i.bEventType==DMUS_CURVET_RPNCURVE ||
       i.bEventType==DMUS_CURVET_NRPNCURVE)
      maxVal = 16383;
    if(i.nStartValue>maxVal || i.nEndValue>maxVal)
      continue;

#ifdef DM_CURVE_AUDIT
    // Key = (bEventType<<8) | bCCData — bCCData is meaningless for non-CC types
    // but keeping it produces a finer-grained histogram.
    const uint16_t histKey = uint16_t((uint16_t(i.bEventType)<<8) | uint16_t(i.bCCData));
    curveHist[histKey]++;
#endif

    Curve c;
    c.at          = time;
    c.duration    = dur;
    c.shape       = i.bCurveShape;
    c.ctrl        = i.bCCData;
    if(i.bEventType==DMUS_CURVET_PBCURVE ||
       i.bEventType==DMUS_CURVET_RPNCURVE ||
       i.bEventType==DMUS_CURVET_NRPNCURVE) {
      // 14-bit, center 0x2000 → normalize to [-1..+1] around centre.
      c.startV = (float(i.nStartValue) - 8192.f) / 8192.f;
      c.endV   = (float(i.nEndValue)   - 8192.f) / 8192.f;
    } else {
      // 7-bit → [0..1]
      c.startV = float(i.nStartValue & 0x7F) / 127.f;
      c.endV   = float(i.nEndValue   & 0x7F) / 127.f;
    }
    c.dwVariation = i.dwVariation;
    c.inst        = inst;

    // Dispatch CC curves by their actual control code.
    // Previously ANY CCCURVE was shoved into idx.volume — which meant Pan
    // curves (CC10) were silently routed as volume modulators. Fixed.
    if(i.bEventType==DMUS_CURVET_CCCURVE) {
      switch(i.bCCData) {
        case ChannelVolume:   // CC7  — track/channel volume
        case ExpressionCtl:   // CC11 — expression (in DX a second volume multiplier)
          idx.volume.push_back(c);
          break;
        case Pan:             // CC10 — time-varying pan (rare; see audit)
          idx.pan.push_back(c);
          break;
        default:
          // CC1 (Mod), CC64 (Sustain), CC91 (Reverb) etc. — currently ignored.
          // Counted in the histogram (if audit enabled) so we can tell if Gothic
          // actually uses them; no silent routing into volume anymore.
          break;
        }
      }
    // Pitch Bend curves — applied via SoundFont::setPitchBendNormalized in the mixer.
    else if(i.bEventType==DMUS_CURVET_PBCURVE) {
      idx.pitchBend.push_back(c);
      }
    // Channel Aftertouch — parsed so the audit histogram reflects them and so a
    // future change can apply them; no synth hookup yet (would modulate volume
    // or filter cutoff depending on the DLS instrument's MAT routing).
    else if(i.bEventType==DMUS_CURVET_MATCURVE) {
      idx.aftertouch.push_back(c);
      }
    }

#ifdef DM_CURVE_AUDIT
  // Emit once after most patterns have been parsed to get a global picture.
  if(!curveHistShown && curveCallCount>64 && !curveHist.empty()) {
    curveHistShown = true;
    Tempest::Log::i("[dmusic curve audit] histogram of (bEventType<<8 | bCCData):");
    for(auto& [k,v] : curveHist) {
      const uint8_t et = uint8_t(k>>8);
      const uint8_t cc = uint8_t(k & 0xFF);
      const char* etName = "?";
      switch(et) {
        case DMUS_CURVET_CCCURVE:   etName = "CC";      break;
        case DMUS_CURVET_PBCURVE:   etName = "PB";      break;
        case DMUS_CURVET_MATCURVE:  etName = "MAT";     break;
        case DMUS_CURVET_PATCURVE:  etName = "PAT";     break;
        case DMUS_CURVET_RPNCURVE:  etName = "RPN";     break;
        case DMUS_CURVET_NRPNCURVE: etName = "NRPN";    break;
        }
      char buf[64];
      std::snprintf(buf, sizeof(buf), "  %-4s cc=%3u count=%u", etName, unsigned(cc), unsigned(v));
      Tempest::Log::i(buf);
      }
    }
#endif
  }

size_t PatternList::size() const {
  return intern->pptn.size();
  }

void PatternList::getDebugPatterns(std::vector<DebugPattern>& out) const {
  out.clear();
  if(!intern) {
    return;
    }

  out.reserve(intern->pptn.size());
  for(const auto& pattern : intern->pptn) {
    DebugPattern dbg;
    dbg.name      = pattern.name;
    dbg.header    = pattern.ptnh;
    dbg.style     = pattern.styh;
    dbg.timeTotal = pattern.timeTotal;
    dbg.notes.reserve(pattern.waves.size());

    for(const auto& note : pattern.waves) {
      DebugNote dn;
      dn.at        = note.at;
      dn.duration  = note.duration;
      dn.note      = note.note;
      dn.velocity  = note.velosity;
      dn.variation = note.dwVariation;
      if(note.inst!=nullptr) {
        dn.pChannel       = note.inst->key;
        dn.patch          = note.inst->dwPatch;
        dn.dls            = note.inst->dls;
        dn.instrumentName = note.inst->instrumentName;
        }
      dbg.notes.emplace_back(std::move(dn));
      }

    out.emplace_back(std::move(dbg));
    }
  }

void PatternList::dbgDumpPatternList() const {
  for(auto& i:style->patterns) {
    std::string st(i.info.unam.begin(),i.info.unam.end());
    Log::i("pattern: ",st);
    }
  Log::i("---");
  }

void PatternList::dbgDump(const size_t patternId) const {
  if(!style || patternId>=style->patterns.size())
    return;
  const Style&        stl = *style;
  const Dx8::Pattern& p   = stl.patterns[patternId];

  std::string str(p.info.unam.begin(),p.info.unam.end());
  Log::i("pattern: ",str," ",p.timeLength(stl.styh.dblTempo));
  for(size_t i=0;i<p.partref.size();++i) {
    auto& pref = p.partref[i];
    auto  part = stl.findPart(pref.io.guidPartID);
    if(part==nullptr)
      continue;

    if(part->notes.size()>0 || part->curves.size()>0) {
      std::string st(pref.unfo.unam.begin(),pref.unfo.unam.end());
      Log::i("part: ",i," ",st," partid=",pref.io.wLogicalPartID);
      if(/* DISABLES CODE */ (false)) {
        for(auto& varChoice : part->header.dwVariationChoices) {
          char buf[16]={};
          std::snprintf(buf,sizeof(buf),"%#010x",varChoice);
          Log::i("  var: ",buf);
          }
        }
      dbgDump(stl,pref,*part);
      }
    }
  }

void PatternList::dbgDump(const Style& stl,const Dx8::Pattern::PartRef& pref,const Style::Part &part) const {
  auto findChordEvent = [this](uint64_t timeMs) -> const ChordEvent* {
    if(chordEvents.empty())
      return nullptr;
    const ChordEvent* current = &chordEvents[0];
    for(const auto& e : chordEvents) {
      if(e.at>timeMs)
        break;
      current = &e;
      }
    return current;
    };

  for(auto& i:part.notes) {
    int64_t time = musicOffset(i.mtGridStart, i.nTimeOffset, part.header.timeSig, stl.styh.dblTempo);
    if(time<0)
      time = 0;
    const ChordEvent* chordEv = findChordEvent(uint64_t(time));
    const auto& allSubchords = chordEv ? chordEv->subchord : subchord;
    const DMUS_IO_SUBCHORD* chord = selectSubchord(allSubchords, pref.io.bSubChordLevel);
    uint8_t  note = 0;
    if(!musicValueToMIDI(i,
                         part.header.bPlayModeFlags,
                         chordEv ? chordEv->header : cordHeader,
                         chord,
                         note,
                         useEnhancedChordMapping))
      continue;

    int cId = note/12;
    auto inst = instruments.find(pref.io.wLogicalPartID);
    if(inst!=instruments.end()) {
      float vol = inst->second.volume;
      float vel = i.bVelocity;
      auto w = (*inst).second.dls->findWave(note);
      const char* name = w==nullptr ? "" : w->info.inam.c_str();
      Log::i("  note:[C",cId," ", note, "] {",
             time," - ",time+i.mtDuration,"} ","vol = ",vol," var=",i.dwVariation," vel=",vel," ",name);
      }
    }

  for(auto& i:part.curves) {
    const char* name = "";
    switch(i.bCCData) {
      case Dx8::ExpressionCtl: name = "ExpressionCtl"; break;
      case Dx8::ChannelVolume: name = "ChannelVolume"; break;
      default: name = "?";
      }
    int64_t time = musicOffset(i.mtGridStart, i.nTimeOffset, part.header.timeSig, stl.styh.dblTempo);
    if(time<0)
      time = 0;

    Log::i("  curve:[", name, "] ",time," - ",time+i.mtDuration," var=",i.dwVariation);
    }
  }

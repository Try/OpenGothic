#include "music.h"

#include <Tempest/Log>
#include <fstream>
#include <algorithm>

#include "dlscollection.h"
#include "directmusic.h"
#include "style.h"

using namespace Dx8;
using namespace Tempest;

// from DX8 SDK docs
static uint32_t musicOffset(uint32_t mtGridStart, int16_t nTimeOffset, const DMUS_IO_TIMESIG& timeSig) {
  const uint32_t ppq  = 768;
  const uint32_t mult = (ppq*4)/timeSig.bBeat;
  return uint32_t(nTimeOffset) +
         (mtGridStart / timeSig.wGridsPerBeat) * (mult) +
         (mtGridStart % timeSig.wGridsPerBeat) * (mult/timeSig.wGridsPerBeat);
  }

static bool offsetFromScale(const uint8_t degree, const uint32_t scale, uint8_t& offset) {
  uint8_t cnt=0;
  for(uint8_t i=0; i<24; i++) {
    if(scale & (1 << i)) {
      offset = i;
      if(cnt==degree)
        return true;
      cnt++;
      }
    }
  return false;
  }

static bool musicValueToMIDI(const DMUS_IO_STYLENOTE&             note,
                             const uint32_t&                      chord,
                             const std::vector<DMUS_IO_SUBCHORD>& subchords,
                             uint8_t&                             value) {
  if(note.bPlayModeFlags == DMUS_PLAYMODE_FIXED) {
    value = uint8_t(note.wMusicValue);
    return true;
    }

  uint32_t dwChordPattern = 0x00000091;
  uint32_t dwScalePattern = 0x00AB5AB5;

  if(subchords.size()>0) {
    dwChordPattern = subchords[0].dwChordPattern;
    dwScalePattern = subchords[0].dwScalePattern;
    }

  uint8_t octave    = ((note.wMusicValue & 0xF000) >> 12);
  uint8_t chordTone = ((note.wMusicValue & 0x0F00) >> 8);
  uint8_t scaleTone = ((note.wMusicValue & 0x00F0) >> 4);

  // Explanation: the accidentals are represented as a two's complement 4bit value.
  // We first take only the last four bits from the word, then we shift it left
  // while keeping it unsigned, so that when we convert it into a signed byte
  // it has the correct sign. Then we divide by 16 to simulate an arithmetic
  // right shift of 4, to bring the value back into the correct range
  int accidentals = int8_t(note.wMusicValue & 0x000F);
  if(accidentals>7)
    accidentals = (accidentals - 16);

  int noteValue = ((chord & 0xFF000000) >> 24) + 12 * octave;
  std::uint8_t chordOffset = 0;
  std::uint8_t scaleOffset = 0;
  if(offsetFromScale(chordTone, dwChordPattern, chordOffset)) {
    noteValue += chordOffset;
    } else {
    return false;
    }

  if(scaleTone && offsetFromScale(scaleTone, dwScalePattern >> chordOffset, scaleOffset)) {
    noteValue += scaleOffset;
    }

  noteValue += accidentals;
  while(noteValue<0)
    noteValue += 12;
  while(noteValue>127)
    noteValue -= 12;

  value = uint8_t(noteValue);
  return true;
  }

Music::Music(const Segment &s, DirectMusic &owner)
  :owner(&owner), intern(std::make_shared<Internal>()) {
  for(const auto& track : s.track) {
    auto& head = track.head;
    if(head.ckid[0]==0 && std::memcmp(head.fccType,"sttr",4)==0) {
      auto& sttr = *track.sttr;
      for(const auto& style : sttr.styles){
        auto& stl = owner.style(style.reference);
        this->style = &stl;
        for(auto& band:stl.band) {
          for(auto& r:band.intrument){
            auto& dls = owner.dlsCollection(r.reference);
            Instrument ins;
            ins.dls    = &dls;
            ins.volume = r.header.bVolume/128.f;
            ins.pan    = (r.header.bPan/255.f)*2.f-1.f;

            instruments[r.header.dwPChannel] = ins;
            }
          }
        }
      }
    else if(head.ckid[0]==0 && std::memcmp(head.fccType,"cord",4)==0) {
      cordHeader = track.cord->header;
      subchord   = track.cord->subchord;
      }
    }
  index();
  }

void Music::index() {
  if(!style)
    return;
  const Style& stl = *style;

  auto& idxWaves = intern->idxWaves;
  idxWaves.clear();
  uint64_t timeOffset=0;
  for(auto& p:stl.patterns) {
    //Log::i("pattern: ",p.timeLength());
    for(size_t i=0;i<p.partref.size();++i) {
      auto& pref = p.partref[i];
      auto  part = stl.findPart(pref.io.guidPartID);
      if(part==nullptr)
        continue;

      if(part->notes.size()>0 || part->curves.size()>0) {
        //std::string st(pref.unfo.unam.begin(),pref.unfo.unam.end());
        //Log::i("part: ",i," ",st," partid=",pref.io.wLogicalPartID);
        //if(pref.io.wLogicalPartID==12)
          index(pref,*part,timeOffset);
        }
      }
    timeOffset += p.timeLength();
    }

  idxWaves.shrink_to_fit();
  if(idxWaves.size()==0)
    return;
  std::sort(idxWaves.begin(),idxWaves.end(),[](const Record& a,const Record& b){
    return a.at<b.at;
    });
  uint64_t t0=idxWaves[0].at;
  for(auto& i:idxWaves)
    i.at-=t0;
  intern->timeTotal = timeOffset-t0;
  }

void Music::index(const Pattern::PartRef& pref,const Style::Part &part,uint64_t timeOffset) {
  for(auto& i:part.notes) {
    uint32_t time = musicOffset(i.mtGridStart, i.nTimeOffset, part.header.timeSig);
    uint8_t  note = 0;
    if(!musicValueToMIDI(i,cordHeader,subchord,note))
      continue;

    auto inst = instruments.find(pref.io.wLogicalPartID);
    if(inst!=instruments.end()) {
      const Instrument& ins = ((*inst).second);
      auto              w   = ins.dls->findWave(note);
      if(w!=nullptr) {
        Record rec;
        rec.at       = timeOffset+time;
        rec.duration = i.mtDuration;
        rec.wave     = w;
        rec.volume   = ins.volume;
        rec.pan      = ins.pan;
        rec.pitch    = i.bVelocity/100.f;

        intern->idxWaves.push_back(rec);
        }
      }
    }
  }

void Music::exec(const size_t patternId) const {
  if(!style)
    return;
  const Style&   stl = *style;
  const Pattern& p   = stl.patterns[patternId];

  Log::i("pattern: ",p.timeLength());
  for(size_t i=0;i<p.partref.size();++i) {
    auto& pref = p.partref[i];
    auto  part = stl.findPart(pref.io.guidPartID);
    if(part==nullptr)
      continue;

    if(part->notes.size()>0 || part->curves.size()>0) {
      std::string st(pref.unfo.unam.begin(),pref.unfo.unam.end());
      Log::i("part: ",i," ",st," partid=",pref.io.wLogicalPartID);
      exec(stl,pref,*part);
      }
    }
  }

void Music::exec(const Style&,const Pattern::PartRef& pref,const Style::Part &part) const {
  for(auto& i:part.notes) {
    uint32_t time = musicOffset(i.mtGridStart, i.nTimeOffset, part.header.timeSig);
    uint8_t  note = 0;
    if(!musicValueToMIDI(i,cordHeader,subchord,note))
      continue;

    auto inst = instruments.find(pref.io.wLogicalPartID);
    if(inst!=instruments.end()) {
      auto w = (*inst).second.dls->findWave(note);
      //const char* n = w ? w->info.inam.c_str() : "";
      //Log::i("  note:[", note, " ", n ,"] ",time," - ",time+i.mtDuration," var=",i.dwVariation);
      }
    }
  }

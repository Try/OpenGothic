#include "music.h"

#include <Tempest/Log>
#include <fstream>
#include <algorithm>

#include "dlscollection.h"
#include "directmusic.h"
#include "style.h"

using namespace Dx8;
using namespace Tempest;

// from DX8 SDK docs: https://docs.microsoft.com/en-us/previous-versions/ms808181(v%3Dmsdn.10)
static uint32_t musicOffset(uint32_t mtGridStart, int16_t nTimeOffset, const DMUS_IO_TIMESIG& timeSig, double tempo) {
  // const uint32_t ppq  = 768;

  // t=100 ->  600ms per grid
  // t=85  ->  705ms per grid
  // t=50  -> 1200ms per grid
  // t=25  -> 2400ms per grid
  const uint32_t ppq  = uint32_t(600*(100.0/tempo));
  const uint32_t mult = (ppq*4)/timeSig.bBeat;
  return uint32_t(nTimeOffset) +
         (mtGridStart / timeSig.wGridsPerBeat) * (mult) +
         (mtGridStart % timeSig.wGridsPerBeat) * (mult/timeSig.wGridsPerBeat);
  }

static uint32_t musicDuration(uint32_t duration, double tempo) {
  return uint32_t(duration*100.0/tempo);
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

  uint8_t octave    = uint8_t((note.wMusicValue & 0xF000) >> 12);
  uint8_t chordTone = uint8_t((note.wMusicValue & 0x0F00) >>  8);
  uint8_t scaleTone = uint8_t((note.wMusicValue & 0x00F0) >>  4);

  int accidentals = int8_t(note.wMusicValue & 0x000F);
  if(accidentals>7)
    accidentals = (accidentals - 16);

  int noteValue = ((chord & 0xFF000000) >> 24) + 12 * octave;
  uint8_t chordOffset = 0;
  uint8_t scaleOffset = 0;
  if(offsetFromScale(chordTone, dwChordPattern, chordOffset)) {
    noteValue += chordOffset;
    } else {
    noteValue += chordOffset+4; //FIXME
    //return false;
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

PatternList::PatternList(const Segment &s, DirectMusic &owner)
  :owner(&owner), intern(std::make_shared<Internal>()) {
  for(const auto& track : s.track) {
    if(track.sttr!=nullptr) {
      auto& sttr = *track.sttr;
      for(const auto& style : sttr.styles){
        auto& stl = owner.style(style.reference);
        this->style = &stl;
        for(auto& band:stl.band) {
          for(auto& r:band.intrument){
            if(r.reference.file.empty())
              continue;
            auto& dls = owner.dlsCollection(r.reference);
            Instrument ins;
            ins.dls     = &dls;
            ins.volume  = r.header.bVolume/127.f;
            ins.pan     = r.header.bPan/127.f;
            ins.dwPatch = r.header.dwPatch;

            if(ins.volume<0.f)
              ins.volume=0;

            instruments[r.header.dwPChannel] = ins;
            }
          }
        }
      }
    else if(track.cord!=nullptr) {
      cordHeader = track.cord->header;
      subchord   = track.cord->subchord;
      }
    }
  index();
  }

void PatternList::index() {
  if(!style)
    return;
  const Style& stl = *style;
  intern->pptn.resize(stl.patterns.size());
  for(size_t i=0;i<stl.patterns.size();++i){
    index(stl,intern->pptn[i],stl.patterns[i]);
    intern->pptn[i].styh = stl.styh;
    }
  }

void PatternList::index(const Style& stl,PatternInternal &inst, const Dx8::Pattern &pattern) {
  inst.waves.clear();
  inst.volume.clear();
  inst.name.assign(pattern.info.unam.begin(),pattern.info.unam.end());

  auto& instument = inst.instruments;
  instument.resize(pattern.partref.size());

  // fill instruments upfront
  for(size_t i=0;i<pattern.partref.size();++i) {
    const auto& pref = pattern.partref[i];
    auto pinst = instruments.find(pref.io.wLogicalPartID);
    if(pinst==instruments.end())
      continue;
    if(pref.io.wLogicalPartID!=26)
      ;//continue;
    auto& pr = instument[i];
    const Instrument& ins = ((*pinst).second);
    pr.key    = pref.io.wLogicalPartID;
    pr.font   = ins.dls->toSoundfont(ins.dwPatch);

    pr.font.setVolume(ins.volume);
    pr.font.setPan(ins.pan);

    pr.volume = ins.volume;
    pr.pan    = ins.pan;
    }

  for(size_t i=0;i<pattern.partref.size();++i) {
    const auto& pref = pattern.partref[i];
    auto        part = stl.findPart(pref.io.guidPartID);
    if(part==nullptr)
      continue;
    index(inst,&instument[i],stl,*part);
    }

  std::sort(inst.waves.begin(),inst.waves.end(),[](const Note& a,const Note& b){
    return a.at<b.at;
    });
  std::sort(inst.volume.begin(),inst.volume.end(),[](const Curve& a,const Curve& b){
    return a.at<b.at;
    });

  inst.timeTotal = inst.waves.size()>0 ? pattern.timeLength(stl.styh.dblTempo) : 0;
  }

void PatternList::index(PatternInternal &idx, InsInternal* inst,
                        const Style& stl, const Style::Part &part) {
  for(auto& i:part.notes) {
    uint8_t  note = 0;
    if(!musicValueToMIDI(i,cordHeader,subchord,note))
      continue;

    uint32_t time = musicOffset(i.mtGridStart, i.nTimeOffset, part.header.timeSig, stl.styh.dblTempo);
    uint32_t dur  = musicDuration(i.mtDuration, stl.styh.dblTempo);

    Note rec;
    rec.at       = time;
    rec.duration = dur;
    rec.note     = note;
    rec.velosity = i.bVelocity;
    rec.inst     = inst;

    idx.waves.push_back(rec);
    }

  for(auto& i:part.curves) {
    uint32_t time = musicOffset(i.mtGridStart, i.nTimeOffset, part.header.timeSig, stl.styh.dblTempo);
    uint32_t dur  = musicDuration(i.mtDuration, stl.styh.dblTempo);
    if(i.nStartValue>127 || i.nEndValue>127)
      continue;

    Curve c;
    c.at       = time;
    c.duration = dur;
    c.shape    = i.bCurveShape;
    c.ctrl     = i.bCCData;
    c.startV   = float(i.nStartValue&0x7F)/127.f;
    c.endV     = float(i.nEndValue  &0x7F)/127.f;
    c.inst     = inst;
    if(i.bEventType==DMUS_CURVET_CCCURVE)
      idx.volume.push_back(c);
    }
  }

size_t PatternList::size() const {
  return intern->pptn.size();
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

  Log::i("pattern: ",p.timeLength(stl.styh.dblTempo));
  for(size_t i=0;i<p.partref.size();++i) {
    auto& pref = p.partref[i];
    auto  part = stl.findPart(pref.io.guidPartID);
    if(part==nullptr)
      continue;

    if(part->notes.size()>0 || part->curves.size()>0) {
      std::string st(pref.unfo.unam.begin(),pref.unfo.unam.end());
      Log::i("part: ",i," ",st," partid=",pref.io.wLogicalPartID);
      dbgDump(stl,pref,*part);
      }
    }
  }

void PatternList::dbgDump(const Style& stl,const Dx8::Pattern::PartRef& pref,const Style::Part &part) const {
  for(auto& i:part.notes) {
    uint32_t time = musicOffset(i.mtGridStart, i.nTimeOffset, part.header.timeSig, stl.styh.dblTempo);
    uint8_t  note = 0;
    if(!musicValueToMIDI(i,cordHeader,subchord,note))
      continue;

    auto inst = instruments.find(pref.io.wLogicalPartID);
    if(inst!=instruments.end()) {
      auto w = (*inst).second.dls->findWave(note);
      Log::i("  note:[", note, "] ",time," - ",time+i.mtDuration," var=",i.dwVariation," ",w->info.inam);
      }
    }

  for(auto& i:part.curves) {
    const char* name = "";
    switch(i.bCCData) {
      case Dx8::ExpressionCtl: name = "ExpressionCtl"; break;
      case Dx8::ChannelVolume: name = "ChannelVolume"; break;
      default: name = "?";
      }
    uint32_t time = musicOffset(i.mtGridStart, i.nTimeOffset, part.header.timeSig, stl.styh.dblTempo);

    Log::i("  curve:[", name, "] ",time," - ",time+i.mtDuration," var=",i.dwVariation);
    }
  }

#include "mixer.h"

#include <Tempest/Application>
#include <Tempest/SoundEffect>
#include <Tempest/Sound>
#include <Tempest/Log>
#include <cmath>
#include <set>

#include "soundfont.h"
#include "wave.h"

using namespace Dx8;
using namespace Tempest;

static int64_t toSamples(uint64_t time) {
  return int64_t(time*SoundFont::SampleRate)/1000;
  }

Mixer::Mixer() {
  const size_t reserve=2048;
  pcm.reserve(reserve*2);
  pcmMix.reserve(reserve*2);
  vol.reserve(reserve);
  // uniqInstr.reserve(32);
  }

Mixer::~Mixer() {
  for(auto& i:active)
    SoundFont::noteOff(i.ticket);
  }

void Mixer::setMusic(const Music& m,DMUS_EMBELLISHT_TYPES e) {
  if(current==m.impl)
    return;

  nextMus = m.impl;
  embellishment.store(e);
  }

void Mixer::setMusicVolume(float v) {
  if(current)
    current->volume.store(v);
  }

int64_t Mixer::currentPlayTime() const {
  return (sampleCursor*1000/SoundFont::SampleRate);
  }

int64_t Mixer::nextNoteOn(PatternList::PatternInternal& part,int64_t b,int64_t e) {
  int64_t nextDt    = std::numeric_limits<int64_t>::max();
  int64_t timeTotal = toSamples(part.timeTotal);
  bool    inv       = (e<b);

  b-=patStart;
  e-=patStart;

  for(auto& i:part.waves) {
    int64_t at = toSamples(i.at);
    if((b<at && at<=e)^inv) {
      int64_t t = at-b;
      if(inv)
        t += timeTotal;
      if(t<nextDt && i.duration>0)
        nextDt = t;
      }
    }

  return nextDt;
  }

int64_t Mixer::nextNoteOff(int64_t b, int64_t /*e*/) {
  int64_t actT = std::numeric_limits<int64_t>::max();

  for(auto& i:active) {
    int64_t at = i.at;
    int64_t dt = at>b ? at-b : 0;
    if(dt<actT)
      actT = dt;
    }
  return actT;
  }

void Mixer::noteOn(std::shared_ptr<PatternInternal>& pattern, PatternList::Note *r) {
  if(!checkVariation(*r))
    return;

  Active a;
  a.at      = sampleCursor + toSamples(r->duration);
  a.ticket  = r->inst->font.noteOn(r->note,r->velosity);
  if(a.ticket==nullptr)
    return;

  for(auto& i:uniqInstr)
    if(i.ptr==r->inst) {
      a.parent = &i;
      a.parent->counter++;
      active.push_back(a);
      return;
      }
  Instr u;
  u.ptr     = r->inst;
  u.pattern = pattern;
  uniqInstr.push_back(u);

  a.parent = &uniqInstr.back();
  a.parent->counter++;

  active.push_back(a);
  }

void Mixer::noteOn(std::shared_ptr<PatternInternal>& pattern, int64_t time) {
  time-=patStart;

  size_t n=0;
  for(auto& i:pattern->waves) {
    int64_t at = toSamples(i.at);
    if(at==time) {
      noteOn(pattern,&i);
      ++n;
      }
    }
  if(n==0)
    throw std::runtime_error("mixer critical error");
  }

void Mixer::noteOff(int64_t time) {
  size_t sz=0;
  for(size_t i=0;i<active.size();++i){
    if(active[i].at>time) {
      active[sz]=active[i];
      sz++;
      } else {
      SoundFont::noteOff(active[i].ticket);
      active[i].parent->counter--;
      }
    }
  active.resize(sz);
  }

void Mixer::nextPattern() {
  auto mus = current;
  if(mus->pptn.size()==0) {
    // no active music
    pattern = nullptr;
    return;
    }

  auto prev = pattern;
  for(size_t i=0;i<mus->pptn.size();++i) {
    if(mus->pptn[i]->timeTotal==0)
      continue;
    pattern = std::shared_ptr<PatternInternal>(mus,mus->pptn[i].get());
    break;
    }
  size_t nextOff=0;
  for(size_t i=0;i<mus->pptn.size();++i){
    if(mus->pptn[i].get()==prev.get()) {
      nextOff = (i+1)%mus->pptn.size();
      break;
      }
    }

  const DMUS_EMBELLISHT_TYPES em = embellishment.exchange(DMUS_EMBELLISHT_NORMAL);
  const int groove = getGroove();
  if(nextMus!=nullptr) {
    current = nextMus;
    nextMus = nullptr;
    grooveCounter.store(0);
    //variationCounter.store(0);
    nextOff=0;
    }

  for(size_t i=0;i<mus->pptn.size();++i) {
    auto& ptr = mus->pptn[(i+nextOff)%mus->pptn.size()];
    if(ptr->timeTotal==0)
      continue;
    if(ptr->ptnh.wEmbellishment!=em)
      continue;

    if(mus->groove.size()==0 || (ptr->ptnh.bGrooveBottom<=groove && groove<=ptr->ptnh.bGrooveTop)) {
      pattern = std::shared_ptr<PatternList::PatternInternal>(mus,ptr.get());
      break;
      }
    }

  if(pattern->timeTotal>0) {
    grooveCounter.fetch_add(1);
    }

  if(em!=DMUS_EMBELLISHT_NORMAL) {
    while(active.size()>0)
      noteOff(active[0].at);
    }

  patStart = sampleCursor;
  patEnd   = patStart+toSamples(pattern->timeTotal);
  for(auto& i:pattern->waves)
    if(i.at==0) {
      noteOn(pattern,&i);
      }
  variationCounter.fetch_add(1);
  }

Mixer::Step Mixer::stepInc(PatternInternal& pptn, int64_t b, int64_t e, int64_t samplesRemain) {
  int64_t nextT   = nextNoteOn (pptn,b,e);
  int64_t offT    = nextNoteOff(b,e);
  int64_t samples = std::min(offT,nextT);

  Step s;
  if(samples>samplesRemain) {
    s.samples=samplesRemain;
    return s;
    }

  s.nextOn  = nextT;//nextNoteOn (pptn,b,e);
  s.nextOff = offT;//nextNoteOff(b,e);
  s.samples = samples;
  return s;
  }

void Mixer::stepApply(std::shared_ptr<PatternInternal> &pptn, const Mixer::Step &s,int64_t b) {
  if(s.nextOff<s.nextOn) {
    noteOff(s.nextOff+b);
    } else
  if(s.nextOff>s.nextOn) {
    noteOn (pptn,s.nextOn+b);
    } else
  if(s.nextOff==s.nextOn) {
    noteOff(s.nextOff+b);
    noteOn (pptn,s.nextOn+b);
    }
  }

std::shared_ptr<Mixer::PatternInternal> Mixer::checkPattern(std::shared_ptr<PatternInternal> p) {
  auto cur = current;
  if(cur==nullptr)
    return nullptr;

  if(p==nullptr) {
    grooveCounter.store(0);
    } else {
    for(auto& i:cur->pptn)
      if(i.get()==p.get()) {
        return p;
        }
    }
  // null or foreign
  nextPattern();
  return pattern;
  }

void Mixer::mix(int16_t *out, size_t samples) {
  std::memset(out,0,2*samples*sizeof(int16_t));

  auto cur = current;
  if(cur==nullptr) {
    current = nextMus;
    return;
    }

  const int64_t samplesTotal = toSamples(cur->timeTotal);
  if(samplesTotal==0) {
    current = nextMus;
    return;
    }

  auto pat = checkPattern(pattern);

  size_t samplesRemain = samples;
  while(samplesRemain>0) {
    if(pat==nullptr)
      break;
    const int64_t remain = std::min(patEnd-sampleCursor,int64_t(samplesRemain));
    const int64_t b      = (sampleCursor       );
    const int64_t e      = (sampleCursor+remain);

    auto& pptn         = *pat;
    const float volume = cur->volume.load()*this->volume.load();

    const Step stp = stepInc(pptn,b,e,remain);
    implMix(pptn,volume,out,size_t(stp.samples));

    if(remain!=stp.samples)
      stepApply(pat,stp,sampleCursor);

    sampleCursor += stp.samples;
    out          += stp.samples*2;
    samplesRemain-= size_t(stp.samples);

    // HACK: some music in addonworld.zen has odd paddings in the end of each track
    if(stp.nextOn==std::numeric_limits<int64_t>::max() && uniqInstr.size()==0)
      sampleCursor = patEnd;

    if(sampleCursor==patEnd || nextMus!=nullptr) {
      nextPattern();
      if(!stp.isValid())
        break;
      }
    }

  uniqInstr.remove_if([](Instr& i){
    return i.counter==0 && !i.ptr->font.hasNotes();
    });
  }

void Mixer::setVolume(float v) {
  volume.store(v);
  }

void Mixer::implMix(PatternInternal &pptn, float volume, int16_t *out, size_t cnt) {
  const size_t cnt2=cnt*2;
  pcm   .resize(cnt2);
  pcmMix.resize(cnt2);
  vol   .resize(cnt);

  std::memset(pcmMix.data(),0,cnt2*sizeof(pcmMix[0]));

  for(auto& i:uniqInstr) {
    auto& ins = *i.ptr;
    if(!ins.font.hasNotes())
      continue;

    std::memset(pcm.data(),0,cnt2*sizeof(pcm[0]));
    ins.font.mix(pcm.data(),cnt);

    float insVolume = std::pow(ins.volume,2.f);
    if(ins.key==5 || ins.key==6) {
      // HACK
      // insVolume*=0.10f;
      }
    const bool hasVol = hasVolumeCurves(pptn,i);
    if(hasVol) {
      volFromCurve(pptn,i,vol);
      for(size_t r=0;r<cnt2;++r) {
        float v = vol[r/2];
        pcmMix[r] += pcm[r]*insVolume*(v*v);
        }
      } else {
      for(size_t r=0;r<cnt2;++r) {
        float v = i.volLast;
        pcmMix[r] += pcm[r]*insVolume*(v*v);
        }
      }
    }

  for(size_t i=0;i<cnt2;++i) {
    float v = pcmMix[i]*volume;
    out[i] = (v < -1.00004566f ? int16_t(-32768) : (v > 1.00001514f ? int16_t(32767) : int16_t(v * 32767.5f)));
    }
  }

void Mixer::volFromCurve(PatternInternal &part,Instr& inst,std::vector<float> &v) {
  float& base = inst.volLast;
  for(auto& i:v)
    i=base;

  const int64_t shift = sampleCursor-patStart;
  //const int64_t e = s+v.size();

  for(auto& i:part.volume) {
    if(i.inst!=inst.ptr)
      continue;
    if(!checkVariation(i))
      continue;

    int64_t s = toSamples(i.at)-shift;
    int64_t e = toSamples(i.at+i.duration)-shift;
    if((s>=0 && size_t(s)>v.size()) || e<0)
      continue;

    const size_t begin = size_t(std::max<int64_t>(s,0));
    const size_t size  = std::min(size_t(e),v.size());
    const float  range = float(e-s);
    const float  diffV = i.endV-i.startV;
    const float  shift = i.startV;
    const float  endV  = i.endV;

    switch(i.shape) {
      case DMUS_CURVES_LINEAR: {
        for(size_t i=begin;i<size;++i) {
          float val = (float(i)-float(s))/range;
          v[i] = val*diffV+shift;
          }
        break;
        }
      case DMUS_CURVES_INSTANT: {
        for(size_t i=begin;i<size;++i) {
          v[i] = endV;
          }
        break;
        }
      case DMUS_CURVES_EXP: {
        for(size_t i=begin;i<size;++i) {
          float val = (float(i)-float(s))/range;
          v[i] = std::pow(val,2.f)*diffV+shift;
          }
        break;
        }
      case DMUS_CURVES_LOG: {
        for(size_t i=begin;i<size;++i) {
          float val = (float(i)-float(s))/range;
          v[i] = std::sqrt(val)*diffV+shift;
          }
        break;
        }
      case DMUS_CURVES_SINE: {
        for(size_t i=begin;i<size;++i) {
          float linear = (float(i)-float(s))/range;
          float val    = std::sin(float(M_PI)*linear*0.5f);
          v[i] = val*diffV+shift;
          }
        break;
        }
      }
    if(size>begin)
      base = v[size-1];
    }
  }

int Mixer::getGroove() const {
  auto  pmus = current;
  auto& mus  = *pmus;
  if(mus.groove.size()==0)
    return 0;
  auto& g = mus.groove[grooveCounter.load()%mus.groove.size()];
  return g.bGrooveLevel;
  }

bool Mixer::hasVolumeCurves(Mixer::PatternInternal& part, Mixer::Instr& inst) const {
  for(auto& i:part.volume) {
    if(i.inst!=inst.ptr)
      continue;
    if(!checkVariation(i))
      continue;
    return true;
    }
  return false;
  }

template<class T>
bool Mixer::checkVariation(const T& item) const {
  if(item.inst->dwVarCount==0)
    return false;
  uint32_t vbit = variationCounter.load()%item.inst->dwVarCount;
  if((item.dwVariation & (1<<vbit))==0)
    return false;
  return true;
  }

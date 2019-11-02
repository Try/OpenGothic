#include "mixer.h"

#include <Tempest/Application>
#include <Tempest/SoundEffect>
#include <Tempest/Sound>
#include <Tempest/Log>
#include <cmath>

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

  exitFlg.store(false);
  // mixTh = std::thread(&Mixer::thFunc,this);
  }

Mixer::~Mixer() {
  exitFlg.store(true);
  for(auto& i:active){
    setNote(*i.ptr,false);
    }
  // mixTh.join();
  }

void Mixer::setMusic(const Music &m) {
  current = m.intern;
  if(pattern==nullptr)
    nextPattern();
  }

int64_t Mixer::currentPlayTime() const {
  return (sampleCursor*1000/SoundFont::SampleRate);
  }

void Mixer::thFunc() {
  Log::d("start mixer thread");

  Log::d("shutdown mixer thread");
  }

int64_t Mixer::nextNoteOn(Music::PatternInternal& part,int64_t b,int64_t e) {
  int64_t  nextDt    = std::numeric_limits<int64_t>::max();
  int64_t  timeTotal = toSamples(part.timeTotal);
  bool     inv       = (e<b);

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
    int64_t dt = at>=b ? i.at-b : 0;
    if(dt<actT)
      actT = dt;
    }
  return actT;
  }

void Mixer::noteOn(Music::Note *r) {
  for(auto& i:active) {
    if(i.ptr==r) {
      return;
      }
    }

  setNote(*r,true);

  Active a;
  a.ptr = r;
  a.at  = sampleCursor + toSamples(r->duration);
  active.push_back(a);
  }

void Mixer::noteOn(Music::PatternInternal &part, int64_t time) {
  time-=patStart;

  size_t n=0;
  for(auto& i:part.waves) {
    int64_t at = toSamples(i.at);
    if(at==time) {
      noteOn(&i);
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
      setNote(*active[i].ptr,false);
      }
    }
  active.resize(sz);
  }

void Mixer::setNote(Music::Note &rec, bool on) {
  auto i = *rec.inst;
  i.font.setNote(rec.note,on,rec.velosity);
  }

void Mixer::nextPattern() {
  auto mus = current;
  if(mus->pptn.size()==0) {
    // no active music
    pattern = nullptr;
    return;
    }

  auto prev = pattern;
  pattern  = std::shared_ptr<Music::PatternInternal>(mus,&mus->pptn[0]);
  for(size_t i=0;i<mus->pptn.size();++i){
    if(&mus->pptn[i]==prev.get()) {
      size_t next = (i+1)%mus->pptn.size();
      pattern  = std::shared_ptr<Music::PatternInternal>(mus,&mus->pptn[next]);
      break;
      }
    }

  patStart = sampleCursor;
  patEnd   = patStart+toSamples(pattern->timeTotal);
  for(auto& i:pattern->waves)
    if(i.at==0) {
      noteOn(&i);
      }
  volPerInst.resize(pattern->instruments.size(),1.f);
  }

Mixer::Step Mixer::stepInc(Music::PatternInternal& part, int64_t b, int64_t e, int64_t samplesRemain) {
  int64_t nextT   = nextNoteOn (part,b,e);
  int64_t offT    = nextNoteOff(b,e);
  int64_t samples = std::min(offT,nextT);

  Step s;
  if(samples>samplesRemain)  {
    s.samples=samplesRemain;
    return s;
    }

  s.nextOn  = nextNoteOn (part,b,e);
  s.nextOff = nextNoteOff(b,e);
  s.samples = std::min(offT,nextT);
  return s;
  }

void Mixer::stepApply(Music::PatternInternal& part,const Mixer::Step &s,int64_t b) {
  if(s.nextOff<s.nextOn) {
    noteOff(s.nextOff+b);
    } else
  if(s.nextOff>s.nextOn) {
    noteOn (part,s.nextOn+b);
    } else
  if(s.nextOff==s.nextOn) {
    noteOff(s.nextOff+b);
    noteOn (part,s.nextOn+b);
    }
  }

void Mixer::mix(int16_t *out, size_t samples) {
  std::memset(out,0,2*samples*sizeof(int16_t));

  auto cur = current;
  const int64_t samplesTotal = toSamples(cur->timeTotal);
  if(samplesTotal==0)
    return;

  size_t samplesRemain = samples;
  while(samplesRemain>0) {
    if(pattern==nullptr)
      return;
    const int64_t remain = std::min(patEnd-sampleCursor,int64_t(samplesRemain));
    const int64_t b      = (sampleCursor       );
    const int64_t e      = (sampleCursor+remain);

    auto& part        = *pattern;
    const Step    stp = stepInc(part,b,e,remain);
    implMix(part,out,size_t(stp.samples));

    if(remain!=stp.samples)
      stepApply(part,stp,sampleCursor);

    sampleCursor += stp.samples;
    out          += stp.samples*2;
    samplesRemain-= stp.samples;

    if(sampleCursor==patEnd)
      nextPattern();
    }
  }

void Mixer::implMix(Music::PatternInternal &part, int16_t *out, size_t cnt) {
  const size_t cnt2=cnt*2;
  pcm   .resize(cnt2);
  pcmMix.resize(cnt2);
  vol   .resize(cnt);

  std::memset(pcmMix.data(),0,cnt2*sizeof(pcmMix[0]));

  for(auto& i:part.instruments){
    volFromCurve(part,i.first,&i.second,vol);
    //if(!i.second.font.hasNotes())
    //  continue; // DONT!
    std::memset(pcm.data(),0,cnt2*sizeof(pcm[0]));
    i.second.font.mix(pcm.data(),cnt);

    float volume = i.second.volume;
    for(size_t i=0;i<cnt2;++i) {
      float v = volume*vol[i/2];
      pcmMix[i] += pcm[i]*(v*v);
      }
    }

  for(size_t i=0;i<cnt2;++i) {
    float v = pcmMix[i]*volume;
    out[i] = (v < -1.00004566f ? int16_t(-32768) : (v > 1.00001514f ? int16_t(32767) : int16_t(v * 32767.5f)));
    }
  }

void Mixer::volFromCurve(Music::PatternInternal &part,size_t instId,const Music::InsInternal* inst,std::vector<float> &v) {
  float& base = volPerInst[instId];
  for(auto& i:v)
    i=base;

  const int64_t shift = sampleCursor-patStart;
  //const int64_t e = s+v.size();

  for(auto& i:part.volume) {
    if(i.inst!=inst)
      continue;
    int64_t s = toSamples(i.at)-shift;
    int64_t e = toSamples(i.at+i.duration)-shift;
    if(s>v.size() || e<0)
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
          float val = (i-s)/range;
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
          float val = (i-s)/range;
          v[i] = std::pow(val,2.f)*diffV+shift;
          }
        break;
        }
      case DMUS_CURVES_LOG: {
        for(size_t i=begin;i<size;++i) {
          float val = (i-s)/range;
          v[i] = std::sqrt(val)*diffV+shift;
          }
        break;
        }
      case DMUS_CURVES_SINE: {
        for(size_t i=begin;i<size;++i) {
          float linear = (i-s)/range;
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

#include "soundfont.h"

#include <Tempest/Log>
#include <bitset>

#include "dlscollection.h"
#include "hydra.h"
#include "tsf.h"

using namespace Dx8;
using namespace Tempest;

#ifdef TSF_IMPLEMENTATION
// For testing
TSFDEF void __note_on(tsf* f, int preset_index, int key, float vel) {
  short midiVelocity = short(vel * 127);
  struct tsf_region *region, *regionEnd;

  if(preset_index < 0 || preset_index >= f->presetNum)
    return;

  if(vel <= 0.0f) {
    tsf_note_off(f, preset_index, key);
    return;
    }

  // Play all matching regions.
  int voicePlayIndex = f->voicePlayIndex++;
  for(region = f->presets[preset_index].regions, regionEnd = region + f->presets[preset_index].regionNum; region != regionEnd; region++) {
    if(key < region->lokey || key > region->hikey || midiVelocity < region->lovel || midiVelocity > region->hivel)
      continue;

    tsf_voice *voice = nullptr;
    tsf_voice *v = f->voices, *vEnd = v + f->voiceNum;
    if(region->group) {
      for(; v != vEnd; v++)
        if(v->playingPreset == preset_index && v->region->group == region->group)
          tsf_voice_endquick(v, f->outSampleRate);
        else if (v->playingPreset == -1 && !voice)
          voice = v;
      } else {
      for(; v != vEnd; v++)
        if(v->playingPreset==-1) {
          voice = v;
          break;
          }
      }

    if(!voice) {
      f->voiceNum += 4;
      f->voices = (struct tsf_voice*)TSF_REALLOC(f->voices, f->voiceNum * sizeof(struct tsf_voice));
      voice = &f->voices[f->voiceNum - 4];
      voice[1].playingPreset = voice[2].playingPreset = voice[3].playingPreset = -1;
      }

    voice->region = region;
    voice->playingPreset = preset_index;
    voice->playingKey = key;
    voice->playIndex = voicePlayIndex;
    voice->noteGainDB = f->globalGainDB - region->attenuation - tsf_gainToDecibels(1.0f / vel);

    if(f->channels) {
      f->channels->setupVoice(f, voice);
      } else {
      tsf_voice_calcpitchratio(voice, 0, f->outSampleRate);
      // The SFZ spec is silent about the pan curve, but a 3dB pan law seems common. This sqrt() curve matches what Dimension LE does; Alchemy Free seems closer to sin(adjustedPan * pi/2).
      voice->panFactorLeft  = TSF_SQRTF(0.5f - region->pan);
      voice->panFactorRight = TSF_SQRTF(0.5f + region->pan);
      }

    // Offset/end.
    voice->sourceSamplePosition = region->offset;

    // Loop.
    bool doLoop = (region->loop_mode != TSF_LOOPMODE_NONE && region->loop_start < region->loop_end);
    voice->loopStart = (doLoop ? region->loop_start : 0);
    voice->loopEnd   = (doLoop ? region->loop_end   : 0);

    // Setup envelopes.
    tsf_voice_envelope_setup(&voice->ampenv, &region->ampenv, key, midiVelocity, TSF_TRUE, f->outSampleRate);
    tsf_voice_envelope_setup(&voice->modenv, &region->modenv, key, midiVelocity, TSF_FALSE, f->outSampleRate);

    // Setup lowpass filter.
    float filterQDB = region->initialFilterQ / 10.0f;
    voice->lowpass.QInv = 1.0 / TSF_POW(10.0, (filterQDB / 20.0));
    voice->lowpass.z1 = voice->lowpass.z2 = 0;
    voice->lowpass.active = (region->initialFilterFc <= 13500);
    if (voice->lowpass.active) tsf_voice_lowpass_setup(&voice->lowpass, tsf_cents2Hertz((float)region->initialFilterFc) / f->outSampleRate);

    // Setup LFO filters.
    tsf_voice_lfo_setup(&voice->modlfo, region->delayModLFO, region->freqModLFO, f->outSampleRate);
    tsf_voice_lfo_setup(&voice->viblfo, region->delayVibLFO, region->freqVibLFO, f->outSampleRate);
    }
  }

TSFDEF void __note_off(tsf* f, int preset_index, int key) {
  tsf_voice *v = f->voices, *vEnd = v + f->voiceNum, *vMatchFirst = nullptr, *vMatchLast = nullptr;
  for(; v!=vEnd; v++) {
    //Find the first and last entry in the voices list with matching preset, key and look up the smallest play index
    if(v->playingPreset != preset_index || v->playingKey != key || v->ampenv.segment >= TSF_SEGMENT_RELEASE)
      continue;
    if(!vMatchFirst || v->playIndex < vMatchFirst->playIndex)
      vMatchFirst = vMatchLast = v;
    else if(v->playIndex == vMatchFirst->playIndex)
      vMatchLast = v;
    }
  if(!vMatchFirst)
    return;
  int sz = vMatchLast-vMatchFirst;
  if(sz>1)
    Tempest::Log::d();
  for(v = vMatchFirst; v <= vMatchLast; v++) {
    //Stop all voices with matching preset, key and the smallest play index which was enumerated above
    if(v != vMatchFirst && v != vMatchLast &&
       (v->playIndex != vMatchFirst->playIndex || v->playingPreset != preset_index || v->playingKey != key || v->ampenv.segment >= TSF_SEGMENT_RELEASE))
      continue;
    tsf_voice_end(v, f->outSampleRate);
    }
  }

TSFDEF void __render_float(tsf* f, float* buffer, int samples, int flag_mixing)
{
  struct tsf_voice *v = f->voices, *vEnd = v + f->voiceNum;
  if (!flag_mixing) TSF_MEMSET(buffer, 0, (f->outputmode == TSF_MONO ? 1 : 2) * sizeof(float) * samples);
  for (; v != vEnd; v++)
    if (v->playingPreset != -1)
      tsf_voice_render(f, v, buffer, samples);
}
#endif

struct SoundFont::Data {
  Data(const DlsCollection &dls,const std::vector<Wave>& wave)
    :hydra(dls,wave) {
    }

  Dx8::Hydra hydra;
  };

struct SoundFont::Instance {
  Instance(std::shared_ptr<Data> &shData,uint32_t dwPatch){
    uint8_t bankHi = uint8_t((dwPatch & 0x00FF0000) >> 0x10);
    uint8_t bankLo = uint8_t((dwPatch & 0x0000FF00) >> 0x8);
    uint8_t patch  = uint8_t(dwPatch & 0x000000FF);
    int32_t bank   = (bankHi << 16) + bankLo;

    fnt    = shData->hydra.toTsf();
    preset = tsf_get_presetindex(fnt, bank, patch);
    tsf_set_output(fnt,TSF_STEREO_INTERLEAVED,44100,0);
    }

  ~Instance(){
    Hydra::finalize(fnt);
    }

  bool hasNotes() {
    return Hydra::hasNotes(fnt);
    }

  void setPan(float p){
    tsf_channel_set_pan(fnt,0,p);
    tsf_channel_set_pan(fnt,1,p);
    }

  bool noteOn(uint8_t note, uint8_t velosity){
    if(alloc[note]) {
      return false;
      }
    alloc[note]=true;
    tsf_note_on(fnt,preset,note,velosity/127.f);
    return true;
    }

  bool noteOff(uint8_t note){
    if(!alloc[note])
      return false;
    alloc[note]=false;
    tsf_note_off(fnt,preset,note);
    return true;
    }

  std::bitset<256> alloc;
  tsf*             fnt=nullptr;
  int              preset=0;
  };

struct SoundFont::Impl {
  Impl(std::shared_ptr<Data> &shData,uint32_t dwPatch)
    :shData(shData), dwPatch(dwPatch) {
    }

  ~Impl() {
    }

  void setPan(float p){
    for(auto& i:inst)
      i->setPan(p);
    pan = p;
    }

  std::shared_ptr<Instance> noteOn(uint8_t note, uint8_t velosity){
    for(auto& i:inst){
      if(i->noteOn(note,velosity))
        return i;
      }
    auto fnt = std::make_shared<Instance>(shData,dwPatch);
    fnt->setPan(pan);
    fnt->noteOn(note,velosity);
    inst.emplace_back(fnt);
    return inst.back();
    }

  /*
  void noteOff(uint8_t note){
    for(auto& i:inst){
      if(i->noteOff(note))
        return;
      }
    }*/

  bool hasNotes() {
    for(auto& i:inst)
      if(i->hasNotes())
        return true;
    return false;
    }

  void mix(float *samples, size_t count) {
    for(auto& i:inst)
      tsf_render_float(i->fnt,samples,int(count),true);
    }

  std::shared_ptr<Data>                  shData;
  uint32_t                               dwPatch=0;
  float                                  pan=0.5f;
  std::vector<std::shared_ptr<Instance>> inst;
  };

SoundFont::SoundFont() {
  }

SoundFont::SoundFont(std::shared_ptr<Data> &sh, uint32_t dwPatch) {
  impl = std::shared_ptr<Impl>(new Impl(sh,dwPatch));
  }

SoundFont::~SoundFont() {
  }

std::shared_ptr<SoundFont::Data> SoundFont::shared(const DlsCollection &dls,const std::vector<Wave>& wave) {
  return std::shared_ptr<Data>(new Data(dls,wave));
  }

bool SoundFont::hasNotes() const {
  if(impl==nullptr)
    return false;
  return impl->hasNotes();
  }

void SoundFont::setVolume(float /*v*/) {
   // handled in mixer
  }

void SoundFont::setPan(float p) {
  if(impl==nullptr)
    return;
  impl->setPan(p);
  }

void SoundFont::mix(float *samples, size_t count) {
  if(impl==nullptr)
    return;
  impl->mix(samples,count);
  }

SoundFont::Ticket SoundFont::noteOn(uint8_t note, uint8_t velosity) {
  Ticket t;
  if(impl==nullptr)
    return t;
  t.impl = impl->noteOn(note,velosity);
  t.note = note;
  return t;
  }

void SoundFont::noteOff(SoundFont::Ticket &t) {
  if(t.impl==nullptr)
    return;
  auto& i = *t.impl;
  i.noteOff(t.note);
  }


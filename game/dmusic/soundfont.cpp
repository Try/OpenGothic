#include "soundfont.h"

#include <Tempest/Log>
#include <array>
#include <bitset>

#include "dlscollection.h"
#include "dxsynth.h"
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

// ── Global backend selection ─────────────────────────────────────────────────
static SoundFont::Backend gActiveBackend = SoundFont::Backend::DxSynth;

void SoundFont::setBackend(Backend b) { gActiveBackend = b; }
SoundFont::Backend SoundFont::activeBackend() { return gActiveBackend; }

// ─────────────────────────────────────────────────────────────────────────────
struct SoundFont::Data {
  Data(const DlsCollection &dls, const std::vector<Wave>& wave) {
    const Backend backend = gActiveBackend;

    // ── DxSynth path — pure DLS synthesis, no Hydra/SF2 ──────────────────
    // Build WaveBank first (while wave data is available; it will be cleared
    // by DlsCollection after this constructor returns if keepWaveData=false).
    // Copy instruments by value so compileInstr can run safely later even
    // after the originating DlsCollection has been moved or destroyed.
    if(backend == Backend::DxSynth) {
      dxWaveBank    = DxSynth::WaveBank::build(wave);
      dxInstruments = dls.instrument;   // safe value copy — no dangling pointer
      return;
      }

    // ── Legacy path: build Hydra (DLS → SF2 in-memory → TinySoundFont) ──
    hydra = std::make_unique<Dx8::Hydra>(dls, wave);
    }

  // DxSynth (primary)
  // dxInstruments is a value-copy of DlsCollection::instrument captured at
  // construction time, so it remains valid regardless of DlsCollection lifetime.
  std::shared_ptr<DxSynth::WaveBank>     dxWaveBank;
  std::vector<DlsCollection::Instrument> dxInstruments;

  // Legacy (Hydra → TinySoundFont)
  std::unique_ptr<Dx8::Hydra>        hydra;
  };

struct SoundFont::Instance {
  static int32_t resolvePresetIndex(tsf* fnt,
                                    uint8_t bankHi,
                                    uint8_t bankLo,
                                    uint8_t patch,
                                    bool preferDrumKit) {
    if(fnt==nullptr)
      return -1;

    const int32_t bankLegacy = (int32_t(bankHi) << 16) + int32_t(bankLo);
    std::array<int32_t, 8> banksToTry = {};
    size_t banksToTryCount = 0;
    banksToTry[banksToTryCount++] = bankLegacy;
    banksToTry[banksToTryCount++] = int32_t(uint16_t(bankLegacy));
    banksToTry[banksToTryCount++] = (int32_t(bankHi) << 8) + int32_t(bankLo);
    banksToTry[banksToTryCount++] = (int32_t(bankHi) << 7) + int32_t(bankLo);
    banksToTry[banksToTryCount++] = int32_t(bankLo);

    // DLS drum kits are exported to TSF bank 128; try that before the generic bank 0 fallback.
    if(preferDrumKit) {
      banksToTry[banksToTryCount++] = 128 | (int32_t(bankLegacy) & 0x7FFF);
      banksToTry[banksToTryCount++] = 128;
      }
    banksToTry[banksToTryCount++] = 0;

    const std::array<int32_t, 2> presetToTry = {
      int32_t(patch),
      int32_t(patch & 0x7F)
      };

    for(size_t p=0; p<presetToTry.size(); ++p) {
      bool presetVisited = false;
      for(size_t pPrev=0; pPrev<p; ++pPrev) {
        if(presetToTry[pPrev]==presetToTry[p]) {
          presetVisited = true;
          break;
          }
        }
      if(presetVisited)
        continue;

      for(size_t b=0; b<banksToTryCount; ++b) {
        bool bankVisited = false;
        for(size_t bPrev=0; bPrev<b; ++bPrev) {
          if(banksToTry[bPrev]==banksToTry[b]) {
            bankVisited = true;
            break;
            }
          }
        if(bankVisited)
          continue;

        const int32_t presetIndex = Hydra::presetIndex(fnt, banksToTry[b], presetToTry[p]);
        if(presetIndex>=0)
          return presetIndex;
        }
      }

    const int32_t presetCount = tsf_get_presetcount(fnt);
    return presetCount>0 ? 0 : -1;
    }

  Instance(const std::shared_ptr<Data> &shData, uint32_t dwPatch, bool preferDrumKit){
    uint8_t bankHi = uint8_t((dwPatch & 0x00FF0000) >> 0x10);
    uint8_t bankLo = uint8_t((dwPatch & 0x0000FF00) >> 0x8);
    uint8_t patch  = uint8_t(dwPatch & 0x000000FF);
    const bool patchDrumKit = (dwPatch & 0x80000000u)!=0u;

    // ── DxSynth (primary backend — authentic NT5 dmsynth) ───────────────────
    // dxWaveBank is non-null and dxInstruments is non-empty when Data was built
    // in DxSynth mode.  No Hydra/tsf/Fluid objects were created, so we MUST NOT
    // fall through to the legacy paths below.
    if(shData->dxWaveBank && !shData->dxInstruments.empty()) {
      try {
        DxSynth::CompiledInstr instr = DxSynth::compileInstr(
            shData->dxInstruments, dwPatch, shData->dxWaveBank);
        if(!instr.regions.empty())
          dxSynth = std::make_unique<DxSynth::Synth>(shData->dxWaveBank, std::move(instr), SoundFont::SampleRate);
        // else: silent instrument (no matching patch) — dxSynth stays null,
        //       all operations below become no-ops via the null guards in each method.
        }
      catch(const std::exception& e) {
        Log::e("DxSynth compile failed: ", e.what());
        }
      return; // never fall through — Hydra was NOT created in DxSynth mode
      }

    if(shData->hydra) {
      fnt    = shData->hydra->toTsf();
      preset = resolvePresetIndex(fnt, bankHi, bankLo, patch, preferDrumKit || patchDrumKit);
      Hydra::setOutput(fnt, 44100, 0);
      }
    }

  ~Instance(){
    if(fnt!=nullptr)
      Hydra::finalize(fnt);
    }

  bool hasNotes() {
    if(dxSynth)
      return dxSynth->hasNotes();
    if(fnt==nullptr) return false;
    return Hydra::hasNotes(fnt);
    }

  void setPan(float p){
    if(dxSynth) {
      dxSynth->setPan(p); // 0..1 passed through; Synth::setPan converts internally
      return;
      }
    if(fnt==nullptr) return;
    Hydra::channelSetPan(fnt,0,p);
    Hydra::channelSetPan(fnt,1,p);
    }

  void setCC(int cc, int val) {
    if(dxSynth) {
      // DxSynth: handle sustain pedal (CC64)
      if(cc == 64 && val >= 64) {
        // Sustain on — future: latch notes
        }
      return;
      }
    // TSF: no CC support; silently ignore
    (void)cc; (void)val;
    }

  void setPitchBendNormalized(float n) {
    if(dxSynth) {
      dxSynth->setPitchBendNormalized(n);
      return;
      }
    // TSF backend: no pitch-bend API, silently ignore.
    (void)n;
    }

  bool noteOn(uint8_t note, uint8_t velosity){
    const uint8_t midiVelocity = (velosity>127 ? 127 : velosity);
    if(midiVelocity==0)
      return false;

    if(dxSynth) {
      if(alloc[note])
        return false;
      alloc[note] = true;
      dxSynth->noteOn(note, midiVelocity);
      return true;
      }

    if(fnt==nullptr || preset<0)
      return false;
    if(alloc[note])
      return false;

    alloc[note]=true;
    Hydra::noteOn(fnt,preset,note,float(midiVelocity)/127.f);
    return true;
    }

  bool noteOff(uint8_t note){
    if(!alloc[note])
      return false;
    alloc[note]=false;

    if(dxSynth) {
      dxSynth->noteOff(note);
      return true;
      }

    if(fnt==nullptr || preset<0)
      return false;
    Hydra::noteOff(fnt, preset, note);
    return true;
    }

  std::bitset<256>                  alloc;
  std::unique_ptr<DxSynth::Synth>   dxSynth;   // primary: authentic NT5 dmsynth
  tsf*                              fnt=nullptr;
  int                               preset=0;
  };

struct SoundFont::Impl {
  Impl(const std::shared_ptr<Data> &shData, uint32_t dwPatch, bool preferDrumKit)
    :shData(shData), dwPatch(dwPatch), preferDrumKit(preferDrumKit) {
    }

  ~Impl() {
    }

  void setPan(float p){
    for(auto& i:inst)
      i->setPan(p);
    pan = p;
    }

  void setCC(int cc, int val) {
    for(auto& i:inst)
      i->setCC(cc,val);
    }

  void setPitchBendNormalized(float n) {
    for(auto& i:inst)
      i->setPitchBendNormalized(n);
    pitchBend = n;
    }

  std::shared_ptr<Instance> noteOn(uint8_t note, uint8_t velosity){
    for(auto& i:inst){
      if(i->noteOn(note,velosity))
        return i;
      }
    auto fnt = std::make_shared<Instance>(shData, dwPatch, preferDrumKit);
    fnt->setPan(pan);
    fnt->setPitchBendNormalized(pitchBend);
    if(!fnt->noteOn(note,velosity))
      return std::shared_ptr<Instance>();
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
    for(auto& i:inst) {
      // DxSynth (primary backend)
      if(i->dxSynth) {
        i->dxSynth->render(samples, int(count));
        continue;
        }
      Hydra::renderFloat(i->fnt,samples,int(count),true);
      }
    }

  std::shared_ptr<Data>                  shData;
  uint32_t                               dwPatch=0;
  bool                                   preferDrumKit=false;
  float                                  pan=0.5f;
  float                                  pitchBend=0.f; // -1..+1 latched state
  std::vector<std::shared_ptr<Instance>> inst;
  };

SoundFont::SoundFont() {
  }

SoundFont::SoundFont(const std::shared_ptr<Data> &sh, uint32_t dwPatch, bool preferDrumKit) {
  impl = std::shared_ptr<Impl>(new Impl(sh, dwPatch, preferDrumKit));
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

void SoundFont::setCC(int cc, int val) {
  if(impl==nullptr)
    return;
  impl->setCC(cc, val);
  }

void SoundFont::setPitchBendNormalized(float n) {
  if(impl==nullptr)
    return;
  impl->setPitchBendNormalized(n);
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


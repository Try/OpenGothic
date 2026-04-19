#include "hydra.h"

#include "dlscollection.h"

#include <cmath>

#define TSF_IMPLEMENTATION
#define TSF_STATIC

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wunused-function"
#include "tsf.h"
#pragma GCC diagnostic pop
#else
#include "tsf.h"
#endif

#include <atomic>

using namespace Dx8;

enum {
  kTerminatorSampleLength=46
  };

static constexpr uint32_t kDlsDrumKitFlag = 0x80000000u;
static constexpr uint16_t kModSrcNone = 0;
static constexpr uint16_t kModSrcNoteOnVelocity = 2;
static constexpr uint16_t kModSrcNoteOnKey = 3;
static constexpr uint16_t kModTransformLinear = 0;
static std::atomic<int> gLoopModeOverride{Hydra::LoopModeAuto};

static bool compValue(const tsf_hydra_phdr& a,const tsf_hydra_phdr& b){
  return
      std::memcmp(a.presetName,b.presetName,20)==0 &&
      a.preset      ==b.preset &&
      a.bank        ==b.bank &&
      a.presetBagNdx==b.presetBagNdx &&
      a.library     ==b.library &&
      a.genre       ==b.genre &&
      a.morphology  ==b.morphology;
  }

static bool compValue(const tsf_hydra_shdr& a,const tsf_hydra_shdr& b){
  return
      std::memcmp(a.sampleName,b.sampleName,20)==0 &&
      //a.start          ==b.start &&
      //a.end            ==b.end &&
      //a.startLoop      ==b.startLoop &&
      //a.endLoop        ==b.endLoop &&
      a.sampleRate     ==b.sampleRate &&
      a.originalPitch  ==b.originalPitch &&
      a.pitchCorrection==b.pitchCorrection &&
      a.sampleLink     ==b.sampleLink &&
      a.sampleType     ==b.sampleType;
  }

static bool compValue(const tsf_hydra_pbag& a,const tsf_hydra_pbag& b){
  return
      a.genNdx==b.genNdx &&
      a.modNdx==b.modNdx;
  }

static bool compValue(const tsf_hydra_pmod& a,const tsf_hydra_pmod& b){
  return
      a.modSrcOper   ==b.modSrcOper &&
      a.modDestOper  ==b.modDestOper &&
      a.modAmount    ==b.modAmount &&
      a.modAmtSrcOper==b.modAmtSrcOper &&
      a.modTransOper ==b.modTransOper;
  }

static bool compValue(const tsf_hydra_pgen& a,const tsf_hydra_pgen& b){
  return std::memcmp(&a,&b,sizeof(a))==0;
  }

static bool compValue(const tsf_hydra_inst& a,const tsf_hydra_inst& b){
  return std::memcmp(&a,&b,sizeof(a))==0;
  }

static bool compValue(const tsf_hydra_ibag& a,const tsf_hydra_ibag& b){
  return std::memcmp(&a,&b,sizeof(a))==0;
  }

static bool compValue(const tsf_hydra_imod& a,const tsf_hydra_imod& b){
  return std::memcmp(&a,&b,sizeof(a))==0;
  }

static bool compValue(const tsf_hydra_igen& a,const tsf_hydra_igen& b){
  return std::memcmp(&a,&b,sizeof(a))==0;
  }

static int32_t clampI32(int32_t v, int32_t lo, int32_t hi) {
  if(v<lo)
    return lo;
  if(v>hi)
    return hi;
  return v;
  }

static uint16_t toPresetBank(const uint32_t dlsBank) {
  if((dlsBank & kDlsDrumKitFlag)!=0u)
    return 128u;

  const uint16_t bankMsb = uint16_t((dlsBank >> 16) & 0xFFu);
  const uint16_t bankLsb = uint16_t(dlsBank & 0xFFu);
  uint16_t bank = uint16_t((bankMsb << 8) | bankLsb);

  // Compatibility path for files that encode the bank byte at bits 8..15.
  if(bank==0u && (dlsBank & 0x0000FF00u)!=0u)
    bank = uint16_t((dlsBank >> 8) & 0xFFu);
  return bank;
  }

template<class T>
static bool comp(const T* a,const T* b,int n,size_t vn){
  if(size_t(n)!=vn)
    return false;
  for(int i=0;i<n;++i){
    if(!compValue(a[i],b[i]))
      return false;
    }
  return true;
  }

static tsf_hydra_phdr mkPhdr(const DlsCollection::Instrument& instr,uint16_t index){
  tsf_hydra_phdr r={};
  std::strncpy(r.presetName,instr.info.inam.c_str(),19);
  r.preset       = uint16_t(instr.header.Locale.ulInstrument & 0xFFu);
  r.bank         = toPresetBank(instr.header.Locale.ulBank);
  r.presetBagNdx = index;
  r.library      = 0;
  r.genre        = 0;
  r.morphology   = 0;
  return r;
  }

static tsf_hydra_phdr mkPhdr(const char* presetName,uint16_t index){
  tsf_hydra_phdr r = {};
  std::strncpy(r.presetName,presetName,19);
  r.presetBagNdx = index;
  return r;
  }

static tsf_hydra_pbag mkPbag(uint16_t genNdx,uint16_t modNdx){
  tsf_hydra_pbag r={};
  r.genNdx = genNdx;
  r.modNdx = modNdx;
  return r;
  }

static tsf_hydra_pgen mkPgen(uint16_t genOper,uint16_t wordAmount){
  tsf_hydra_pgen r={};
  r.genOper              = genOper;
  r.genAmount.wordAmount = wordAmount;
  return r;
  }

static tsf_hydra_inst mkInst(const char* instName,uint16_t instBagNdx){
  tsf_hydra_inst r={};
  std::strncpy(r.instName,instName,19);
  r.instBagNdx=instBagNdx;
  return r;
  }

static tsf_hydra_ibag mkIbag(uint16_t instGenNdx,uint16_t instModNdx){
  tsf_hydra_ibag r={};
  r.instGenNdx = instGenNdx;
  r.instModNdx = instModNdx;
  return r;
  }

struct tsf_hydra_igen mkIgen(uint16_t op,uint16_t lo,uint16_t hi) {
  if(lo>255 || hi>255)
    throw std::runtime_error("hydra.igen range error");
  tsf_hydra_igen r={};
  r.genOper            = op;
  r.genAmount.range.lo = uint8_t(lo);
  r.genAmount.range.hi = uint8_t(hi);
  return r;
  }

struct tsf_hydra_igen mkIgen(uint16_t op,uint16_t u16) {
  tsf_hydra_igen r={};
  r.genOper              = op;
  r.genAmount.wordAmount = u16;
  return r;
  }

static tsf_hydra_imod mkImod(uint16_t src, uint16_t dest, int16_t amount, uint16_t amountSrc, uint16_t transform) {
  tsf_hydra_imod r={};
  r.modSrcOper    = src;
  r.modDestOper   = dest;
  r.modAmount     = amount;
  r.modAmtSrcOper = amountSrc;
  r.modTransOper  = transform;
  return r;
  }

bool Hydra::mkGeneratorOp(uint16_t usSource, uint16_t usControl, uint16_t usDestination, uint16_t usTransform, uint16_t& out) {
  if(usControl!=0)
    return false;
  (void)usTransform;

  switch(usSource) {
    case SourceLFO:
      switch(usDestination) {
        case Pitch:
          out = kModLfoToPitch;
          return true;
        case Attenuation:
          out = kModLfoToVolume;
          return true;
        case FilterCutoff:
          out = kModLfoToFilterFc;
          return true;
        default:
          return false;
        }
    case SourceEG2:
      switch(usDestination) {
        case Pitch:
          out = kModEnvToPitch;
          return true;
        case FilterCutoff:
          out = kModEnvToFilterFc;
          return true;
        default:
          return false;
        }
    case SourceKeyNumber:
      switch(usDestination) {
        case Pitch:
          out = kScaleTuning;
          return true;
        case EG1HoldTime:
          out = kKeynumToVolEnvHold;
          return true;
        case EG1DecayTime:
          out = kKeynumToVolEnvDecay;
          return true;
        case EG2HoldTime:
          out = kKeynumToModEnvHold;
          return true;
        case EG2DecayTime:
          out = kKeynumToModEnvDecay;
          return true;
        default:
          return false;
        }
    case SourceVibrato:
      if(usDestination==Pitch) {
        out = kVibLfoToPitch;
        return true;
        }
      return false;
    case SourceNone:
      switch(usDestination) {
        case Pitch:
          return false;
        case Attenuation:
          out = kInitialAttenuation;
          return true;
        case Pan:
          out = kPan;
          return true;
        case EG1AttachTime:
          out = kAttackVolEnv;
          return true;
        case EG2AttachTime:
          out = kAttackModEnv;
          return true;
        case EG1DecayTime:
          out = kDecayVolEnv;
          return true;
        case EG2DecayTime:
          out = kDecayModEnv;
          return true;
        case EG1ReleaseTime:
          out = kReleaseVolEnv;
          return true;
        case EG2ReleaseTime:
          out = kReleaseModEnv;
          return true;
        case EG1DelayTime:
          out = kDelayVolEnv;
          return true;
        case EG2DelayTime:
          out = kDelayModEnv;
          return true;
        case EG1HoldTime:
          out = kHoldVolEnv;
          return true;
        case EG2HoldTime:
          out = kHoldModEnv;
          return true;
        case EG1SustainLevel:
          out = kSustainVolEnv;
          return true;
        case EG2SustainLevel:
          out = kSustainModEnv;
          return true;
        case FilterCutoff:
          out = kInitialFilterFc;
          return true;
        case FilterQ:
          out = kInitialFilterQ;
          return true;
        case LFOFrequency:
          out = kFreqModLFO;
          return true;
        case LFOStartDelay:
          out = kDelayModLFO;
          return true;
        case VibFrequency:
          out = kFreqVibLFO;
          return true;
        case VibStartDelay:
          out = kDelayVibLFO;
          return true;
        default:
          return false;
        }
    default:
      return false;
    }
  }

Hydra::Hydra(const DlsCollection &dls,const std::vector<Wave>& wave) {
  std::vector<tsf_hydra_shdr> samples;
  wdata = allocSamples(wave,samples,wdataSize);

  const uint16_t modIndex     = 0;
  uint16_t       instBagIndex = 0;

  for(size_t idInstr=0; idInstr<dls.instrument.size(); ++idInstr) {
    const auto& instr = dls.instrument[idInstr];

    phdr.push_back(mkPhdr(instr,uint16_t(phdr.size())));
    pbag.push_back(mkPbag(uint16_t(pbag.size()),modIndex));

    pgen.push_back(mkPgen(41,uint16_t(idInstr)));
    inst.push_back(mkInst(instr.info.inam.c_str(),instBagIndex));

    instBagIndex = uint16_t(instBagIndex+instr.regions.size());

    for(const auto& reg : instr.regions) {
      const auto& hdr        = reg.head;
      const auto& wavelink   = reg.wlink;
      if(wavelink.ulTableIndex>=samples.size())
        throw std::runtime_error("Invalid wave table index in instrument");

      const Wave* waveRef = (wavelink.ulTableIndex<wave.size()) ? &wave[wavelink.ulTableIndex] : nullptr;

      uint16_t unityNote = reg.waveSample.usUnityNote;
      int16_t  fineTune = reg.waveSample.sFineTune;
      int32_t  attenuation = reg.waveSample.lAttenuation;
      if(reg.waveSample.cbSize==0 && waveRef!=nullptr && waveRef->waveSample.cbSize!=0) {
        unityNote = waveRef->waveSample.usUnityNote;
        fineTune = waveRef->waveSample.sFineTune;
        attenuation = waveRef->waveSample.lAttenuation;
        }

      ibag.push_back(mkIbag(uint16_t(igen.size()),uint16_t(imod.size())));

      uint16_t keyrangeLow  = hdr.RangeKey.usLow;
      uint16_t keyrangeHigh = hdr.RangeKey.usHigh;
      uint16_t velrangeLow  = 0;
      uint16_t velrangeHigh = 0;
      // Preserve exact velocity splits/layers. The old <= fallback turned
      // fixed layers (low==high) into full-range 0..127 and caused stacked
      // regions, louder attacks and overly long tails.
      const bool invalidVelRange = hdr.RangeVelocity.usHigh < hdr.RangeVelocity.usLow;
      const bool implicitFullVelRange = (hdr.RangeVelocity.usLow==0 && hdr.RangeVelocity.usHigh==0);
      if(invalidVelRange || implicitFullVelRange) {
        velrangeLow  = 0;
        velrangeHigh = 127;
        } else {
        velrangeLow  = hdr.RangeVelocity.usLow;
        velrangeHigh = hdr.RangeVelocity.usHigh;
        }
      igen.push_back(mkIgen(kKeyRange,keyrangeLow,keyrangeHigh));
      igen.push_back(mkIgen(kVelRange,velrangeLow,velrangeHigh));

      if(hdr.usKeyGroup!=0)
        igen.push_back(mkIgen(kExclusiveClass,hdr.usKeyGroup));

      const int32_t initialAttenuation = clampI32(-(attenuation >> 16),0,65535);
      igen.push_back(mkIgen(kInitialAttenuation,uint16_t(initialAttenuation)));

      auto appendGenerator = [this](const DlsCollection::ConnectionBlock& c) {
        if(c.usControl==0 && c.usSource==SourceNone && c.usDestination==Pitch) {
          // TinySoundFont backend does not apply modulators from hydra (imod/pmod are ignored),
          // so static pitch offsets must be baked as generators.
          int32_t cents = -(c.lScale >> 16);
          cents = clampI32(cents,-32768,32767);

          int32_t coarse = cents / 100;
          int32_t fine   = cents - coarse*100;
          coarse = clampI32(coarse,-32768,32767);
          fine   = clampI32(fine,-32768,32767);

          if(coarse!=0)
            igen.push_back(mkIgen(kCoarseTune,uint16_t(int16_t(coarse))));
          if(fine!=0)
            igen.push_back(mkIgen(kFineTune,uint16_t(int16_t(fine))));
          return;
          }

        uint16_t gen = 0;
        if(!mkGeneratorOp(c.usSource,c.usControl,c.usDestination,c.usTransform,gen))
          return;

        int32_t amount = (c.lScale >> 16);
        if(c.usSource==SourceLFO && c.usDestination==Attenuation)
          amount = clampI32(amount,0,120);
        if((c.usSource==SourceLFO && c.usDestination==Pitch) ||
           (c.usSource==SourceEG2 && c.usDestination==Pitch) ||
           (c.usSource==SourceVibrato && c.usDestination==Pitch))
          amount = clampI32(amount,-1200,1200);
        if(c.usSource==SourceKeyNumber && c.usDestination==Pitch)
          amount = (c.lScale/12800)*100;
        if(c.usSource==SourceKeyNumber &&
           (c.usDestination==EG1HoldTime || c.usDestination==EG1DecayTime ||
            c.usDestination==EG2HoldTime || c.usDestination==EG2DecayTime))
          amount = clampI32(amount,-1200,1200);
        if(c.usSource==SourceNone && c.usDestination==EG1SustainLevel) {
          // DLS EG1 sustain is linear 0-1000 (1000=max sustain).
          // SF2 kSustainVolEnv is centibels (0=full sustain, 1440=silence).
          // Convert: centibels = -200 * log10(level / 1000.0)
          int32_t level = clampI32(amount, 0, 1000);
          if(level <= 0)
            amount = 1440;
          else
            amount = clampI32(int32_t(-200.0 * std::log10(double(level) / 1000.0)), 0, 1440);
          }
        else if(c.usSource==SourceNone && c.usDestination==EG2SustainLevel)
          amount = 1000 - clampI32(amount,0,1000);

        amount = clampI32(amount,-32768,32767);

        const int16_t signedAmount = int16_t(amount);
        igen.push_back(mkIgen(gen,uint16_t(signedAmount)));
        };

      auto appendModulator = [this](const DlsCollection::ConnectionBlock& c) {
        if(c.usControl!=0)
          return;

        int32_t amount = clampI32((c.lScale >> 16),-32768,32767);

        if(c.usSource==SourceNone && c.usDestination==Pitch) {
          // Handled in appendGenerator because imod/pmod are ignored by TinySoundFont parser.
          return;
          }

        if(c.usSource==SourceKeyOnVelocity && c.usDestination==EG1AttachTime) {
          imod.push_back(mkImod(kModSrcNoteOnVelocity,kAttackVolEnv,int16_t(amount),kModSrcNone,kModTransformLinear));
          return;
          }

        if(c.usSource==SourceKeyOnVelocity && c.usDestination==EG2AttachTime) {
          imod.push_back(mkImod(kModSrcNoteOnVelocity,kAttackModEnv,int16_t(amount),kModSrcNone,kModTransformLinear));
          return;
          }

        };

      if(reg.articulationConnections.size()>0) {
        for(const auto& c : reg.articulationConnections) {
          appendGenerator(c);
          appendModulator(c);
          }
        } else {
        for(const auto& art : instr.articulators)
          for(const auto& c : art.connectionBlocks) {
            appendGenerator(c);
            appendModulator(c);
            }
        }

      if(hdr.RangeKey.usHigh < hdr.RangeKey.usLow)
        throw std::runtime_error("Invalid key range in instrument");

      tsf_hydra_shdr sample={};
      uint16_t sampleMode = 0;
      sample = samples[wavelink.ulTableIndex];

      bool hasLoop = false;
      uint32_t loopType = 0;
      uint32_t loopStart = 0;
      uint32_t loopLength = 0;
      if(!reg.loop.empty()) {
        const auto& loop = reg.loop[0];
        hasLoop = true;
        loopType = loop.ulLoopType;
        loopStart = loop.ulLoopStart;
        loopLength = loop.ulLoopLength;
        } else if(waveRef!=nullptr && !waveRef->loop.empty()) {
        const auto& loop = waveRef->loop[0];
        hasLoop = true;
        loopType = loop.ulLoopType;
        loopStart = loop.ulLoopStart;
        loopLength = loop.ulLoopLength;
        }

      if(!hasLoop) {
        sample.startLoop = 0;
        sample.endLoop   = 0;
        } else {
        sample.startLoop = loopStart;
        sample.endLoop   = loopStart + loopLength;

        sample.startLoop += sample.start;
        sample.endLoop   += sample.start;
        // Keep auto behavior aligned with OpenGothic/dmusic hydra conversion:
        // ulLoopType==1 means sustain-style loop (TSF sampleMode=3), otherwise continuous (1).
        const bool loopWithRelease = (loopType==1u);
        const int loopOverride = Hydra::getLoopModeOverride();
        // TinySoundFont mapping: 1=continuous loop, 3=sustain loop.
        if(loopOverride==Hydra::LoopModeForceSustain)
          sampleMode = 3u;
        else if(loopOverride==Hydra::LoopModeForceRelease)
          sampleMode = 1u;
        else
          sampleMode = loopWithRelease ? 3u : 1u;
        }
      igen.push_back(mkIgen(kSampleModes,sampleMode));

      igen.push_back(mkIgen(kSampleID,uint16_t(shdr.size())));

      sample.originalPitch   = uint8_t(unityNote);
      sample.pitchCorrection = int8_t (fineTune);
      shdr.push_back(sample);
      }
    }

  phdr.push_back(mkPhdr("EOP",uint16_t(phdr.size())));
  pbag.push_back(mkPbag(uint16_t(pbag.size()),modIndex));

  tsf_hydra_pmod mod={};
  pmod.push_back(mod);
  pgen.push_back(mkPgen(0,0));

  inst.push_back(mkInst("EOI",instBagIndex));
  ibag.push_back(mkIbag(uint16_t(igen.size()),uint16_t(imod.size())));

  tsf_hydra_imod imd={};
  imod.push_back(imd);

  tsf_hydra_igen gen={};
  igen.push_back(gen);

  tsf_hydra_shdr sampleEnd={};
  std::strncpy(sampleEnd.sampleName,"EOS",19);
  shdr.push_back(sampleEnd);
  }

Hydra::~Hydra() {
  }

void Hydra::finalize(tsf *tsf) {
  tsf->fontSamples=nullptr;
  tsf_close(tsf);
  }

void Hydra::getBankPreset(tsf* f, int presetIdx, int& bank, int& preset) {
  bank   = 0;
  preset = 0;
  if(f && presetIdx >= 0 && presetIdx < tsf_get_presetcount(f)) {
    bank   = int(f->presets[presetIdx].bank);
    preset = int(f->presets[presetIdx].preset);
    }
  }

bool Hydra::hasNotes(tsf *tsf) {
  tsf_voice *v = tsf->voices, *vEnd = v + tsf->voiceNum;
  for(; v!=vEnd; v++)
    if(v->playingPreset != -1)
      return true;
  return false;
  }

void Hydra::noteOn(tsf* f, int presetId, int key, float vel) {
  tsf_note_on(f, presetId, key, vel);
  }

void Hydra::noteOff(tsf* f, int presetId, int key) {
  tsf_note_off(f, presetId, key);
  }

int  Hydra::presetIndex(const tsf* f, int bank, int presetNum) {
  return tsf_get_presetindex(f, bank, presetNum);
  }

void Hydra::renderFloat(tsf* f, float* buffer, int samples, int flag) {
  tsf_render_float(f, buffer, samples, flag);
  }

int  Hydra::channelSetPan(tsf* f, int channel, float pan) {
  return tsf_channel_set_pan(f, channel, pan);
  }

void Hydra::setOutput(tsf* f, int samplerate, float gain) {
  tsf_set_output(f, TSF_STEREO_INTERLEAVED, samplerate, gain);
  }

void Hydra::setLoopModeOverride(int mode) {
  if(mode<int(LoopModeAuto))
    mode = int(LoopModeAuto);
  if(mode>int(LoopModeForceRelease))
    mode = int(LoopModeForceRelease);
  gLoopModeOverride.store(mode,std::memory_order_relaxed);
  }

int Hydra::getLoopModeOverride() {
  return gLoopModeOverride.load(std::memory_order_relaxed);
  }

tsf *Hydra::toTsf() {
  tsf_hydra hydra={};
  toTsf(hydra);

  tsf* res = reinterpret_cast<tsf*>(TSF_MALLOC(sizeof(tsf)));
  TSF_MEMSET(res, 0, sizeof(tsf));
  res->outSampleRate = 44100.0f;
  res->presetNum     = hydra.phdrNum - 1;
  res->presets       = reinterpret_cast<tsf_preset*>(TSF_MALLOC(size_t(res->presetNum)*sizeof(tsf_preset)));

  res->fontSamples   = wdata.get();
  tsf_load_presets(res, &hydra, unsigned(wdataSize));
  return res;
  }

void Hydra::toTsf(tsf_hydra &out) {
  out.phdrNum = int(phdr.size());
  out.phdrs   = phdr.data();

  out.pbagNum = int(pbag.size());
  out.pbags   = pbag.data();

  out.pmodNum = int(pmod.size());
  out.pmods   = pmod.data();

  out.pgenNum = int(pgen.size());
  out.pgens   = pgen.data();

  out.instNum = int(inst.size());
  out.insts   = inst.data();

  out.ibagNum = int(ibag.size());
  out.ibags   = ibag.data();

  out.imodNum = int(imod.size());
  out.imods   = imod.data();

  out.igenNum = int(igen.size());
  out.igens   = igen.data();

  out.shdrNum = int(shdr.size());
  out.shdrs   = shdr.data();
  }

std::unique_ptr<float[]> Hydra::allocSamples(const std::vector<Wave>& wave,std::vector<tsf_hydra_shdr>& smp,size_t& count) {
  size_t wavStart=0;
  for(const auto& wav : wave) {
    if(wav.wfmt.wBitsPerSample!=16)
      throw std::runtime_error("Unexpected DLS sample format");
    const size_t wavSize = wav.wavedata.size()/(sizeof(int16_t));

    tsf_hydra_shdr sx={};
    std::strncpy(sx.sampleName,wav.info.inam.c_str(),19);
    sx.start           = tsf_u32(wavStart);
    sx.end             = tsf_u32(wavStart+wavSize);
    sx.startLoop       = 0; // will be overridden, later
    sx.endLoop         = 0;
    sx.sampleRate      = wav.wfmt.dwSamplesPerSec;
    sx.originalPitch   = uint8_t(wav.waveSample.usUnityNote);
    sx.pitchCorrection = int8_t (wav.waveSample.sFineTune);
    sx.sampleLink      = 0;
    sx.sampleType      = 1; // mono
    smp.push_back(sx);

    wavStart += wavSize;
    wavStart += kTerminatorSampleLength;
    }

  std::unique_ptr<float[]> samples(new float[wavStart]);
  float* wr = samples.get();

  // exception safe
  for(const auto& wav : wave) {
    wav.toFloatSamples(wr);
    wr+=wav.wavedata.size()/sizeof(int16_t);

    // terminator samples.
    for(size_t i=0; i<kTerminatorSampleLength; i++) {
      *wr = 0.f;
      ++wr;
      }
    }
  count = wavStart;
  return samples;
  }

// ---------------------------------------------------------------------------
// SF2 blob serializer
// ---------------------------------------------------------------------------

namespace {

struct BlobWriter {
  std::vector<uint8_t> data;

  size_t pos() const { return data.size(); }

  void writeU8(uint8_t v) {
    data.push_back(v);
    }

  void writeS8(int8_t v) {
    data.push_back(uint8_t(v));
    }

  void writeU16(uint16_t v) {
    data.push_back(uint8_t(v & 0xFF));
    data.push_back(uint8_t((v >> 8) & 0xFF));
    }

  void writeS16(int16_t v) {
    writeU16(uint16_t(v));
    }

  void writeU32(uint32_t v) {
    data.push_back(uint8_t(v & 0xFF));
    data.push_back(uint8_t((v >> 8) & 0xFF));
    data.push_back(uint8_t((v >> 16) & 0xFF));
    data.push_back(uint8_t((v >> 24) & 0xFF));
    }

  void writeBytes(const char* buf, size_t len) {
    for(size_t i=0; i<len; ++i)
      data.push_back(uint8_t(buf[i]));
    }

  // Write exactly fixedLen bytes: copy from str (up to fixedLen), zero-pad remainder
  void writeStr(const char* str, size_t fixedLen) {
    size_t slen = str ? std::strlen(str) : 0;
    for(size_t i=0; i<fixedLen; ++i)
      data.push_back(i < slen ? uint8_t(str[i]) : 0);
    }

  // Write a RIFF 4-char tag
  void writeTag(const char* tag) {
    writeBytes(tag, 4);
    }

  // Patch a uint32_t at 'offset' with current value
  void patchU32(size_t offset, uint32_t v) {
    data[offset+0] = uint8_t(v & 0xFF);
    data[offset+1] = uint8_t((v >> 8) & 0xFF);
    data[offset+2] = uint8_t((v >> 16) & 0xFF);
    data[offset+3] = uint8_t((v >> 24) & 0xFF);
    }

  // Begin a chunk: write tag + placeholder size, return size-field offset
  size_t beginChunk(const char* tag) {
    writeTag(tag);
    size_t szOff = pos();
    writeU32(0);
    return szOff;
    }

  // End a chunk: patch size, add pad byte if size is odd
  void endChunk(size_t szOff) {
    size_t contentStart = szOff + 4;
    size_t contentSize  = pos() - contentStart;
    patchU32(szOff, uint32_t(contentSize));
    if(contentSize & 1)
      writeU8(0); // pad to even
    }

  // Begin a LIST chunk (tag + list-type written inside)
  size_t beginList(const char* tag, const char* listType) {
    writeTag(tag);
    size_t szOff = pos();
    writeU32(0);
    writeTag(listType);
    return szOff;
    }

  void endList(size_t szOff) {
    endChunk(szOff);
    }
  };

} // anonymous namespace

std::vector<uint8_t> Hydra::toSf2Blob() const {
  BlobWriter w;

  // Top-level RIFF sfbk
  size_t riffOff = w.beginList("RIFF","sfbk");

  // ---- INFO list ----
  {
  size_t infoOff = w.beginList("LIST","INFO");

  // ifil: SF2 version 2.1
  {
  size_t cOff = w.beginChunk("ifil");
  w.writeU16(2); // major
  w.writeU16(1); // minor
  w.endChunk(cOff);
  }

  // isng
  {
  size_t cOff = w.beginChunk("isng");
  w.writeStr("EMU8000",8);
  w.endChunk(cOff);
  }

  // INAM — length must be even (RIFF); write without null terminator (10 bytes)
  {
  static const char inam[] = "Gothic DLS";
  size_t cOff = w.beginChunk("INAM");
  w.writeStr(inam, sizeof(inam)-1); // 10 chars, no null → even size
  w.endChunk(cOff);
  }

  w.endList(infoOff);
  }

  // ---- sdta list ----
  {
  size_t sdtaOff = w.beginList("LIST","sdta");

  // smpl: convert float → int16_t
  {
  size_t cOff = w.beginChunk("smpl");
  for(size_t i=0; i<wdataSize; ++i) {
    float f = wdata[i];
    if(f >  1.f) f =  1.f;
    if(f < -1.f) f = -1.f;
    int16_t s = int16_t(f * 32767.f);
    w.writeS16(s);
    }
  w.endChunk(cOff);
  }

  w.endList(sdtaOff);
  }

  // ---- pdta list ----
  {
  size_t pdtaOff = w.beginList("LIST","pdta");

  // phdr: 38 bytes each
  {
  size_t cOff = w.beginChunk("phdr");
  for(const auto& h : phdr) {
    w.writeStr(h.presetName, 20);
    w.writeU16(h.preset);
    w.writeU16(h.bank);
    w.writeU16(h.presetBagNdx);
    w.writeU32(h.library);
    w.writeU32(h.genre);
    w.writeU32(h.morphology);
    }
  w.endChunk(cOff);
  }

  // pbag: 4 bytes each
  {
  size_t cOff = w.beginChunk("pbag");
  for(const auto& b : pbag) {
    w.writeU16(b.genNdx);
    w.writeU16(b.modNdx);
    }
  w.endChunk(cOff);
  }

  // pmod: 10 bytes each
  {
  size_t cOff = w.beginChunk("pmod");
  for(const auto& m : pmod) {
    w.writeU16(m.modSrcOper);
    w.writeU16(m.modDestOper);
    w.writeS16(m.modAmount);
    w.writeU16(m.modAmtSrcOper);
    w.writeU16(m.modTransOper);
    }
  w.endChunk(cOff);
  }

  // pgen: 4 bytes each
  {
  size_t cOff = w.beginChunk("pgen");
  for(const auto& g : pgen) {
    w.writeU16(g.genOper);
    w.writeU16(g.genAmount.wordAmount);
    }
  w.endChunk(cOff);
  }

  // inst: 22 bytes each
  {
  size_t cOff = w.beginChunk("inst");
  for(const auto& i : inst) {
    w.writeStr(i.instName, 20);
    w.writeU16(i.instBagNdx);
    }
  w.endChunk(cOff);
  }

  // ibag: 4 bytes each
  {
  size_t cOff = w.beginChunk("ibag");
  for(const auto& b : ibag) {
    w.writeU16(b.instGenNdx);
    w.writeU16(b.instModNdx);
    }
  w.endChunk(cOff);
  }

  // imod: 10 bytes each
  {
  size_t cOff = w.beginChunk("imod");
  for(const auto& m : imod) {
    w.writeU16(m.modSrcOper);
    w.writeU16(m.modDestOper);
    w.writeS16(m.modAmount);
    w.writeU16(m.modAmtSrcOper);
    w.writeU16(m.modTransOper);
    }
  w.endChunk(cOff);
  }

  // igen: 4 bytes each
  {
  size_t cOff = w.beginChunk("igen");
  for(const auto& g : igen) {
    w.writeU16(g.genOper);
    w.writeU16(g.genAmount.wordAmount);
    }
  w.endChunk(cOff);
  }

  // shdr: 46 bytes each
  // SF2 spec requires startLoop and endLoop to be within [start, end].
  // TSF may leave them as 0 for non-looping samples; clamp here so downstream
  // SF2 consumers do not sanitize them (which can misplace the loop).
  {
  size_t cOff = w.beginChunk("shdr");
  for(const auto& s : shdr) {
    uint32_t sl = s.startLoop;
    uint32_t el = s.endLoop;
    // Clamp loop points inside sample boundaries
    if(s.end > s.start) {
      if(sl < s.start || sl >= s.end) sl = s.start;
      if(el <= sl || el > s.end)      el = s.end;
      } else {
      sl = el = s.start; // degenerate sample — zero-length loop
      }
    w.writeStr(s.sampleName, 20);
    w.writeU32(s.start);
    w.writeU32(s.end);
    w.writeU32(sl);
    w.writeU32(el);
    w.writeU32(s.sampleRate);
    w.writeU8 (s.originalPitch);
    w.writeS8 (s.pitchCorrection);
    w.writeU16(s.sampleLink);
    w.writeU16(s.sampleType);
    }
  w.endChunk(cOff);
  }

  w.endList(pdtaOff);
  }

  w.endList(riffOff);
  return std::move(w.data);
  }

bool Hydra::validate(const tsf_hydra &tsf) const {
  if(!comp(tsf.phdrs,phdr.data(),tsf.phdrNum,phdr.size()))
    return false;
  if(!comp(tsf.pbags,pbag.data(),tsf.pbagNum,pbag.size()))
    return false;
  if(!comp(tsf.pmods,pmod.data(),tsf.pmodNum,pmod.size()))
    return false;
  if(!comp(tsf.pgens,pgen.data(),tsf.pgenNum,pgen.size()))
    return false;
  if(!comp(tsf.insts,inst.data(),tsf.instNum,inst.size()))
    return false;
  if(!comp(tsf.ibags,ibag.data(),tsf.ibagNum,ibag.size()))
    return false;
  if(!comp(tsf.imods,imod.data(),tsf.imodNum,imod.size()))
    return false;
  if(!comp(tsf.igens,igen.data(),tsf.igenNum,igen.size()))
    return false;
  if(!comp(tsf.shdrs,shdr.data(),tsf.shdrNum,shdr.size()))
    return false;
  return true;
  }

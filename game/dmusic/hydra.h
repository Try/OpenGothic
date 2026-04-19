#pragma once

#include <vector>
#include <cstdint>
#include <memory>

struct tsf_hydra_phdr;
struct tsf_hydra_pbag;
struct tsf_hydra_pmod;
struct tsf_hydra_pgen;
struct tsf_hydra_inst;
struct tsf_hydra_ibag;
struct tsf_hydra_imod;
struct tsf_hydra_igen;
struct tsf_hydra_shdr;
struct tsf_hydra;
struct tsf;

namespace Dx8 {

class DlsCollection;
class Wave;

class Hydra {
  public:
    enum LoopModeOverride : uint8_t {
      LoopModeAuto = 0,
      LoopModeForceSustain = 1,
      LoopModeForceRelease = 2
      };

    enum Generator : uint8_t {
      kModLfoToPitch = 5,
      kVibLfoToPitch = 6,
      kModEnvToPitch = 7,
      kInitialFilterFc = 8,
      kInitialFilterQ  = 9,
      kModLfoToFilterFc = 10,
      kModEnvToFilterFc = 11,
      kModLfoToVolume = 13,
      kPan           = 17,
      kDelayModLFO   = 21,
      kFreqModLFO    = 22,
      kDelayVibLFO   = 23,
      kFreqVibLFO    = 24,
      kDelayModEnv   = 25,
      kAttackModEnv  = 26,
      kHoldModEnv    = 27,
      kDecayModEnv   = 28,
      kSustainModEnv = 29,
      kReleaseModEnv = 30,
      kKeynumToModEnvHold = 31,
      kKeynumToModEnvDecay = 32,
      kDelayVolEnv   = 33,
      kAttackVolEnv  = 34,
      kHoldVolEnv    = 35,
      kDecayVolEnv   = 36,
      kSustainVolEnv = 37,
      kReleaseVolEnv = 38,
      kKeynumToVolEnvHold = 39,
      kKeynumToVolEnvDecay = 40,
      kKeyRange      = 43,
      kVelRange      = 44,
      kInitialAttenuation = 48,
      kCoarseTune    = 51,
      kFineTune      = 52,
      kSampleID      = 53,
      kSampleModes   = 54,
      kScaleTuning   = 56,
      kExclusiveClass = 57,
      };
    enum ArticulatorSource : uint16_t {
      SourceNone = 0x0000,
      SourceLFO = 0x0001,
      SourceKeyOnVelocity = 0x0002,
      SourceKeyNumber = 0x0003,
      SourceEG2 = 0x0005,
      SourceVibrato = 0x0009
      };
    enum ArticulatorDestination : uint16_t {
      /* Generic Destinations */
      None            = 0x0000,
      Attenuation     = 0x0001,
      Pitch           = 0x0003,
      Pan             = 0x0004,
      /* LFO Destinations */
      LFOFrequency    = 0x0104,
      LFOStartDelay   = 0x0105,
      VibFrequency    = 0x0114,
      VibStartDelay   = 0x0115,
      /* EG1 Destinations */
      EG1AttachTime   = 0x0206,
      EG1DecayTime    = 0x0207,
      EG1ReleaseTime  = 0x0209,
      EG1SustainLevel = 0x020a,
      EG1DelayTime    = 0x020b,
      EG1HoldTime     = 0x020c,
      /* EG2 Destinations */
      EG2AttachTime   = 0x030a,
      EG2DecayTime    = 0x030b,
      EG2ReleaseTime  = 0x030d,
      EG2SustainLevel = 0x030e,
      EG2DelayTime    = 0x030f,
      EG2HoldTime     = 0x0310,
      FilterCutoff    = 0x0500,
      FilterQ         = 0x0501
      };

    Hydra(const DlsCollection& dls, const std::vector<Wave>& wave);
    ~Hydra();

    static void finalize(tsf* tsf);
    static bool hasNotes(tsf* tsf);
    static void getBankPreset(tsf* f, int presetIdx, int& bank, int& preset);

    static void noteOn(tsf* f, int presetId, int key, float vel);
    static void noteOff(tsf* f, int presetId, int key);
    static int  presetIndex(const tsf* f, int bank, int presetNum);
    static void renderFloat(tsf* f, float* buffer, int samples, int flag);
    static int  channelSetPan(tsf* f, int channel, float pan);
    static void setOutput(tsf* f, int samplerate, float gain);
    static void setLoopModeOverride(int mode);
    static int  getLoopModeOverride();

    tsf* toTsf   ();
    void toTsf   (tsf_hydra& out);
    bool validate(const tsf_hydra& tsf) const;

    std::vector<uint8_t> toSf2Blob() const;

    std::vector<tsf_hydra_phdr> phdr;
    std::vector<tsf_hydra_pbag> pbag;
    std::vector<tsf_hydra_pmod> pmod;
    std::vector<tsf_hydra_pgen> pgen;
    std::vector<tsf_hydra_inst> inst;
    std::vector<tsf_hydra_ibag> ibag;
    std::vector<tsf_hydra_imod> imod;
    std::vector<tsf_hydra_igen> igen;
    std::vector<tsf_hydra_shdr> shdr;

    std::unique_ptr<float[]>    wdata;
    size_t                      wdataSize=0;

  private:
    static bool              mkGeneratorOp(uint16_t usSource, uint16_t usControl, uint16_t usDestination, uint16_t usTransform, uint16_t& out);
    std::unique_ptr<float[]> allocSamples(const std::vector<Dx8::Wave>& wave, std::vector<tsf_hydra_shdr> &samples, size_t &count);
  };

}


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
    enum Generator : uint8_t {
      kAttackModEnv  = 26,
      kDecayModEnv   = 28,
      kSustainModEnv = 29,
      kReleaseModEnv = 30,
      kAttackVolEnv  = 34,
      kDecayVolEnv   = 36,
      kSustainVolEnv = 37,
      kReleaseVolEnv = 38,
      kKeyRange      = 43,
      kVelRange      = 44,
      kSampleID      = 53,
      kSampleModes   = 54,
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
      /* EG1 Destinations */
      EG1AttachTime   = 0x0206,
      EG1DecayTime    = 0x0207,
      EG1ReleaseTime  = 0x0209,
      EG1SustainLevel = 0x020a,
      /* EG2 Destinations */
      EG2AttachTime   = 0x030a,
      EG2DecayTime    = 0x030b,
      EG2ReleaseTime  = 0x030d,
      EG2SustainLevel = 0x030e
      };

    Hydra(const DlsCollection& dls, const std::vector<Wave>& wave);
    ~Hydra();

    static void finalize(tsf* tsf);
    static bool hasNotes(tsf* tsf);

    tsf* toTsf   ();
    void toTsf   (tsf_hydra& out);
    bool validate(const tsf_hydra& tsf) const;

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
    static uint16_t          mkGeneratorOp(uint16_t usDestination);
    std::unique_ptr<float[]> allocSamples(const std::vector<Dx8::Wave>& wave, std::vector<tsf_hydra_shdr> &samples, size_t &count);
  };

}


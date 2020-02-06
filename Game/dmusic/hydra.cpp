#include "hydra.h"

#include "dlscollection.h"

#define TSF_IMPLEMENTATION
// #define TSF_STATIC

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#include "tsf.h"
#pragma GCC diagnostic pop
#else
#include "tsf.h"
#endif

using namespace Dx8;

enum {
  kTerminatorSampleLength=46
  };

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
  r.preset       = uint16_t(instr.header.Locale.ulInstrument);
  r.bank         = uint16_t(instr.header.Locale.ulBank);
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

uint16_t Hydra::mkGeneratorOp(uint16_t usDestination) {
  switch(usDestination) {
    case EG1AttachTime:
      return kAttackVolEnv;
    case EG1DecayTime:
      return kDecayVolEnv;
    case EG1ReleaseTime:
      return kReleaseVolEnv;
    case EG1SustainLevel:
      return kSustainVolEnv;

    case EG2AttachTime:
      return kAttackModEnv;
    case EG2DecayTime:
      return kDecayModEnv;
    case EG2ReleaseTime:
      return kReleaseModEnv;
    case EG2SustainLevel:
      return kSustainModEnv;
    default:
      return 0;// error
    }
  }

Hydra::Hydra(const DlsCollection &dls,const std::vector<Wave>& wave) {
  std::vector<tsf_hydra_shdr> samples;
  wdata = allocSamples(wave,samples,wdataSize);

  const uint16_t modIndex     = 0;
  const uint16_t instModNdx   = 0;
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
      const auto& wavesample = reg.waveSample;

      ibag.push_back(mkIbag(uint16_t(igen.size()),instModNdx));

      uint16_t keyrangeLow  = hdr.RangeKey.usLow;
      uint16_t keyrangeHigh = hdr.RangeKey.usHigh;
      uint16_t velrangeLow  = 0;
      uint16_t velrangeHigh = 0;
      if(hdr.RangeVelocity.usHigh<=hdr.RangeVelocity.usLow) {
        velrangeLow  = 0;
        velrangeHigh = 127;
        } else {
        velrangeLow  = hdr.RangeVelocity.usLow;
        velrangeHigh = hdr.RangeVelocity.usHigh;
        }
      igen.push_back(mkIgen(kKeyRange,keyrangeLow,keyrangeHigh));
      igen.push_back(mkIgen(kVelRange,velrangeLow,velrangeHigh));

      const uint16_t time = 61550; //FIXME
      igen.push_back(mkIgen(kAttackVolEnv,time));

      for(const auto& art : instr.articulators) {
        for(const auto& c : art.connectionBlocks) {
          if(c.usControl == 0 && c.usSource == 0 && c.usTransform == 0) {
            uint16_t gen = mkGeneratorOp(c.usDestination);
            igen.push_back(mkIgen(gen,uint16_t(c.lScale/65536)));
            }
          }
        }

      if(hdr.RangeKey.usHigh < hdr.RangeKey.usLow)
        throw std::runtime_error("Invalid key range in instrument");

      tsf_hydra_shdr sample={};
      if(reg.loop.size()==0) {
        sample = samples[wavelink.ulTableIndex];
        sample.startLoop = 0;
        sample.endLoop   = 0;
        igen.push_back(mkIgen(kSampleModes,0));
        } else {
        auto loop = reg.loop[0];
        sample = samples[wavelink.ulTableIndex];
        sample.startLoop = loop.ulLoopStart;
        sample.endLoop   = loop.ulLoopStart + loop.ulLoopLength;

        sample.startLoop += sample.start;
        sample.endLoop   += sample.start;
        igen.push_back(mkIgen(kSampleModes,1));
        }

      igen.push_back(mkIgen(kSampleID,uint16_t(shdr.size())));

      sample.originalPitch   = uint8_t(wavesample.usUnityNote);
      sample.pitchCorrection = int8_t (wavesample.sFineTune);
      shdr.push_back(sample);
      }
    }

  phdr.push_back(mkPhdr("EOP",uint16_t(phdr.size())));
  pbag.push_back(mkPbag(uint16_t(pbag.size()),modIndex));

  tsf_hydra_pmod mod={};
  pmod.push_back(mod);
  pgen.push_back(mkPgen(0,0));

  inst.push_back(mkInst("EOI",instBagIndex));
  ibag.push_back(mkIbag(uint16_t(igen.size()),instModNdx));

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

bool Hydra::hasNotes(tsf *tsf) {
  return tsf->voiceNum>0;
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
    sx.startLoop       = 0; // will be overriden, later
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

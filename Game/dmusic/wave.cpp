#include "soundfont.h"
#include "wave.h"

#include <Tempest/File>
#include <Tempest/MemReader>
#include <Tempest/MemWriter>
#include <Tempest/Log>

#include <stdexcept>
#include <memory>
#include <cassert>

template<class T>
static T clip(T v,T low,T hi){
  if(v<low)
    return low;
  if(v>hi)
    return hi;
  return v;
  }

using namespace Dx8;

Wave::Wave(Riff& input) {
  if(!input.is("LIST"))
    throw std::runtime_error("not a list");
  input.readListId("wave");
  implRead(input);
  }

Wave::Wave(const char *dbg) {
  Tempest::RFile fin(dbg);
  std::vector<uint8_t> v(size_t(fin.size()));
  fin.read(&v[0],v.size());

  auto input = Dx8::Riff(v.data(),v.size());
  if(!input.is("RIFF"))
    throw std::runtime_error("not a list");
  input.readListId("WAVE");
  implRead(input);
  }

Wave::Wave(const int16_t *pcm, size_t count) {
  wavedata.resize(count*sizeof(int16_t));
  std::memcpy(&wavedata[0],pcm,wavedata.size());

  wfmt.wFormatTag       = Dx8::Wave::PCM;
  wfmt.wChannels        = 2;
  wfmt.dwSamplesPerSec  = SoundFont::SampleRate;
  wfmt.dwAvgBytesPerSec = wfmt.dwSamplesPerSec * uint32_t(wfmt.wChannels * sizeof(int16_t));
  wfmt.wBlockAlign      = 2;
  wfmt.wBitsPerSample   = 16;
  }

void Wave::implRead(Riff &input) {
  input.read([this](Riff& c){
    implParse(c);
    });

  if(wfmt.wFormatTag==Dx8::Wave::ADPCM) {
    uint16_t                   samplesPerBlock=0;
    std::unique_ptr<int16_t[]> coeffTable;
    uint16_t                   nCoefs=0;
    int                        errct=0;

    {
    static const int16_t msAdpcmIcoef[7][2] = {
      { 256,   0},
      { 512,-256},
      {   0,   0},
      { 192,  64},
      { 240,   0},
      { 460,-208},
      { 392,-232}
      };

    Tempest::MemReader f(extra.data(),extra.size());
    f.read(&samplesPerBlock,sizeof(samplesPerBlock));
    f.read(&nCoefs,sizeof(nCoefs));
    if(nCoefs<7 || nCoefs>0x100)
      throw std::runtime_error("invalid MS ADPCM sound");
    if(extra.size()<size_t(4+4*nCoefs))
      throw std::runtime_error("invalid MS ADPCM sound");
    coeffTable.reset(new int16_t[nCoefs*2]);
    for(size_t i=0; i<2*nCoefs; i++) {
      f.read(&coeffTable[i],2);
      if(i<14)
        errct += (coeffTable[i] != msAdpcmIcoef[i/2][i%2]);
      }
    }
    if(errct>0)
      Tempest::Log::i("");

    size_t blockCount = (wavedata.size()+wfmt.wBlockAlign-1) / wfmt.wBlockAlign;

    /* We decode two samples per byte. There will be blockCount headers in the data chunk.
     *  This is enough to know how to calculate the total PCM frame count. */
    size_t totalBlockHeaderSizeInBytes = blockCount * (6*wfmt.wChannels);
    size_t totalPCMFrameCount = ((wavedata.size() - totalBlockHeaderSizeInBytes) * 2) / wfmt.wChannels;

    std::vector<uint8_t> src = std::move(wavedata);
    wavedata.resize(totalPCMFrameCount*wfmt.wChannels*sizeof(int16_t));

    Tempest::MemReader f(src.data(),src.size());
    decodeAdpcm(f,totalPCMFrameCount,wfmt.wBlockAlign,wfmt.wChannels,reinterpret_cast<int16_t*>(wavedata.data()));

    wfmt.wFormatTag       = Dx8::Wave::PCM;
    wfmt.wChannels        = 2;
    //wfmt.dwSamplesPerSec  = wavedata.size()/2;
    wfmt.dwAvgBytesPerSec = wfmt.dwSamplesPerSec * uint32_t(wfmt.wChannels * sizeof(int16_t));
    wfmt.wBlockAlign      = 2;
    wfmt.wBitsPerSample   = 16;
    }
  }

void Wave::implParse(Riff& input) {
  if(input.is("data"))
    input.read(wavedata);
  if(input.is("fmt ")) {
    input.read(&wfmt,sizeof(wfmt));
    if(input.remaning()>=2){
      uint16_t cbSize=0;
      input.read(&cbSize,2);
      extra.resize(cbSize);
      if(cbSize>0)
        input.read(&extra[0],extra.size());
      }
    }
  if(input.is("wsmp")){
    input.read(&waveSample,sizeof(waveSample));
    loop.resize(waveSample.cSampleLoops);
    for(auto& i:loop){
      input.read(&i,sizeof(i));
      input.skip(i.cbSize-sizeof(i));
      }
    }
  if(input.is("LIST") && input.isListId("INFO")) {
    info = Info(input);
    }
  }

size_t Wave::decodeAdpcm(Tempest::MemReader& rd, const size_t framesToRead,
                           uint16_t blockAlign, uint16_t channels, int16_t* pBufferOut) {
  size_t total = 0;
  while(total<framesToRead) {
    size_t decoded = decodeAdpcmBlock(rd,framesToRead-total,blockAlign,channels,pBufferOut);
    if(decoded==0)
      break;
    total      += decoded;
    pBufferOut += decoded*channels;
    }
  return total;
  }

int32_t Wave::decodeADPCMFrame(AdpcChannel& msadpcm, int32_t nibble) {
  static const int32_t adaptationTable[] = {
    230, 230, 230, 230, 307, 409, 512, 614,
    768, 614, 512, 409, 307, 230, 230, 230
    };

  static const int32_t coeff1Table[] = { 256, 512, 0, 192, 240, 460,  392 };
  static const int32_t coeff2Table[] = { 0,  -256, 0, 64,  0,  -208, -232 };

  int32_t sample=0;
  sample  = ((msadpcm.prevFrames[1] * coeff1Table[msadpcm.predictor]) +
             (msadpcm.prevFrames[0] * coeff2Table[msadpcm.predictor])) >> 8;

  int32_t nibbleMul = (nibble & 0x08) ? (nibble-16) : nibble;
  sample += nibbleMul * msadpcm.delta;
  sample  = clip(sample, -32768, 32767);

  msadpcm.delta = (adaptationTable[nibble] * msadpcm.delta) >> 8;
  if(msadpcm.delta<16)
    msadpcm.delta = 16;

  msadpcm.prevFrames[0] = msadpcm.prevFrames[1];
  msadpcm.prevFrames[1] = sample;

  return sample;
  }

size_t Wave::decodeAdpcmBlock(Tempest::MemReader& rd, const size_t framesToRead,
                              uint16_t blockAlign, uint16_t channels, int16_t* pBufferOut) {
  size_t    totalFramesRead = 0;
  AdpcState msadpcm         = {};
  size_t    bytesRemaining  = 0;

  if(channels == 1) {
    /* Mono. */
    uint8_t     predictor;
    AdpcHdrMono header;
    if(rd.read(&predictor,1)!=1 ||
       rd.read(&header,sizeof(header))!=sizeof(header))
      return totalFramesRead;
    bytesRemaining = blockAlign-sizeof(header)-1;

    AdpcChannel& c = msadpcm.channel[0];
    c.predictor     = predictor;
    c.delta         = header.delta;
    c.prevFrames[1] = header.prevFrames1;
    c.prevFrames[0] = header.prevFrames0;

    msadpcm.cachedFrames[2]  = c.prevFrames[0];
    msadpcm.cachedFrames[3]  = c.prevFrames[1];
    msadpcm.cachedFrameCount = 2;
    } else {
    /* Stereo. */
    uint8_t       predictor[2];
    AdpcHdrStereo header;
    if(rd.read(&predictor,2)!=2 ||
       rd.read(&header,sizeof(header))!=sizeof(header))
      return totalFramesRead;

    bytesRemaining = blockAlign-sizeof(header)-2;

    AdpcChannel& c0 = msadpcm.channel[0];
    AdpcChannel& c1 = msadpcm.channel[0];

    c0.predictor     = predictor[0];
    c1.predictor     = predictor[1];
    c0.delta         = header.delta[0];
    c1.delta         = header.delta[1];
    c0.prevFrames[1] = header.prevFrames1[0];
    c1.prevFrames[1] = header.prevFrames1[1];
    c0.prevFrames[0] = header.prevFrames0[0];
    c1.prevFrames[0] = header.prevFrames0[1];

    msadpcm.cachedFrames[0] = c0.prevFrames[0];
    msadpcm.cachedFrames[1] = c1.prevFrames[0];
    msadpcm.cachedFrames[2] = c0.prevFrames[1];
    msadpcm.cachedFrames[3] = c1.prevFrames[1];
    msadpcm.cachedFrameCount = 2;
    }

  while(true) {
    // flush cache
    while(msadpcm.cachedFrameCount>0 && totalFramesRead<framesToRead) {
      const int32_t* cache = msadpcm.cachedFrames+MAX_CACHED_FRAMES-(msadpcm.cachedFrameCount*channels);
      for(uint16_t i=0; i<channels; i++)
        pBufferOut[i] = int16_t(cache[i]);

      pBufferOut               += channels;
      totalFramesRead          += 1;
      msadpcm.cachedFrameCount -= 1;
      }

    if(totalFramesRead>=framesToRead)
      return totalFramesRead;

    if(bytesRemaining==0)
      return totalFramesRead;

    // fill cache
    uint8_t nibbles=0;
    if(rd.read(&nibbles,1)!=1)
      return totalFramesRead;

    bytesRemaining--;
    int32_t nibble0 = ((nibbles & 0xF0) >> 4);
    int32_t nibble1 = ((nibbles & 0x0F) >> 0);

    if(channels==1) {
      /* Mono. */
      msadpcm.cachedFrames[2]  = decodeADPCMFrame(msadpcm.channel[0],nibble0);
      msadpcm.cachedFrames[3]  = decodeADPCMFrame(msadpcm.channel[0],nibble1);
      msadpcm.cachedFrameCount = 2;
      } else {
      /* Stereo. */
      msadpcm.cachedFrames[2]  = decodeADPCMFrame(msadpcm.channel[0],nibble0);
      msadpcm.cachedFrames[3]  = decodeADPCMFrame(msadpcm.channel[1],nibble1);
      msadpcm.cachedFrameCount = 1;
      }
    }
  }

void Wave::toFloatSamples(float* out) const {
  const int16_t* smp = reinterpret_cast<const int16_t*>(wavedata.data());
  for(size_t i=0;i<wavedata.size();i+=sizeof(int16_t), ++smp) {
    *out = (*smp)/32767.f;
    ++out;
    }
  }

void Wave::save(const char *path) const {
  Tempest::WFile f(path);
  f.write("RIFF",4);

  uint32_t fmtSize = uint32_t(sizeof(wfmt)) + uint32_t(extra.size()>0 ? (2+extra.size()) : 0);
  uint32_t sz      = 8u + fmtSize + 8u + uint32_t(wavedata.size());
  f.write(reinterpret_cast<const char*>(&sz),4);
  f.write("WAVE",4);

  f.write("fmt ",4);
  sz=fmtSize;
  f.write(&sz,4);
  f.write(&wfmt,sizeof(wfmt));
  if(extra.size()>0){
    uint16_t sz=uint16_t(extra.size());
    f.write(&sz,2);
    f.write(&extra[0],extra.size());
    }

  f.write("data",4);
  sz=uint32_t(wavedata.size());
  f.write(&sz,4);
  f.write(wavedata.data(),sz);
  }

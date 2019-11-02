#include "soundfont.h"
#include "wave.h"

#include <Tempest/File>
#include <Tempest/MemReader>
#include <Tempest/MemWriter>
#include <Tempest/Log>

#include <stdexcept>
#include <memory>

using namespace Dx8;

static const int16_t msAdpcmIcoef[7][2] = {
  { 256,   0},
  { 512,-256},
  {   0,   0},
  { 192,  64},
  { 240,   0},
  { 460,-208},
  { 392,-232}
  };

template<class T>
static T clip(T v,T low,T hi){
  if(v<low)
    return low;
  if(v>hi)
    return hi;
  return v;
  }

template<class X,class P>
static void lsbshortldi(X& x,P*& p){
  x  = int16_t(int(p[0]) + (int(p[1])<<8));
  p += 2;
  }

static uint16_t msAdpcmSamplesIn(size_t dataLen, size_t chans,
                                 size_t blockAlign, size_t samplesPerBlock){
  size_t m, n;
  if(samplesPerBlock) {
    n = (dataLen / blockAlign) * samplesPerBlock;
    m = (dataLen % blockAlign);
    } else {
    n = 0;
    m = blockAlign;
    }
  if(m>=size_t(7*chans)) {
    m -= 7*chans;          /* bytes beyond block-header */
    m = (2*m)/chans + 2;   /* nibbles/chans + 2 in header */
    if(samplesPerBlock && m > samplesPerBlock)
      m = samplesPerBlock;
    n += m;
    }
  return uint16_t(n);
  }

Wave::Wave(Riff& input) {
  if(!input.is("LIST"))
    throw std::runtime_error("not a list");
  input.readListId("wave");
  input.read([this](Riff& c){
    implRead(c);
    });

  if(wfmt.wFormatTag==Dx8::Wave::ADPCM){
    uint16_t                   samplesPerBlock=0;
    std::unique_ptr<int16_t[]> adpcm_i_coefs;
    uint16_t                   nCoefs=0;
    int                        errct=0;

    Tempest::MemReader f(extra.data(),extra.size());
    f.read(&samplesPerBlock,sizeof(samplesPerBlock));
    f.read(&nCoefs,sizeof(nCoefs));
    if(nCoefs<7 || nCoefs>0x100)
      throw std::runtime_error("invalid MS ADPCM sound");
    if(extra.size()<size_t(4+4*nCoefs))
      throw std::runtime_error("invalid MS ADPCM sound");
    adpcm_i_coefs.reset(new int16_t[nCoefs*2]);
    for(size_t i=0; i<2*nCoefs; i++) {
      f.read(&adpcm_i_coefs[i],2);
      if(i<14)
        errct += (adpcm_i_coefs[i] != msAdpcmIcoef[i/2][i%2]);
      }

    std::vector<uint8_t> src = std::move(wavedata);
    wavedata.clear();
    decodeAdpcm(src.data(),src.size(),samplesPerBlock,nCoefs,adpcm_i_coefs.get());

    wfmt.wFormatTag       = Dx8::Wave::PCM;
    wfmt.wChannels        = 2;
    //wfmt.dwSamplesPerSec;
    wfmt.dwAvgBytesPerSec = wfmt.dwSamplesPerSec * wfmt.wChannels;
    wfmt.wBlockAlign      = 2;
    wfmt.wBitsPerSample   = 16;
    }
  }

Wave::Wave(const char *dbg) {
  uint16_t                   samplesPerBlock=0;
  std::unique_ptr<int16_t[]> adpcm_i_coefs;
  uint16_t                   nCoefs=0;
  int                        errct=0;

  {
  Tempest::RFile f(dbg);
  f.seek(4+4*4);//wave + fmt
  f.read(&wfmt,16);

  if(wfmt.wFormatTag==Dx8::Wave::ADPCM) {
    uint16_t cbSize=0;
    f.read(&cbSize,2);

    f.read(&samplesPerBlock,sizeof(samplesPerBlock));
    f.read(&nCoefs,sizeof(nCoefs));
    if(nCoefs<7 || nCoefs>0x100)
      throw std::runtime_error("invalid MS ADPCM sound");
    if(cbSize<4+4*nCoefs)
      throw std::runtime_error("invalid MS ADPCM sound");

    adpcm_i_coefs.reset(new int16_t[nCoefs*2]);
    for(size_t i=0; i<2*nCoefs; i++) {
      f.read(&adpcm_i_coefs[i],2);
      if(i<14)
        errct += (adpcm_i_coefs[i] != msAdpcmIcoef[i/2][i%2]);
      }
    }

  f.seek(4);
  uint32_t len=0;
  f.read(&len,4);
  wavedata.resize(len);
  f.read(&wavedata[0],wavedata.size());
  }

  if(wfmt.wFormatTag==Dx8::Wave::ADPCM){
    std::vector<uint8_t> src = std::move(wavedata);
    wavedata.clear();
    decodeAdpcm(src.data(),src.size(),samplesPerBlock,nCoefs,adpcm_i_coefs.get());

    wfmt.wFormatTag       = Dx8::Wave::PCM;
    wfmt.wChannels        = 2;
    //wfmt.dwSamplesPerSec;
    wfmt.dwAvgBytesPerSec = wfmt.dwSamplesPerSec * wfmt.wChannels;
    wfmt.wBlockAlign      = 2;
    wfmt.wBitsPerSample   = 16;
    }
  }

Wave::Wave(const int16_t *pcm, size_t count) {
  wavedata.resize(count*sizeof(int16_t));
  std::memcpy(&wavedata[0],pcm,wavedata.size());

  wfmt.wFormatTag       = Dx8::Wave::PCM;
  wfmt.wChannels        = 2;
  wfmt.dwSamplesPerSec  = SoundFont::SampleRate;
  wfmt.dwAvgBytesPerSec = wfmt.dwSamplesPerSec * wfmt.wChannels * sizeof(int16_t);
  wfmt.wBlockAlign      = 2;
  wfmt.wBitsPerSample   = 16;
  }

void Wave::save(const char *path) const {
  Tempest::WFile f(path);
  f.write("RIFF",4);

  uint32_t fmtSize = sizeof(wfmt) + (extra.size()>0 ? (2+extra.size()) : 0);
  uint32_t sz      = 8 + fmtSize + 8 + wavedata.size();
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
  sz=wavedata.size();
  f.write(&sz,4);
  f.write(wavedata.data(),sz);
  }

Tempest::Sound Wave::toSound() const {
  struct IoHlp:Tempest::IDevice {
    size_t               pos=0;
    std::vector<uint8_t> buf;

    size_t  read(void* dest,size_t count){
      size_t c = std::min(count, buf.size()-pos);

      std::memcpy(dest, buf.data()+pos, c);
      pos+=c;

      return c;
      }

    size_t  size() const {
      return buf.size();
      }

    uint8_t peek() {
      if(pos==buf.size())
        return 0;
      return buf[pos];
      }

    size_t  seek(size_t advance) {
      size_t c = std::min(advance, buf.size()-pos);
      pos += c;
      return c;
      }
    };

  IoHlp h;

  Tempest::MemWriter f(h.buf);
  f.write("RIFF",4);

  uint32_t fmtSize = sizeof(wfmt) + (extra.size()>0 ? (2+extra.size()) : 0);
  uint32_t sz=  8+fmtSize + 8+wavedata.size();
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
  sz=wavedata.size();
  f.write(&sz,4);
  f.write(wavedata.data(),sz);

  return Tempest::Sound(h);
  }

void Wave::implRead(Riff &input) {
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

bool Wave::decodeAdpcm(const uint8_t* adpcm,size_t dataLen,uint16_t samplesPerBlock, uint16_t nCoefs,const int16_t* adpcm_i_coefs) {
  size_t   numSamples   = msAdpcmSamplesIn(dataLen, wfmt.wChannels, wfmt.wBlockAlign, samplesPerBlock);
  size_t   len          = numSamples*wfmt.wChannels;
  uint16_t smpRemaining = 0;

  std::unique_ptr<int16_t[]> samples{new int16_t[wfmt.wChannels*samplesPerBlock]};
  int16_t* samplePtr=nullptr;

  for(size_t done=0;done<len;){
    // See if need to read more from disk
    if(smpRemaining == 0) {
      smpRemaining = adpcmReadBlock(adpcm,dataLen,samplesPerBlock,nCoefs,adpcm_i_coefs,samples.get());
      if(smpRemaining==0) {
        // Don't try to read any more samples
        numSamples = 0;
        return true;
        }
      samplePtr = samples.get();
      }

    // Copy interleaved data into buf, converting to int16_t
    size_t ct = len-done;
    if(ct>size_t(smpRemaining*wfmt.wChannels))
      ct = smpRemaining*wfmt.wChannels;

    done += ct;
    smpRemaining -= (ct/wfmt.wChannels);
    int16_t* cur  = samplePtr;
    int16_t* top  = samplePtr+ct;

    while(cur<top) {
      int16_t v = *cur;
      uint8_t vv[2];
      std::memcpy(vv,&v,2);
      wavedata.push_back(vv[0]);
      wavedata.push_back(vv[1]);
      cur++;
      }

    samplePtr = cur;
    }
  return true;
  }

uint16_t Wave::adpcmReadBlock(const uint8_t*& adpcm,size_t& dataLen,
                              uint16_t samplesPerBlock,
                              uint16_t nCoefs,const int16_t* adpcm_i_coefs,
                              int16_t* samples) {
  const uint8_t* packet    = adpcm;
  size_t         bytesRead = std::min<size_t>(dataLen, wfmt.wBlockAlign);
  adpcm  +=bytesRead;
  dataLen-=bytesRead;

  uint16_t samplesThisBlock = samplesPerBlock;
  if(bytesRead<wfmt.wBlockAlign) {
    samplesThisBlock = msAdpcmSamplesIn(0,wfmt.wChannels,bytesRead,0);
    if(samplesThisBlock==0 || samplesThisBlock>samplesPerBlock)
      return 0; // error
    }

  auto errmsg = adpcmBlockExpand(wfmt.wChannels,nCoefs,adpcm_i_coefs,packet,samples,samplesThisBlock);
  if(errmsg){
    Tempest::Log::e(errmsg);
    return 0;
    }
  return uint16_t(samplesThisBlock);
  }

const char* Wave::adpcmBlockExpand(
    uint16_t channels,        /* total channels             */
    int nCoef,
    const int16_t* coef,
    const uint8_t* ibuff,     /* input buffer[blockAlign]   */
    int16_t *obuff,           /* output samples, n*chans    */
    int n                     /* samples to decode PER channel */
    ) {
  if(channels<=0)
    return "invalid channels count";

  const char *errmsg = nullptr;
  MsState state[4]={};  /* One decompressor state for each channel */

  /* Read the four-byte header for each channel */
  const uint8_t* ip = ibuff;
  for(uint16_t ch=0; ch<channels; ch++) {
    uint8_t bpred = *ip++;
    if(bpred >= nCoef) {
      errmsg = "MSADPCM bpred >= nCoef, arbitrarily using 0\n";
      bpred = 0;
      }
    state[ch].coef[0] = coef[bpred*2+0];
    state[ch].coef[1] = coef[bpred*2+1];
    }

  for(uint16_t ch=0; ch<channels; ch++)
    lsbshortldi(state[ch].step, ip);

  // sample1's directly into obuff
  for(uint16_t ch=0; ch<channels; ch++)
    lsbshortldi(obuff[channels+ch], ip);

  // sample2's directly into obuff
  for(uint16_t ch=0; ch<channels; ch++)
    lsbshortldi(obuff[ch], ip);

  // already have 1st 2 samples from block-header
  int16_t* prev = obuff + 0;
  int16_t* op   = obuff + 2*channels;
  int16_t* top  = obuff + n*channels;

  uint16_t ch2 = 0;
  while(op < top) {
    const uint8_t b[2] = {uint8_t((*ip)>>4), uint8_t((*ip)&0xf)};
    ++ip;

    for(uint32_t i=0;i<2;++i){
      op[i] = adpcmDecode(b[i], state+ch2, prev[channels], prev[0]);
      ch2 = (ch2+1)%channels;
      prev+=channels;
      }
    op +=2;
    }
  return errmsg;
  }

int16_t Wave::adpcmDecode(int32_t c, MsState *state, int32_t sample1, int32_t sample2) {
  static const int32_t stepAdjustTable[] = {
    230, 230, 230, 230, 307, 409, 512, 614,
    768, 614, 512, 409, 307, 230, 230, 230
    };

  const int32_t step  = state->step;

  // next step
  const int32_t nstep = (stepAdjustTable[c] * step) >> 8;
  state->step         = (nstep < 16) ? 16 : nstep;

  // linear prediction for next sample
  int32_t vlin = ((sample1*state->coef[0]) + (sample2*state->coef[1])) >> 8;
  /** then add the code*step adjustment **/
  c -= (c & 0x08) << 1;
  int32_t sample = (c*step) + vlin;

  return int16_t(clip(sample,-32768,32767));
  }

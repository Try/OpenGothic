#include "soundfont.h"

#include <sstream>
#include <algorithm>

#include "dlscollection.h"

#include "hydra.h"
#include "tsf.h"

using namespace Dx8;

struct SoundFont::Data {
  Data(const DlsCollection &dls){
    Dx8::Hydra dxhydra(dls);

    fnt   = dxhydra.toTsf();
    wdata = std::move(dxhydra.wdata);
    }

  ~Data() {
    Hydra::finalize(fnt);
    }

  tsf*                     fnt=nullptr; //FIXME: has to implement separated fonts
  std::unique_ptr<float[]> wdata;
  };

struct SoundFont::Impl {
  Impl(std::shared_ptr<Data> &shData,uint32_t dwPatch)
    :shData(shData), data(*shData) {
    uint8_t bankHi = (dwPatch & 0x00FF0000) >> 0x10;
    uint8_t bankLo = (dwPatch & 0x0000FF00) >> 0x8;
    uint8_t patch  = (dwPatch & 0x000000FF);
    int32_t bank   = (bankHi << 16) + bankLo;

    preset = tsf_get_presetindex(data.fnt, bank, patch);
    tsf_set_output(data.fnt,TSF_STEREO_INTERLEAVED,44100,0);
    }

  ~Impl(){}

  void setVolume(float v) {
    (void)v; // handled in mixer
    //tsf_set_output(fnt,TSF_STEREO_INTERLEAVED,44100,v);
    }

  void setPan(float p){
    tsf_channel_set_pan(data.fnt,0,p);
    tsf_channel_set_pan(data.fnt,1,p);
    }

  std::shared_ptr<Data> shData;
  SoundFont::Data&      data;
  int                   preset=0;
  int                   count=0;
  };

SoundFont::SoundFont() {
  }

SoundFont::SoundFont(std::shared_ptr<Data> &sh, uint32_t dwPatch) {
  impl   = std::shared_ptr<Impl>(new Impl(sh,dwPatch));
  }

SoundFont::~SoundFont() {
  }

std::shared_ptr<SoundFont::Data> SoundFont::shared(const DlsCollection &dls) {
  return std::shared_ptr<Data>(new Data(dls));
  }

bool SoundFont::hasNotes() const {
  return impl->count>0;
  }

void SoundFont::setVolume(float v) {
  impl->setVolume(v);
  }

void SoundFont::setPan(float p) {
  impl->setPan(p);
  }

void SoundFont::mix(float *samples, size_t count) {
  tsf_render_float(impl->data.fnt,samples,int(count),true);
  }

void SoundFont::setNote(uint8_t note, bool e, uint8_t velosity) {
  if( e ) {
    impl->count++;
    tsf_note_on (impl->data.fnt,impl->preset,note,velosity/127.f);
    } else {
    impl->count--;
    tsf_note_off(impl->data.fnt,impl->preset,note);
    }
  }

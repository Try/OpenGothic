#include "soundfont.h"

#include <sstream>
#include <algorithm>

#include "dlscollection.h"

#include "hydra.h"
#include "tsf.h"

using namespace Dx8;

struct SoundFont::Impl {
  Impl(const DlsCollection &dls,uint32_t dwPatch){
    uint8_t bankHi = (dwPatch & 0x00FF0000) >> 0x10;
    uint8_t bankLo = (dwPatch & 0x0000FF00) >> 0x8;
    uint8_t patch  = (dwPatch & 0x000000FF);
    int32_t bank   = (bankHi << 16) + bankLo;

    fnt    = implLooad(dls);
    preset = tsf_get_presetindex(fnt, bank, patch);
    tsf_set_output(fnt,TSF_STEREO_INTERLEAVED,44100,0);
    }

  ~Impl(){
    Hydra::finalize(fnt);
    }

  tsf* implLooad(const DlsCollection &dls) {
    Dx8::Hydra dxhydra(dls);

    tsf* res = dxhydra.toTsf();
    wdata = std::move(dxhydra.wdata);
    return res;
    }

  void setVolume(float v) {
    (void)v; // handled in mixer
    //tsf_set_output(fnt,TSF_STEREO_INTERLEAVED,44100,v);
    }

  void setPan(float p){
    tsf_channel_set_pan(fnt,0,p);
    tsf_channel_set_pan(fnt,1,p);
    }

  tsf*                     fnt=nullptr;
  int                      preset=0;
  int                      count=0;
  std::unique_ptr<float[]> wdata;
  };

SoundFont::SoundFont() {
  }

SoundFont::SoundFont(const DlsCollection &dls,uint32_t dwPatch) {
  impl = std::shared_ptr<Impl>(new Impl(dls,dwPatch));
  }

SoundFont::~SoundFont() {
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
  tsf_render_float(impl->fnt,samples,int(count),true);
  }

void SoundFont::setNote(uint8_t note, bool e, uint8_t velosity) {
  if( e ) {
    impl->count++;
    tsf_note_on (impl->fnt,impl->preset,note,velosity/127.f);
    } else {
    impl->count--;
    tsf_note_off(impl->fnt,impl->preset,note);
    }
  }

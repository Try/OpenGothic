#pragma once

#include <memory>

namespace Dx8 {

class DlsCollection;

class SoundFont final {
  public:
    enum  {
      SampleRate = 44100,
      BitsPerSec = SampleRate*2*16
      };
    struct Data;

    SoundFont();
    SoundFont(std::shared_ptr<Data> &sh, uint32_t dwPatch);
    ~SoundFont();

    static std::shared_ptr<Data> shared(const DlsCollection& dls);

    bool hasNotes() const;
    void setVolume(float v);
    void setPan(float p);
    void mix(float* samples,size_t count);
    void setNote(uint8_t note, bool e, uint8_t velosity);

  private:
    struct Impl;
    std::shared_ptr<Impl> impl;
  };

}

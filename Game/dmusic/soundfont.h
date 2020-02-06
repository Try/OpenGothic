#pragma once

#include <memory>
#include <vector>

namespace Dx8 {

class DlsCollection;
class Wave;

class SoundFont final {
  private:
    struct Impl;
    struct Instance;

  public:
    enum  {
      SampleRate = 44100,
      BitsPerSec = SampleRate*2*16
      };
    struct Data;

    class Ticket final {
      private:
        std::shared_ptr<Instance> impl;
        uint8_t                   note=0;

      public:
        bool operator==(const std::nullptr_t&) const {
          return impl==nullptr;
          }

      friend class SoundFont;
      };

    SoundFont();
    SoundFont(std::shared_ptr<Data> &sh, uint32_t dwPatch);
    ~SoundFont();

    static std::shared_ptr<Data> shared(const DlsCollection& dls, const std::vector<Wave>& wave);

    bool hasNotes() const;
    void setVolume(float v);
    void setPan(float p);
    void mix(float* samples,size_t count);

    Ticket      noteOn(uint8_t note, uint8_t velosity);
    static void noteOff(Ticket& t);

  private:
    std::shared_ptr<Impl> impl;
  };

}

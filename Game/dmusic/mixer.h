#pragma once

#include <Tempest/SoundDevice>
#include <Tempest/SoundEffect>

#include <vector>
#include <cstdint>
#include <thread>
#include <atomic>

#include "music.h"

namespace Dx8 {

class Mixer final {
  public:
    Mixer(size_t sz=2048);
    ~Mixer();

    void     setMusic(const Music& m);
    uint64_t currentPlayTime() const;

  private:
    void thFunc();
    void step(uint64_t dt);

    struct Sample final {
      int16_t l=0,r=0;
      };
    std::vector<Sample> pcm;

    std::atomic<bool> exitFlg;
    std::thread       mixTh;

    std::shared_ptr<const Music::Internal> current=nullptr;
    Tempest::SoundDevice                   device;
    std::atomic<uint64_t>                  timeCursor={};
    float                                  volume=0.3f;

    struct Part {
      Part(const Music::Record &w, Mixer& mix);
      Tempest::SoundEffect eff;
      uint64_t             endT=0;
      };
    std::vector<Part>      eff;
  };

}

#include "mixer.h"

#include <Tempest/Application>
#include <Tempest/SoundEffect>
#include <Tempest/Sound>
#include <Tempest/Log>

#include "wave.h"

using namespace Dx8;
using namespace Tempest;

Mixer::Part::Part(const Music::Record &rec, Mixer &mix) {
  auto eff = mix.device.load(*rec.wave);
  //eff.setPitch(rec.pitch);
  //eff.setPosition(rec.pan,0,0);
  eff.setVolume(mix.volume*rec.volume);
  eff.play();

  this->eff  = std::move(eff);
  this->endT = mix.timeCursor+rec.duration;
  }

Mixer::Mixer(size_t sz)
  :pcm(sz) {
  exitFlg.store(false);
  mixTh = std::thread(&Mixer::thFunc,this);
  }

Mixer::~Mixer() {
  exitFlg.store(true);
  mixTh.join();
  }

void Mixer::setMusic(const Music &m) {
  current = m.intern;
  }

uint64_t Mixer::currentPlayTime() const {
  return timeCursor.load();
  }

void Mixer::thFunc() {
  Log::d("start mixer thread");
  uint64_t time = Application::tickCount();
  uint64_t dt   = 0;
  while(!exitFlg.load()) {
    uint64_t t  = Application::tickCount();
    dt = (t-time);
    time=t;

    step(dt);
    Application::sleep(1);

    timeCursor += dt;
    }
  Log::d("shutdown mixer thread");
  }

void Mixer::step(uint64_t dt) {
  if(current==nullptr || current->timeTotal==0)
    return;
  uint64_t timeTotal = current->timeTotal;
  uint64_t b         = (timeCursor)%timeTotal;
  uint64_t e         = (timeCursor+dt)%timeTotal;

  bool     inv       = (e<b);

  static bool once=false;
  if(!once) {
    for(auto& i:current->idxWaves){
      if((b<=i.at && i.at<e)^inv) {
        eff.emplace_back(i,*this);

        //once=true;
        }
      }
    }

  for(size_t i=0;i<eff.size();){
    if(eff[i].eff.isFinished() || eff[i].endT<timeCursor){
      eff[i] = std::move(eff.back());
      eff.pop_back();
      } else {
      ++i;
      }
    }
  }

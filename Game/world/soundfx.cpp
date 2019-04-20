#include "soundfx.h"

#include "resources.h"

using namespace Tempest;

SoundFx::SoundFx(const char* s) {
  char name[256]={};
  std::snprintf(name,sizeof(name),"%s.WAV",s);

  auto snd = Resources::loadSoundBuffer(name);
  if(snd.isEmpty()){
    for(int i=1;i<100;++i){
      std::snprintf(name,sizeof(name),"%s%02d.WAV",s,i);
      auto snd = Resources::loadSoundBuffer(name);
      if(snd.isEmpty())
        break;
      inst.push_back(std::move(snd));
      }
    } else {
    inst.push_back(std::move(snd));
    }
  }

SoundEffect SoundFx::getEffect(SoundDevice &dev) {
  if(inst.size()==0)
    return SoundEffect();
  return dev.load(inst[size_t(std::rand())%inst.size()]);
  }

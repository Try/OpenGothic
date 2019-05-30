#include "soundfx.h"

#include <Tempest/Log>

#include "game/gamesession.h"
#include "gothic.h"
#include "resources.h"

using namespace Tempest;

SoundFx::SoundVar::SoundVar(const Daedalus::GEngineClasses::C_SFX &sfx, Sound &&snd)
  :snd(std::move(snd)),vol(sfx.vol/127.f){
  }

SoundFx::SoundFx(Gothic &gothic, const char* s) {
  auto& sfx = gothic.getSoundScheme(s);
  auto  snd = Resources::loadSoundBuffer(sfx.file);

  if(!snd.isEmpty())
    inst.emplace_back(sfx,std::move(snd));
  loadVariants(gothic,s);

  if(inst.size()==0)
    Log::d("unable to load sound fx: ",s);
  }

GSoundEffect SoundFx::getEffect(SoundDevice &dev) const {
  if(inst.size()==0)
    return GSoundEffect();
  auto&        var    = inst[size_t(std::rand())%inst.size()];
  GSoundEffect effect = dev.load(var.snd);
  effect.setVolume(var.vol);
  return effect;
  }

SoundEffect SoundFx::getGlobal(SoundDevice &dev) const {
  if(inst.size()==0)
    return SoundEffect();
  auto&       var    = inst[size_t(std::rand())%inst.size()];
  SoundEffect effect = dev.load(var.snd);
  effect.setVolume(var.vol);
  return effect;
  }

void SoundFx::loadVariants(Gothic &gothic, const char *s) {
  char name[256]={};
  for(int i=1;i<100;++i){
    std::snprintf(name,sizeof(name),"%s_A%02d",s,i);
    auto& sfx = gothic.getSoundScheme(name);
    auto  snd = Resources::loadSoundBuffer(sfx.file);
    if(snd.isEmpty())
      break;
    inst.emplace_back(sfx,std::move(snd));
    }
  }

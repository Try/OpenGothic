#include "soundfx.h"

#include <Tempest/Log>

#include "game/gamesession.h"
#include "gothic.h"
#include "resources.h"

using namespace Tempest;

SoundFx::SoundVar::SoundVar(const Daedalus::GEngineClasses::C_SFX &sfx, Sound &&snd)
  :snd(std::move(snd)),vol(float(sfx.vol)/127.f){
  }

SoundFx::SoundVar::SoundVar(const float vol, Sound &&snd)
  :snd(std::move(snd)),vol(vol/127.f){
  }

SoundFx::SoundFx(Gothic &gothic, const char* s) {
  implLoad(gothic,s);
  if(inst.size()!=0)
    return;
  // lowcase?
  std::string name = s;
  for(auto& i:name)
    i = char(std::toupper(i));

  implLoad(gothic,name.c_str());
  if(inst.size()!=0)
    return;

  if(name.rfind(".WAV")==name.size()-4) {
    auto snd = Resources::loadSoundBuffer(name);
    if(snd.isEmpty())
      Log::d("unable to load sound fx: ",s); else
      inst.emplace_back(1.f,std::move(snd));
    }

  if(inst.size()==0)
    Log::d("unable to load sound fx: ",s);
  }

SoundFx::SoundFx(Gothic &, Sound &&snd) {
  if(!snd.isEmpty())
    inst.emplace_back(127.f,std::move(snd));
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

void SoundFx::implLoad(Gothic &gothic, const char *s) {
  auto& sfx = gothic.getSoundScheme(s);
  auto  snd = Resources::loadSoundBuffer(sfx.file.c_str());

  if(!snd.isEmpty())
    inst.emplace_back(sfx,std::move(snd));
  loadVariants(gothic,s);

  }

void SoundFx::loadVariants(Gothic &gothic, const char *s) {
  char name[256]={};
  for(int i=1;i<100;++i){
    std::snprintf(name,sizeof(name),"%s_A%02d",s,i);
    auto& sfx = gothic.getSoundScheme(name);
    auto  snd = Resources::loadSoundBuffer(sfx.file.c_str());
    if(snd.isEmpty())
      break;
    inst.emplace_back(sfx,std::move(snd));
    }
  }

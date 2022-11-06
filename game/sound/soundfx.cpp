#include "soundfx.h"

#include <Tempest/Log>
#include <cctype>

#include "game/definitions/sounddefinitions.h"
#include "game/gamesession.h"
#include "gothic.h"

#include "resources.h"

SoundFx::SoundVar::SoundVar(const phoenix::c_sfx &sfx, Tempest::Sound &&snd)
  :snd(std::move(snd)),vol(float(sfx.vol)/127.f),loop(sfx.loop){
  }

SoundFx::SoundVar::SoundVar(const float vol, Tempest::Sound &&snd)
  :snd(std::move(snd)),vol(vol/127.f){
  }

SoundFx::SoundFx(std::string_view s) {
  implLoad(s);
  if(inst.size()!=0)
    return;
  // lowcase?
  std::string name = std::string(s);
  for(auto& i:name)
    i = char(std::toupper(i));

  implLoad(name);
  if(inst.size()!=0)
    return;

  if(name.rfind(".WAV")==name.size()-4) {
    auto snd = Resources::loadSoundBuffer(name);
    if(snd.isEmpty())
      Tempest::Log::d("unable to load sound fx: ",name); else
      inst.emplace_back(1.f,std::move(snd));
    }

  if(inst.size()==0)
    Tempest::Log::d("unable to load sound fx: ",name);
  }

SoundFx::SoundFx(Tempest::Sound &&snd) {
  if(!snd.isEmpty())
    inst.emplace_back(127.f,std::move(snd));
  }

Tempest::SoundEffect SoundFx::getEffect(Tempest::SoundDevice &dev, bool& loop) const {
  if(inst.size()==0)
    return Tempest::SoundEffect();
  auto&                var    = inst[size_t(std::rand())%inst.size()];
  Tempest::SoundEffect effect = dev.load(var.snd);
  effect.setVolume(var.vol);
  loop = var.loop;
  return effect;
  }

void SoundFx::implLoad(std::string_view s) {
  auto& sfx = Gothic::sfx()[s];
  auto  snd = Resources::loadSoundBuffer(sfx.file);

  if(!snd.isEmpty())
    inst.emplace_back(sfx,std::move(snd));
  loadVariants(s);
  }

void SoundFx::loadVariants(std::string_view s) {
  char name[256]={};
  for(int i=1;i<100;++i){
    std::snprintf(name,sizeof(name),"%.*s_A%02d",int(s.size()),s.data(),i);
    auto& sfx = Gothic::sfx()[name];
    auto  snd = Resources::loadSoundBuffer(sfx.file);
    if(snd.isEmpty())
      break;
    inst.emplace_back(sfx,std::move(snd));
    }
  }

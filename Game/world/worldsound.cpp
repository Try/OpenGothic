#include "worldsound.h"

#include <Tempest/SoundEffect>
#include "game/gamesession.h"
#include "world.h"
#include "resources.h"

const float WorldSound::maxDist   = 1600; // 16 meters
const float WorldSound::talkRange = 800;

WorldSound::WorldSound(GameSession &game, World& owner):game(game),owner(owner) {
  auto snd = Resources::loadMusic("_work/Data/Music/newworld/Gamestart.sgt");
  //auto snd = Resources::loadMusic("_work/Data/Music/newworld/owd_daystd.sgt");
  }

void WorldSound::seDefaultZone(const ZenLoad::zCVobData &vob) {
  def.bbox[0] = vob.bbox[0];
  def.bbox[1] = vob.bbox[1];
  def.name    = vob.vobName;
  // def.prefix = vob.vobName.substr(vob.vobName.find('_') + 1);
  }

void WorldSound::addZone(const ZenLoad::zCVobData &vob) {
  Zone z;
  z.bbox[0] = vob.bbox[0];
  z.bbox[1] = vob.bbox[1];
  z.name    = vob.vobName;

  zones.emplace_back(std::move(z));
  }

void WorldSound::emitSound(const char* s, float x, float y, float z, float range) {
  std::lock_guard<std::mutex> guard(sync);
  if(isInListenerRange({x,y,z})){
    auto snd = Resources::loadSoundFx(s);
    if(snd==nullptr)
      return;
    Tempest::SoundEffect eff = snd->getEffect(dev);
    if(eff.isEmpty())
      return;
    eff.setPosition(x,y,z);
    eff.setMaxDistance(maxDist);
    eff.setRefDistance(range);
    eff.play();
    effect.emplace_back(std::move(eff));
    }
  }

void WorldSound::tick(Npc &player) {
  std::lock_guard<std::mutex> guard(sync);
  plPos = player.position();

  float rot = player.rotationRad()+float(M_PI/2.0);
  float s   = std::sin(rot);
  float c   = std::cos(rot);

  dev.setListenerPosition(plPos[0],plPos[1],plPos[2]);
  dev.setListenerDirection(c,0,s, 0,1,0);

  for(size_t i=0;i<effect.size();){
    if(effect[i].isFinished()){
      effect[i]=std::move(effect.back());
      effect.pop_back();
      } else {
      ++i;
      }
    }

  const size_t sep = def.name.find('_');
  const char*  tag = def.name.c_str();
  if(sep!=std::string::npos)
    tag = tag+sep+1;

  char name[64]={};
  std::snprintf(name,sizeof(name),"%s_%s_%s",tag,"DAY","STD");
  auto& theme = game.getMusicTheme(name);

  auto snd = Resources::loadMusic("_work/Data/Music/newworld/"+theme.file);
  }

bool WorldSound::isInListenerRange(const std::array<float,3> &pos) const {
  return qDist(pos,plPos)<4*maxDist*maxDist;
  }

float WorldSound::qDist(const std::array<float,3> &a, const std::array<float,3> &b) {
  float dx=a[0]-b[0];
  float dy=a[1]-b[1];
  float dz=a[2]-b[2];
  return dx*dx+dy*dy+dz*dz;
  }

void WorldSound::aiOutput(const std::array<float,3>& pos,const std::string &outputname) {
  std::lock_guard<std::mutex> guard(sync);
  if(isInListenerRange(pos)){
    if(auto snd = Resources::loadSound(outputname+".wav"))
      snd->play();
    }
  }

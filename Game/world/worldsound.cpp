#include "worldsound.h"

#include <Tempest/SoundEffect>
#include "game/gamesession.h"
#include "world.h"
#include "resources.h"

using namespace Tempest;

const float WorldSound::maxDist   = 3500; // 35 meters
const float WorldSound::talkRange = 800;

WorldSound::WorldSound(GameSession &game, World& owner):game(game),owner(owner) {
  plPos = {{-1000000,-1000000,-1000000}};
  auto snd = Resources::loadMusic("_work/Data/Music/newworld/Gamestart.sgt");
  //auto snd = Resources::loadMusic("_work/Data/Music/newworld/owd_daystd.sgt");
  }

void WorldSound::setDefaultZone(const ZenLoad::zCVobData &vob) {
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

void WorldSound::addSound(const ZenLoad::zCVobData &vob) {
  return;

  // TODO: fix background sound
  auto& pr = vob.zCVobSound;
  auto snd = game.loadSoundFx(pr.sndName.c_str());
  if(snd==nullptr)
    return;

  WSound s{std::move(*snd)};
  s.eff = game.loadSound(s.proto);
  if(s.eff.isEmpty())
    return;

  s.eff.setPosition(vob.position.x,vob.position.y,vob.position.z);
  s.eff.setMaxDistance(maxDist);
  s.eff.setRefDistance(0);
  s.eff.setVolume(0.3f);
  s.active   = pr.sndStartOn;
  s.delay    = uint64_t(pr.sndRandDelay*1000);
  s.delayVar = uint64_t(pr.sndRandDelayVar*1000);

  worldEff.emplace_back(std::move(s));
  }

void WorldSound::emitSound(const char* s, float x, float y, float z, float range, GSoundEffect* slot) {
  if(slot && !slot->isFinished())
    return;

  if(range<=0.f)
    range = 35.f;

  std::lock_guard<std::mutex> guard(sync);
  if(isInListenerRange({x,y,z})){
    if(std::strcmp("WHOOSH",s)==0){
      }

    auto snd = game.loadSoundFx(s);
    if(snd==nullptr)
      return;
    GSoundEffect eff = game.loadSound(*snd);
    if(eff.isEmpty())
      return;
    eff.setPosition(x,y,z);
    //eff.setMaxDistance(maxDist);
    eff.setMaxDistance(range*100);
    eff.setRefDistance(0);
    eff.play();
    tickSlot(eff);
    if(slot)
      *slot = std::move(eff); else
      effect.emplace_back(std::move(eff));
    }
  }

void WorldSound::emitDlgSound(const char *s, float x, float y, float z, float range, uint64_t& timeLen) {
  std::lock_guard<std::mutex> guard(sync);

  if(isInListenerRange({x,y,z})){
    auto snd = Resources::loadSoundBuffer(s);
    if(snd.isEmpty())
      return;
    Tempest::SoundEffect eff = game.loadSound(snd);
    if(eff.isEmpty())
      return;
    eff.setPosition(x,y+180,z);
    eff.setMaxDistance(maxDist);
    eff.setRefDistance(range);
    eff.play();
    timeLen = eff.timeLength();
    effect.emplace_back(std::move(eff));
    }
  }

void WorldSound::takeSoundSlot(GSoundEffect &&eff) {
  if(eff.isFinished())
    return;
  effect.emplace_back(std::move(eff));
  }

void WorldSound::tick(Npc &player) {
  std::lock_guard<std::mutex> guard(sync);
  plPos = player.position();

  game.updateListenerPos(player);

  for(size_t i=0;i<effect.size();){
    if(effect[i].isFinished()){
      effect[i]=std::move(effect.back());
      effect.pop_back();
      } else {
      tickSlot(effect[i]);
      ++i;
      }
    }

  for(size_t i=0;i<worldEff.size();++i){
    if(worldEff[i].active && worldEff[i].eff.isFinished()){
      worldEff[i].eff.play();
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

void WorldSound::tickSlot(GSoundEffect& slot) {
  if(slot.isFinished())
    return;
  auto  dyn = owner.physic();
  auto  pos = slot.position();
  float occ = dyn->soundOclusion(plPos[0],plPos[1]+180/*head pos*/,plPos[2], pos[0],pos[1],pos[2]);

  slot.setOcclusion(std::max(0.f,1.f-occ/20.f));
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
    game.emitGlobalSound(Resources::loadSoundBuffer(outputname+".wav"));
    }
  }

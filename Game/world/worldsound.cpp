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
  }

void WorldSound::setDefaultZone(const ZenLoad::zCVobData &vob) {
  def.bbox[0] = vob.bbox[0];
  def.bbox[1] = vob.bbox[1];
  def.name    = vob.vobName;
  }

void WorldSound::addZone(const ZenLoad::zCVobData &vob) {
  Zone z;
  z.bbox[0] = vob.bbox[0];
  z.bbox[1] = vob.bbox[1];
  z.name    = vob.vobName;

  zones.emplace_back(std::move(z));
  }

void WorldSound::addSound(const ZenLoad::zCVobData &vob) {
  auto& pr  = vob.zCVobSound;
  auto  snd = game.loadSoundFx(pr.sndName.c_str());
  if(snd==nullptr)
    return;

  WSound s{std::move(*snd)};
  s.eff = game.loadSound(s.proto);
  if(s.eff.isEmpty())
    return;

  s.eff.setPosition(vob.position.x,vob.position.y,vob.position.z);
  s.eff.setMaxDistance(pr.sndRadius);
  s.eff.setRefDistance(0);
  s.eff.setVolume(0.5f);

  s.loop     = pr.sndType==ZenLoad::SoundMode::SM_LOOPING;
  s.active   = pr.sndStartOn;
  s.delay    = uint64_t(pr.sndRandDelay*1000);
  s.delayVar = uint64_t(pr.sndRandDelayVar*1000);

  if(vob.vobType==ZenLoad::zCVobData::VT_zCVobSoundDaytime) {
    float b = vob.zCVobSoundDaytime.sndStartTime;
    float e = vob.zCVobSoundDaytime.sndEndTime;

    s.sndStart = gtime(int(b),int(b*60)%60);
    s.sndEnd   = gtime(int(e),int(e*60)%60);

    s.eff2 = game.loadSound(s.proto);
    s.eff2.setPosition(vob.position.x,vob.position.y,vob.position.z);
    s.eff2.setMaxDistance(pr.sndRadius);
    s.eff2.setRefDistance(0);
    s.eff2.setVolume(0.5f);
    } else {
    s.sndStart = gtime(0,0);
    s.sndEnd   = gtime(24,0);
    }

  worldEff.emplace_back(std::move(s));
  }

void WorldSound::emitSound(const char* s, float x, float y, float z, float range, bool fSlot) {
  if(range<=0.f)
    range = 3500.f;

  GSoundEffect* slot = nullptr;
  std::lock_guard<std::mutex> guard(sync);
  if(isInListenerRange({x,y,z})) {
    if(fSlot)
      slot = &freeSlot[s];
    if(slot!=nullptr && !slot->isFinished())
      return;
    auto snd = game.loadSoundFx(s);
    if(snd==nullptr)
      return;
    GSoundEffect eff = game.loadSound(*snd);
    if(eff.isEmpty())
      return;
    eff.setPosition(x,y,z);
    eff.setMaxDistance(maxDist);
    eff.setRefDistance(range);
    eff.play();
    tickSlot(eff);
    if(slot)
      *slot = std::move(eff); else
      effect.emplace_back(std::move(eff));
    }
  }

void WorldSound::emitSoundRaw(const char *s, float x, float y, float z, float range, bool fSlot) {
  if(range<=0.f)
    range = 3500.f;

  std::lock_guard<std::mutex> guard(sync);
  GSoundEffect* slot = nullptr;
  if(isInListenerRange({x,y,z})){
    if(fSlot)
      slot = &freeSlot[s];
    if(slot!=nullptr && !slot->isFinished())
      return;
    auto snd = game.loadSoundWavFx(s);
    if(snd==nullptr)
      return;
    GSoundEffect eff = game.loadSound(*snd);
    if(eff.isEmpty())
      return;
    eff.setPosition(x,y,z);
    eff.setMaxDistance(maxDist);
    eff.setRefDistance(range);
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

  for(size_t i=0;i<effect.size();) {
    if(effect[i].isFinished()){
      effect[i]=std::move(effect.back());
      effect.pop_back();
      } else {
      tickSlot(effect[i]);
      ++i;
      }
    }

  for(auto& i:freeSlot) {
    tickSlot(i.second);
    }

  for(auto& i:worldEff) {
    if(i.active && i.eff.isFinished() && (i.restartTimeout<owner.tickCount() || i.loop)){
      if(i.restartTimeout!=0) {
        auto time = owner.time();
        time = gtime(0,time.hour(),time.minute());
        if(i.sndStart<= time && time<i.sndEnd){
          i.eff.play();
          } else {
          if(!i.eff2.isEmpty())
            i.eff2.play();
          }
        }
      i.restartTimeout = owner.tickCount() + i.delay;
      if(i.delayVar>0)
        i.restartTimeout += uint64_t(std::rand())%i.delayVar;
      }
    }

  Zone* zone=&def;
  if(currentZone!=nullptr &&
     currentZone->bbox[0].x <= plPos[0] && plPos[0]<currentZone->bbox[1].x &&
     currentZone->bbox[0].y <= plPos[1] && plPos[1]<currentZone->bbox[1].y &&
     currentZone->bbox[0].z <= plPos[2] && plPos[2]<currentZone->bbox[1].z){
    zone = currentZone;
    } else {
    for(auto& z:zones) {
      if(z.bbox[0].x <= plPos[0] && plPos[0]<z.bbox[1].x &&
         z.bbox[0].y <= plPos[1] && plPos[1]<z.bbox[1].y &&
         z.bbox[0].z <= plPos[2] && plPos[2]<z.bbox[1].z) {
        zone = &z;
        }
      }
    }

  currentZone = zone;
  const size_t sep = zone->name.find('_');
  const char*  tag = zone->name.c_str();
  if(sep!=std::string::npos)
    tag = tag+sep+1;

  char name[64]={};
  std::snprintf(name,sizeof(name),"%s_%s_%s",tag,"DAY","STD");
  game.setMusic(name);
  }

void WorldSound::tickSlot(GSoundEffect& slot) {
  if(slot.isFinished())
    return;
  auto  dyn = owner.physic();
  auto  pos = slot.position();
  float occ = dyn->soundOclusion(plPos[0],plPos[1]+180/*head pos*/,plPos[2], pos[0],pos[1],pos[2]);

  slot.setOcclusion(std::max(0.f,1.f-occ));
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

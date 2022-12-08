#include "worldsound.h"

#include <Tempest/SoundEffect>

#include "camera.h"
#include "game/definitions/musicdefinitions.h"
#include "game/gamesession.h"
#include "world/objects/npc.h"
#include "world/objects/sound.h"
#include "sound/soundfx.h"
#include "utils/string_frm.h"
#include "world.h"
#include "gamemusic.h"
#include "gothic.h"
#include "resources.h"

const float WorldSound::maxDist   = 3500; // 35 meters
const float WorldSound::talkRange = 800;

struct WorldSound::WSound final {
  Sound          current;
  const SoundFx* eff0 = nullptr;
  const SoundFx* eff1 = nullptr;

  Tempest::Vec3  pos;
  float          sndRadius = 25;

  bool           loop          =false;
  bool           active        =false;
  uint64_t       delay         =0;
  uint64_t       delayVar      =0;
  uint64_t       restartTimeout=0;

  gtime          sndStart;
  gtime          sndEnd;
  };

struct WorldSound::Zone final {
  Tempest::Vec3 bbox[2]={};
  std::string   name;
  bool          checkPos(float x,float y,float z) const {
    return
        bbox[0].x <= x && x<bbox[1].x &&
        bbox[0].y <= y && y<bbox[1].y &&
        bbox[0].z <= z && z<bbox[1].z;
    }
  };

void WorldSound::Effect::setOcclusion(float v) {
  occ = v;
  eff.setVolume(occ*vol);
  }

void WorldSound::Effect::setVolume(float v) {
  vol = v;
  eff.setVolume(occ*vol);
  }

WorldSound::WorldSound(GameSession &game, World& owner)
  :game(game), owner(owner) {
  plPos = {-1000000,-1000000,-1000000};
  effect.reserve(256);
  }

WorldSound::~WorldSound() {
  }

void WorldSound::setDefaultZone(const phoenix::vobs::zone_music &vob) {
  def.reset(new Zone());
  def->bbox[0] = {vob.bbox.min.x, vob.bbox.min.y, vob.bbox.min.z};
  def->bbox[1] = {vob.bbox.max.x, vob.bbox.max.y, vob.bbox.max.z};
  def->name    = vob.vob_name;
  }

void WorldSound::addZone(const phoenix::vobs::zone_music &vob) {
  Zone z;
  z.bbox[0] = {vob.bbox.min.x, vob.bbox.min.y, vob.bbox.min.z};
  z.bbox[1] = {vob.bbox.max.x, vob.bbox.max.y, vob.bbox.max.z};
  z.name    = vob.vob_name;

  zones.emplace_back(std::move(z));
  }

void WorldSound::addSound(const phoenix::vobs::sound &vob) {
  WSound s;
  s.loop      = vob.mode==phoenix::sound_mode::loop;
  s.active    = vob.initially_playing;
  s.delay     = uint64_t(vob.random_delay * 1000);
  s.delayVar  = uint64_t(vob.random_delay_var * 1000);
  s.eff0      = Gothic::inst().loadSoundFx(vob.sound_name);

  s.pos       = {vob.position.x,vob.position.y,vob.position.z};
  s.sndRadius = vob.radius;

  if(vob.type==phoenix::vob_type::zCVobSoundDaytime) {
    auto& prDay = (const phoenix::vobs::sound_daytime&) vob;
    float b     = prDay.start_time;
    float e     = prDay.end_time;

    s.sndStart = gtime(int(b),int(b*60)%60);
    s.sndEnd   = gtime(int(e),int(e*60)%60);
    s.eff1     = Gothic::inst().loadSoundFx(prDay.sound_name2);
    } else {
    s.sndStart = gtime(0,0);
    s.sndEnd   = gtime(24,0);
    }

  worldEff.emplace_back(std::move(s));
  }

Sound WorldSound::addDlgSound(std::string_view s, const Tempest::Vec3& pos, float range, uint64_t& timeLen) {
  if(!isInListenerRange(pos,range))
    return Sound();
  auto snd = Resources::loadSoundBuffer(s);
  if(snd.isEmpty())
    return Sound();

  auto ret = implAddSound(game.loadSound(snd), pos,range);
  if(ret.isEmpty())
    return Sound();

  std::lock_guard<std::mutex> guard(sync);
  initSlot(*ret.val);
  timeLen = snd.timeLength();
  effect.emplace_back(ret.val);
  return ret;
  }

Sound WorldSound::implAddSound(const SoundFx& eff, const Tempest::Vec3& pos, float rangeMax) {
  bool loop = false;
  auto ret = implAddSound(game.loadSound(eff,loop), pos,rangeMax);
  ret.setLooping(loop);
  return ret;
  }

Sound WorldSound::implAddSound(Tempest::SoundEffect&& eff, const Tempest::Vec3& pos, float rangeMax) {
  if(eff.isEmpty())
    return Sound();
  auto ex = std::make_shared<Effect>();
  eff.setPosition(pos);
  eff.setMaxDistance(rangeMax);

  ex->eff     = std::move(eff);
  ex->pos     = pos;
  ex->vol     = ex->eff.volume();
  ex->maxDist = rangeMax;
  ex->setOcclusion(0);

  return Sound(ex);
  }

void WorldSound::tick(Npc& player) {
  std::lock_guard<std::mutex> guard(sync);

  auto cx = game.camera().listenerPosition();
  plPos = cx.pos;

  game.updateListenerPos(cx);

  for(auto& i:worldEff) {
    if(!i.active || !i.current.isFinished())
      continue;
    if(i.current.isFinished())
      i.current = Sound();

    if(i.restartTimeout>owner.tickCount() && !i.loop)
      continue;

    if(!isInListenerRange(i.pos,i.sndRadius))
      continue;

    auto time = owner.time();
    time = gtime(0,time.hour(),time.minute());

    const SoundFx* snd = nullptr;
    if(i.sndStart<= time && time<i.sndEnd) {
      snd = i.eff0;
      } else {
      snd = i.eff1;
      }

    if(snd==nullptr)
      continue;

    i.current = implAddSound(*snd,i.pos,i.sndRadius);
    if(!i.current.isEmpty()) {
      effect.emplace_back(i.current.val);
      i.current.play();
      }

    i.restartTimeout = owner.tickCount() + i.delay;
    if(i.delayVar>0)
      i.restartTimeout += uint64_t(std::rand())%i.delayVar;
    }

  tickSlot(effect);
  tickSlot(effect3d);
  for(auto& i:freeSlot)
    tickSlot(*i.second);
  tickSoundZone(player);
  }

void WorldSound::tickSoundZone(Npc& player) {
  if(owner.tickCount()<nextSoundUpdate)
    return;
  nextSoundUpdate = owner.tickCount()+5*1000;

  Zone* zone = def.get();
  if(currentZone!=nullptr &&
     currentZone->checkPos(plPos.x,plPos.y+player.translateY(),plPos.z)){
    zone = currentZone;
    } else {
    for(auto& z:zones) {
      if(z.checkPos(plPos.x,plPos.y+player.translateY(),plPos.z)) {
        zone = &z;
        }
      }
    }

  gtime           time  = owner.time().timeInDay();
  bool            isDay = (gtime(4,0)<=time && time<=gtime(21,0));
  bool            isFgt = owner.isTargeted(player);

  GameMusic::Tags mode  = GameMusic::Std;
  if(isFgt) {
    if(player.weaponState()==WeaponState::NoWeapon) {
      mode  = GameMusic::Thr;
      } else {
      mode = GameMusic::Fgt;
      }
    }
  GameMusic::Tags tags = GameMusic::mkTags(isDay ? GameMusic::Day : GameMusic::Ngt,mode);

  if(currentZone==zone && currentTags==tags)
    return;

  currentZone = zone;
  currentTags = tags;

  Zone*           zTry[]    = {zone, def.get()};
  GameMusic::Tags dayTry[]  = {isDay ? GameMusic::Day : GameMusic::Ngt, GameMusic::Day};
  GameMusic::Tags modeTry[] = {mode, mode==GameMusic::Thr ? GameMusic::Fgt : GameMusic::Std, GameMusic::Std};

  // multi-fallback strategy
  for(auto zone:zTry)
    for(auto day:dayTry)
      for(auto mode:modeTry) {
        const size_t sep = zone->name.find('_');
        const char*  tag = zone->name.c_str();
        if(sep!=std::string::npos)
          tag = tag+sep+1;

        tags = GameMusic::mkTags(day,mode);
        if(setMusic(tag,tags))
          return;
        }
  }

void WorldSound::tickSlot(std::vector<PEffect>& effect) {
  for(size_t i=0;i<effect.size();) {
    auto& e = *effect[i];
    if(e.eff.isFinished() && !(e.loop && e.active)){
      effect[i]=std::move(effect.back());
      effect.pop_back();
      } else {
      ++i;
      }
    }
  for(auto& i:effect) {
    tickSlot(*i);
    }
  }

void WorldSound::tickSlot(Effect& slot) {
  if(slot.eff.isFinished()) {
    if(!slot.loop)
      return;
    slot.eff.play();
    }

  if(slot.ambient) {
    slot.setOcclusion(1.f);
    } else {
    auto  dyn  = owner.physic();
    auto  head = plPos;
    auto  pos  = slot.pos;
    float occ  = 1;

    if((pos-head).quadLength()<slot.maxDist*slot.maxDist)
      occ = dyn->soundOclusion(head, pos);
    slot.setOcclusion(std::max(0.f,1.f-occ));
    }
  }

void WorldSound::initSlot(WorldSound::Effect& slot) {
  auto  dyn = owner.physic();
  auto  pos = slot.pos;
  float occ = dyn->soundOclusion(plPos, pos);
  slot.setOcclusion(std::max(0.f,1.f-occ));
  }

bool WorldSound::setMusic(std::string_view zone, GameMusic::Tags tags) {
  bool             isDay = (tags&GameMusic::Ngt)==0;
  std::string_view smode = "STD";
  if(tags&GameMusic::Thr)
    smode = "THR";
  if(tags&GameMusic::Fgt)
    smode = "FGT";

  string_frm name(zone,'_',(isDay ? "DAY" : "NGT"),'_',smode);
  if(auto* theme = Gothic::musicDef()[name]) {
    GameMusic::inst().setMusic(*theme,tags);
    return true;
    }
  return false;
  }

bool WorldSound::isInListenerRange(const Tempest::Vec3& pos, float sndRgn) const {
  float dist = sndRgn+800;
  return (pos-plPos).quadLength()<dist*dist;
  }

bool WorldSound::canSeeSource(const Tempest::Vec3& p) const {
  auto dyn = owner.physic();
  for(auto& i:effect3d) {
    auto rc = dyn->ray(p, i->pos);
    if(!rc.hasCol)
      return true;
    }
  return false;
  }

void WorldSound::aiOutput(const Tempest::Vec3& pos, std::string_view outputname) {
  if(isInListenerRange(pos,talkRange)){
    std::lock_guard<std::mutex> guard(sync);
    Gothic::inst().emitGlobalSound(Resources::loadSoundBuffer(string_frm(outputname,".wav")));
    }
  }

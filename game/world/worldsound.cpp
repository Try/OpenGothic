#include "worldsound.h"

#include <Tempest/SoundEffect>

#include "camera.h"
#include "game/definitions/musicdefinitions.h"
#include "game/gamesession.h"
#include "world/triggers/abstracttrigger.h"
#include "world/objects/npc.h"
#include "world/objects/sound.h"
#include "sound/soundfx.h"
#include "utils/string_frm.h"
#include "world.h"
#include "gamemusic.h"
#include "gothic.h"
#include "resources.h"

const float WorldSound::maxDist   = 7000; // 70 meters
const float WorldSound::talkRange = 2000;

struct WorldSound::WSound final {
  Sound          current;
  const SoundFx* eff0 = nullptr;
  const SoundFx* eff1 = nullptr;

  std::string    vobName;
  Tempest::Vec3  pos;
  float          sndRadius      = 2500;

  bool           loop           = false;
  bool           active         = false;
  uint64_t       delay          = 0;
  uint64_t       delayVar       = 0;
  uint64_t       restartTimeout = 0;

  gtime          sndStart;
  gtime          sndEnd;
  };

struct WorldSound::Zone final {
  Tempest::Vec3 bbox[2]={};
  std::string   name;
  int32_t       priority=0;   // higher wins when zones overlap (see #zone-priority)
  bool          checkPos(const Tempest::Vec3& v) const { return checkPos(v.x, v.y, v.z); }
  bool          checkPos(float x, float y, float z) const {
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

void WorldSound::setDefaultZone(const zenkit::VZoneMusic &vob) {
  def.reset(new Zone());
  def->bbox[0] = {vob.bbox.min.x, vob.bbox.min.y, vob.bbox.min.z};
  def->bbox[1] = {vob.bbox.max.x, vob.bbox.max.y, vob.bbox.max.z};
  def->name    = vob.vob_name;
  def->priority = vob.priority;
  }

void WorldSound::addZone(const zenkit::VZoneMusic &vob) {
  Zone z;
  z.bbox[0] = {vob.bbox.min.x, vob.bbox.min.y, vob.bbox.min.z};
  z.bbox[1] = {vob.bbox.max.x, vob.bbox.max.y, vob.bbox.max.z};
  z.name    = vob.vob_name;
  z.priority = vob.priority;

  zones.emplace_back(std::move(z));
  }

void WorldSound::addSound(const zenkit::VSound &vob) {
  WSound s;
  s.vobName   = vob.vob_name;
  s.loop      = vob.mode==zenkit::SoundMode::LOOP;
  s.active    = vob.initially_playing;
  s.delay     = uint64_t(vob.random_delay * 1000);
  s.delayVar  = uint64_t(vob.random_delay_var * 1000);
  s.eff0      = Gothic::inst().loadSoundFx(vob.sound_name);

  s.pos       = {vob.position.x,vob.position.y,vob.position.z};
  s.sndRadius = vob.radius;

  if(vob.type==zenkit::VirtualObjectType::zCVobSoundDaytime) {
    auto& prDay = (const zenkit::VSoundDaytime&) vob;
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

  auto ret = implAddSound(game.loadSound(snd), pos, range);
  if(ret.isEmpty())
    return Sound();

  //WA for https://github.com/Try/OpenGothic/issues/922
  static float fixupMultiplyer = 2.f;

  std::lock_guard<std::mutex> guard(sync);
  initSlot(*ret.val);
  ret.setVolume(fixupMultiplyer);
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

  return Sound(ex, pos);
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

    // NOTE: in original-game zCVobSoundDaytime::DoSoundUpdate (Gothic2.exe 0x0063ef50) the
    // daytime window is treated modulo 24h: a window whose start is later than its end
    // (e.g. 20:00..06:00) wraps across midnight and is active across midnight. A plain
    // start<=t<end test is never true for such a window, so OpenGothic played the secondary
    // sound all day and never the intended windowed primary sound.
    const bool inWindow = (i.sndStart<=i.sndEnd)
                            ? (i.sndStart<=time && time<i.sndEnd)
                            : (i.sndStart<=time || time<i.sndEnd);
    const SoundFx* snd = inWindow ? i.eff0 : i.eff1;

    if(snd==nullptr)
      continue;

    i.current = implAddSound(*snd,i.pos,i.sndRadius);
    if(!i.current.isEmpty()) {
      effect.emplace_back(i.current.val);
      i.current.play();
      }

    // NOTE: in original-game zCVobSound::DoSoundUpdate (Gothic2.exe 0x0063e210) the next random
    // delay is drawn symmetrically from [delay-delayVar, delay+delayVar] (rand() remapped to
    // [-1,+1] scaled by delayVar), mean == delay. OpenGothic used a one-sided rand()%delayVar ->
    // [delay, delay+delayVar), biasing every ambient repeat ~delayVar/2 ms late and halving the
    // spread.
    int64_t next = int64_t(i.delay);
    if(i.delayVar>0) {
      const double r = 2.0*double(std::rand())/double(RAND_MAX) - 1.0; // [-1,+1]
      next += int64_t(r*double(i.delayVar));
      }
    if(next<0)
      next = 0;
    i.restartTimeout = owner.tickCount() + uint64_t(next);

    if(!i.loop)
      i.active = false;
    }

  tickSlot(effect);
  tickSlot(effect3d);
  for(auto& i:freeSlot)
    tickSlot(*i.second);
  tickSoundZone(player);
  }

bool WorldSound::execTriggerEvent(const TriggerEvent& e) {
  bool emitted=false;
  for(auto& i:worldEff)
    if(i.vobName==e.target) {
      i.active = true;
      emitted = true;
      }
  return emitted;
  }

void WorldSound::tickSoundZone(Npc& player) {
  if(owner.tickCount()<nextSoundUpdate)
    return;
  nextSoundUpdate = owner.tickCount()+5*1000;

  // NOTE: in original-game oCZoneMusic::BuildTempZoneList (Gothic2.exe 0x00641530) builds
  // the active-zone list sorted by priority and uses the highest-priority overlapping zone
  // (GetPriority @0x006410b0; higher value wins, camera-weight as tiebreak), falling back to
  // the default zone only when inside none. OpenGothic took the last bbox match in vector
  // order, so overlapping zones (e.g. a city district nested in a region) picked the wrong
  // theme. Select the highest-priority containing zone.
  Zone*   zone     = def.get();
  int32_t bestPrio = -1;
  for(auto& z:zones) {
    if(z.checkPos(plPos) && z.priority>bestPrio) {
      zone     = &z;
      bestPrio = z.priority;
      }
    }

  gtime time  = owner.time().timeInDay();
  // NOTE: in original-game oCWorldTimer::IsDay (Gothic2.exe 0x00781280, via
  // oCZoneMusic::IsDaytime 0x00642400) treats 06:30-18:30 as day for music DAY/NGT variant
  // selection. The 04:00-21:00 window played the daytime theme ~5h/day too widely.
  bool  isDay = (gtime(6,30)<=time && time<gtime(18,30));
  bool  isFgt = owner.isTargeted(player) || player.isDead();

  GameMusic::Tags mode = GameMusic::Std;
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

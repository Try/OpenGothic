#include "animation.h"

#include <Tempest/Log>
#include <cctype>

#include "world/objects/npc.h"
#include "world/objects/interactive.h"
#include "world/world.h"

#include "utils/string_frm.h"
#include "resources.h"

using namespace Tempest;

static void setupTime(std::vector<uint64_t>& t0,const std::vector<int32_t>& inp,float fps){
  t0.resize(inp.size());
  for(size_t i=0;i<inp.size();++i){
    t0[i] = uint64_t(float(inp[i])*1000.f/fps);
    }
  }

static uint64_t frameClamp(int32_t frame,uint32_t first,uint32_t numFrames,uint32_t last) {
  if(frame<int(first))
    return 0;
  if(frame>=int(first+numFrames))
    return numFrames-1; //workaround for gin, water, milk
  if(frame>int(last))
    return last-first;
  return uint64_t(frame)-first;
  }

Animation::Animation(zenkit::ModelScript& p, std::string_view name, const bool ignoreErrChunks) {
  ref = std::move(p.aliases);

  for(auto& ani : p.animations) {
    auto& data = loadMAN(ani, std::string(name) + '-' + ani.name + ".MAN");
    data.data->sfx = std::move(ani.sfx);
    data.data->gfx = std::move(ani.sfx_ground);
    data.data->events = std::move(ani.events);
    data.data->mmStartAni = std::move(ani.morph);

    data.data->pfx.resize(ani.pfx.size() + ani.pfx_stop.size());
    for(size_t i=0; i<ani.pfx.size(); ++i) {
      static_cast<zenkit::MdsParticleEffect&>(data.data->pfx[i]) = std::move(ani.pfx[i]);
      }
    for(size_t i=0; i<ani.pfx_stop.size(); ++i) {
      auto& p = data.data->pfx[i + ani.pfx.size()];
      p.index = ani.pfx_stop[i].index;
      p.frame = ani.pfx_stop[i].frame;
      p.pfxStop = true;
      }

    std::stable_sort(data.data->pfx.begin(), data.data->pfx.end(), [](const MdsParticleEffect& l, const MdsParticleEffect& r) {
      return l.frame < r.frame;
      });
    }

  for(auto& co : p.combinations) {
    string_frm name(co.model, 1+(co.last_frame-1)/2);

    bool found=false;
    for(size_t r=0;r<sequences.size();++r) { // reverse search: expect to find animations right before aniComb
      auto& i = sequences[sequences.size()-r-1];
      if(i.askName==name) {
        auto d = i.data;
        sequences.emplace_back();
        Animation::Sequence& ani = sequences.back();
        ani.name     = co.name;
        ani.askName  = co.model;
        ani.layer    = co.layer;
        ani.flags    = co.flags;
        ani.blendIn  = uint64_t(1000*co.blend_in);
        ani.blendOut = uint64_t(1000*co.blend_out);
        ani.next     = std::move(co.next);
        ani.data     = d; // set first as default
        ani.comb.resize(size_t(co.last_frame));
        found=true;
        break;
        }
      }

    if(!found) {
      Log::d("comb not found: ", co.name," -> ", co.model, "(", name, ")"); // error
      }
    }

  mesh = std::move(p.meshes);
  meshDef = std::move(p.skeleton);

  setupIndex();
  }

const Animation::Sequence* Animation::sequence(std::string_view name) const {
  if(sqHot!=nullptr && sqHot->name==name)
    return sqHot;
  auto it = std::lower_bound(sequences.begin(),sequences.end(),name,[](const Sequence& s,std::string_view n){
    return s.name<n;
    });

  if(it!=sequences.end() && it->name==name) {
    sqHot = &(*it);
    return &(*it);
    }
  return nullptr;
  }

const Animation::Sequence *Animation::sequenceAsc(std::string_view name) const {
  for(auto& i:sequences)
    if(i.askName==name)
      return &i;
  return nullptr;
  }

void Animation::debug() const {
  for(auto& i:sequences)
    Log::d(i.name);
  }

std::string_view Animation::defaultMesh() const {
  if(!meshDef.name.empty() && !meshDef.disable_mesh)
    return meshDef.name;
  return "";
  }

Animation::Sequence& Animation::loadMAN(const zenkit::MdsAnimation& hdr, std::string_view name) {
  sequences.emplace_back(hdr,name);
  auto& ret = sequences.back();
  if(ret.data==nullptr) {
    ret.data = std::make_shared<AnimData>();
    Log::e("unable to load animation sequence: \"",name,"\"");
    }
  return ret;
  }

void Animation::setupIndex() {
  for(auto& sq:sequences)
    sq.data->setupEvents(sq.data->fpsRate);

  for(auto& r:ref) {
    Sequence ani;
    for(auto& s:sequences)
      if(s.askName==r.alias)
        ani.data = s.data;

    if(ani.data==nullptr) {
      Log::d("alias not found: ",r.name," -> ",r.alias);
      continue;
      }

    ani.name     = r.name;
    ani.layer    = r.layer;
    ani.flags    = r.flags;
    ani.blendIn  = uint64_t(1000 * r.blend_in);
    ani.blendOut = uint64_t(1000 * r.blend_out);
    ani.reverse  = r.direction != zenkit::AnimationDirection::FORWARD;
    ani.next     = r.next;
    sequences.emplace_back(std::move(ani));
    }
  ref.clear();

  for(auto& sq:sequences) {
    for(auto& i:sq.name)
      i = char(std::toupper(i));
    for(auto& i:sq.next)
      i = char(std::toupper(i));
    }

  std::sort(sequences.begin(),sequences.end(),[](const Sequence& a,const Sequence& b){
    return a.name<b.name;
    });

  for(auto& s:sequences) {
    if(s.comb.size()==0)
      continue;
    for(size_t i=0;i<s.comb.size();++i) {
      string_frm name(s.askName, int(i+1));
      s.comb[i] = sequenceAsc(name);
      }
    }

  for(auto& i:sequences) {
    if((i.next==i.askName && !i.next.empty()) || i.next==i.name)
      i.animCls = Loop;
    if(!i.data->defWindow.empty()) {
      i.animCls = Transition;
      i.next    = "";
      }

    if(i.name.find("S_")==0)
      i.shortName = &i.name[2];
    if(i.name=="T_1HRUN_2_1H"   || i.name=="T_BOWRUN_2_BOW" ||
       i.name=="T_2HRUN_2_2H"   || i.name=="T_CBOWRUN_2_CBOW" ||
       i.name=="T_MAGRUN_2_MAG" || i.name=="T_FISTRUN_2_FIST")
      i.next = "";
    }

  for(auto& i:sequences) {
    i.nextPtr = sequence(i.next);
    i.owner   = this;
    }
  // for(auto& i:sequences)
  //   Log::i(i.name);
  }


Animation::Sequence::Sequence(const zenkit::MdsAnimation& hdr, std::string_view fname) {
  const auto* entry = Resources::vdfsIndex().find(fname);
  if(entry==nullptr)
    return;

  auto reader = entry->open_read();
  zenkit::ModelAnimation p;
  p.load(reader.get());

  data = std::make_shared<AnimData>();
  askName    = hdr.name;
  layer      = hdr.layer;
  flags      = hdr.flags;
  blendIn    = uint64_t(1000*hdr.blend_in);
  blendOut   = uint64_t(1000*hdr.blend_out);
  next       = hdr.next;
  reverse    = hdr.direction != zenkit::AnimationDirection::FORWARD;

  data->firstFrame = uint32_t(hdr.first_frame);
  data->lastFrame  = uint32_t(hdr.last_frame);


  name  = p.name;
  layer = p.layer;

  data->fpsRate   = p.fps;
  data->numFrames = p.frame_count;
  data->nodeIndex = p.node_indices;
  data->samples   = p.samples;

  data->setupMoveTr();
  }

bool Animation::Sequence::isFinished(uint64_t now, uint64_t sTime, uint16_t comboLen) const {
  const uint64_t t = now-sTime;
  if(comboLen<data->defHitEnd.size()) {
    if(t>data->defHitEnd[comboLen])
      return true;
    }
  return float(t)>totalTime();
  }

float Animation::Sequence::totalTime() const {
  return float(data->numFrames)*1000.f/data->fpsRate;
  }

float Animation::Sequence::atkTotalTime(uint16_t comboLen) const {
  if(comboLen<data->defHitEnd.size()) {
    uint64_t time = data->defHitEnd[comboLen];
    return float(time);
    }
  return totalTime();
  }

bool Animation::Sequence::canInterrupt(uint64_t now, uint64_t sTime, uint16_t comboLen) const {
  const uint16_t cId = uint16_t(comboLen*2u);
  if(animCls==Animation::Transition) {
    if(size_t(cId+1)<data->defWindow.size() && data->defWindow.size()>0 && !data->defHitEnd.empty()) {
      return !((now-sTime)<=data->defWindow[cId+0]);
      }
    return false;
    }

  if(size_t(cId+1)<data->defWindow.size() && isInComboWindow(now-sTime,comboLen)) {
    return false;
    }
  return true;
  }

bool Animation::Sequence::isInComboWindow(uint64_t t, uint16_t comboLen) const {
  uint16_t id = uint16_t(comboLen*2u);
  return (data->defWindow[id+0]<t && t<=data->defWindow[id+1]);
  }

bool Animation::Sequence::isDefParWindow(uint64_t t) const {
  if(data->defParFrame.size()!=2)
    return false;
  return data->defParFrame[0]<=t && t<data->defParFrame[1];
  }

bool Animation::Sequence::isDefWindow(uint64_t t) const {
  for(size_t i=0;i+1<data->defWindow.size();i+=2) {
    if(data->defWindow[i+0]<=t && t<data->defWindow[i+1])
      return true;
    }
  return false;
  }

bool Animation::Sequence::isAttackAnim() const {
  for(auto& e:data->events)
    if(e.type == zenkit::MdsEventType::OPTIMAL_FRAME)
      return true;
  return false;
  }

bool Animation::Sequence::isPrehit(uint64_t sTime, uint64_t now) const {
  auto& d         = *data;
  auto  numFrames = d.numFrames;
  if(numFrames==0)
    return false;

  int64_t frame = int64_t(float(now-sTime)*d.fpsRate/1000.f);

  for(auto& e:data->events)
    if(e.type == zenkit::MdsEventType::OPTIMAL_FRAME)
      for(auto i : e.frames)
        if(int64_t(i) > frame)
          return true;
  return false;
  }

bool Animation::Sequence::extractFrames(uint64_t& frameA,uint64_t& frameB,bool& invert,uint64_t barrier, uint64_t sTime, uint64_t now) const {
  auto& d         = *data;
  auto  numFrames = d.numFrames;
  if(numFrames==0)
    return false;

  float fpsRate = d.fpsRate;
  frameA  = uint64_t(float(barrier-sTime)*fpsRate/1000.f);
  frameB  = uint64_t(float(now    -sTime)*fpsRate/1000.f);

  if(frameA==frameB)
    return false;

  if(animCls==Animation::Loop){
    frameA%=numFrames;
    frameB%=numFrames;
    } else {
    frameA = std::min<uint64_t>(frameA,numFrames);
    frameB = std::min<uint64_t>(frameB,numFrames);
    }

  if(reverse) {
    frameA = numFrames-frameA;
    frameB = numFrames-frameB;
    std::swap(frameA,frameB);
    }

  invert = (frameB<frameA);
  if(invert)
    std::swap(frameA,frameB);
  return true;
  }

void Animation::Sequence::processSfx(uint64_t barrier, uint64_t sTime, uint64_t now, Npc* npc, Interactive* mob) const {
  uint64_t frameA=0,frameB=0;
  bool     invert=false;
  if(!extractFrames(frameA,frameB,invert,barrier,sTime,now))
    return;

  auto& d = *data;
  for(auto& i:d.sfx) {
    uint64_t fr = frameClamp(i.frame,d.firstFrame,d.numFrames,d.lastFrame);
    if(((frameA<=fr && fr<frameB) ^ invert) || i.frame==int32_t(d.lastFrame)) {
      if(npc!=nullptr)
        npc->emitSoundEffect(i.name,i.range,i.empty_slot);
      if(mob!=nullptr)
        mob->emitSoundEffect(i.name,i.range,i.empty_slot);
      }
    }
  if(npc!=nullptr && !npc->isInAir()) {
    for(auto& i:d.gfx){
      uint64_t fr = frameClamp(i.frame,d.firstFrame,d.numFrames,d.lastFrame);
      if((frameA<=fr && fr<frameB) ^ invert)
        npc->emitSoundGround(i.name, i.range, i.empty_slot);
      }
    }
  }

void Animation::Sequence::processPfx(uint64_t barrier, uint64_t sTime, uint64_t now, MdlVisual& visual, World& world) const {
  if(data->pfx.empty())
    return;

  uint64_t frameA=0,frameB=0;
  bool     invert=false;
  if(!extractFrames(frameA,frameB,invert,barrier,sTime,now))
    return;

  auto& d = *data;
  if(invert) {
    //NOTE: carefull with ordering, in new loop:
    //  first finish event of current frame, then start events of the next one
    for(auto& i:d.pfx) {
      uint64_t fr = frameClamp(i.frame,d.firstFrame,d.numFrames,d.lastFrame);
      if(!(fr<frameB))
        processPfx(i, visual, world);
      }
    for(auto& i:d.pfx) {
      uint64_t fr = frameClamp(i.frame,d.firstFrame,d.numFrames,d.lastFrame);
      if(!(frameA<=fr))
        processPfx(i, visual, world);
      }
    } else {
    for(auto& i:d.pfx) {
      uint64_t fr = frameClamp(i.frame,d.firstFrame,d.numFrames,d.lastFrame);
      if(frameA<=fr && fr<frameB)
        processPfx(i, visual, world);
      }
    }
  }

void Animation::Sequence::processPfx(const MdsParticleEffect& p, MdlVisual& visual, World& world) const {
  if(p.pfxStop) {
    visual.stopEffect(p.index);
    }
  else if(!p.name.empty()) {
    Effect e(PfxEmitter(world,p.name),p.position);
    e.setActive(true);
    visual.startEffect(world,std::move(e),p.index,false);
    }
  }

void Animation::Sequence::processEvents(uint64_t barrier, uint64_t sTime, uint64_t now, EvCount& ev) const {
  uint64_t frameA=0,frameB=0;
  bool     invert=false;
  if(!extractFrames(frameA,frameB,invert,barrier,sTime,now))
    return;

  auto& d       = *data;
  float fpsRate = d.fpsRate;

  for(auto& e:d.events) {
    if(e.type == zenkit::MdsEventType::OPTIMAL_FRAME) {
      for(auto i:e.frames) {
        uint64_t fr = frameClamp(i,d.firstFrame,d.numFrames,d.lastFrame);
        if((frameA<=fr && fr<frameB) ^ invert)
          processEvent(e,ev,uint64_t(float(fr)*1000.f/fpsRate)+sTime);
        }
      } else {
      uint64_t fr = frameClamp(e.frame,d.firstFrame,d.numFrames,d.lastFrame);
      if((frameA<=fr && fr<frameB) ^ invert)
        processEvent(e,ev,uint64_t(float(fr)*1000.f/fpsRate)+sTime);
      }
    }

  for(auto& i:d.gfx){
    uint64_t fr = frameClamp(i.frame,d.firstFrame,d.numFrames,d.lastFrame);
    if((frameA<=fr && fr<frameB) ^ invert)
      ev.groundSounds++;
    }

  for(auto& i:d.mmStartAni){
    uint64_t fr = frameClamp(i.frame,d.firstFrame,d.numFrames,d.lastFrame);
    if((frameA<=fr && fr<frameB) ^ invert) {
      EvMorph e;
      e.anim = i.animation;
      e.node = i.node;
      ev.morph.push_back(e);
      }
    }
  }

void Animation::Sequence::processEvent(const zenkit::MdsEventTag& e, Animation::EvCount &ev, uint64_t time) {
  switch(e.type) {
    case zenkit::MdsEventType::OPTIMAL_FRAME:
      ev.def_opt_frame++;
      break;
    case zenkit::MdsEventType::SET_FIGHT_MODE:
      ev.weaponCh = e.fight_mode;
      break;
    case zenkit::MdsEventType::ITEM_CREATE:
    case zenkit::MdsEventType::ITEM_EXCHANGE:{
      EvTimed ex;
      ex.def     = e.type;
      ex.item    = e.item;
      ex.slot[0] = e.slot;
      ex.time    = time;
      ev.timed.push_back(ex);
      break;
      }
    case zenkit::MdsEventType::ITEM_INSERT:
    case zenkit::MdsEventType::ITEM_REMOVE:
    case zenkit::MdsEventType::ITEM_DESTROY:
    case zenkit::MdsEventType::ITEM_PLACE: {
      EvTimed ex;
      ex.def     = e.type;
      ex.slot[0] = e.slot;
      ex.time    = time;
      ev.timed.push_back(ex);
      break;
      }
    case zenkit::MdsEventType::MUNITION_PLACE:
    case zenkit::MdsEventType::MUNITION_REMOVE: {
      EvTimed ex;
      ex.def     = e.type;
      ex.slot[0] = e.slot;
      ex.time    = time;
      ev.timed.push_back(ex);
      break;
      }
    case zenkit::MdsEventType::MESH_SWAP: {
      EvTimed ex;
      ex.def     = e.type;
      ex.slot[0] = e.slot;
      ex.slot[1] = e.slot2;
      ex.time    = time;
      ev.timed.push_back(ex);
      break;
      }
    case zenkit::MdsEventType::TORCH_DRAW:
    case zenkit::MdsEventType::TORCH_INVENTORY:
    case zenkit::MdsEventType::TORCH_DROP: {
      EvTimed ex;
      ex.def     = e.type;
      ex.slot[0] = e.slot;
      ex.time    = time;
      ev.timed.push_back(ex);
      break;
      }
    default:
      break;
    }
  }

Tempest::Vec3 Animation::Sequence::speed(uint64_t at, uint64_t dt) const {
  auto a = translateXZ(at), b=translateXZ(at+dt); // note: at-dt vs at is more correct probably
  Tempest::Vec3 f = b-a;

  if(reverse)
    return -f;
  return f;
  }

Tempest::Vec3 Animation::Sequence::translateXZ(uint64_t at) const {
  auto&    d         = *data;
  uint32_t numFrames = d.numFrames;
  if(numFrames==0 || d.tr.size()==0)
    return Tempest::Vec3();

  if(animCls==Transition && !isFly()){
    uint64_t all=uint64_t(totalTime());
    if(at>all)
      at = all;
    }

  float    fr     = float(data->fpsRate*float(at))/1000.f;
  float    a      = std::fmod(fr,1.f);
  uint64_t frameA = uint64_t(fr);
  uint64_t frameB = frameA+1;

  auto  mA = frameA/d.tr.size();
  auto  pA = d.tr[size_t(frameA%d.tr.size())];

  auto  mB = frameB/d.tr.size();
  auto  pB = d.tr[size_t(frameB%d.tr.size())];

  pA += d.moveTr*float(mA);
  pB += d.moveTr*float(mB);

  Tempest::Vec3 p = pA + (pB-pA)*a;
  return p;
  }

void Animation::Sequence::schemeName(char scheme[64]) const {
  scheme[0] = '\0';
  for(size_t i=0, r=0; i<name.size(); ++i) {
    if(name[i]=='_') {
      for(i++;i<name.size() && r<63;++i) {
        if(name[i]=='_')
          break;
        scheme[r] = name[i];
        ++r;
        }
      scheme[r] = '\0';
      break;
      }
    }
  }

void Animation::AnimData::setupMoveTr() {
  size_t sz = nodeIndex.size();
  if(sz==0)
    return;

  if(nodeIndex[0]!=0)
    return;

  if(0<samples.size() && sz<=samples.size()) {
    auto& a = samples[0].position;
    auto& b = samples[samples.size()-sz].position;
    moveTr.x = b.x-a.x;
    moveTr.y = b.y-a.y;
    moveTr.z = b.z-a.z;

    tr.resize(samples.size()/sz - 1);
    for(size_t i=0, r=0; r<tr.size(); i+=sz,++r) {
      auto& p  = tr[r];
      auto& bi = samples[i].position;
      p.x = bi.x-a.x;
      p.y = bi.y-a.y;
      p.z = bi.z-a.z;
      }
    static const float eps = 0.4f;
    for(auto& i:tr) {
      if(std::fabs(i.x)<eps && std::fabs(i.y)<eps && std::fabs(i.z)<eps)
        continue;
      hasMoveTr = true;
      break;
      }
    }

  if(0<samples.size()){
    translate.x = samples[0].position.x;
    translate.y = samples[0].position.y;
    translate.z = samples[0].position.z;
    }
  }

void Animation::AnimData::setupEvents(float fpsRate) {
  if(fpsRate<=0.f)
    return;

  int hasOptFrame = std::numeric_limits<int>::max();
  for(size_t i=0; i<events.size(); ++i)
    if(events[i].type==zenkit::MdsEventType::OPTIMAL_FRAME) {
      hasOptFrame = std::min(hasOptFrame, events[i].frames[0]);
      }

  for(size_t i=0; i<events.size(); ++i)
    if(events[i].type==zenkit::MdsEventType::OPTIMAL_FRAME && events[i].frames[0]!=hasOptFrame) {
      events[i] = std::move(events.back());
      events.pop_back();
      }

  for(auto& r:events) {
    if(r.type==zenkit::MdsEventType::HIT_END)
      setupTime(defHitEnd,r.frames,fpsRate);
    if(r.type==zenkit::MdsEventType::PARRY_FRAME)
      setupTime(defParFrame,r.frames,fpsRate);
    if(r.type==zenkit::MdsEventType::COMBO_WINDOW)
      setupTime(defWindow,r.frames,fpsRate);
    }
  }

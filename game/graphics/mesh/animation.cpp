#include "animation.h"

#include <Tempest/Log>
#include <cctype>

#include <zenload/modelAnimationParser.h>
#include <zenload/zCModelPrototype.h>
#include <zenload/zenParser.h>

#include "graphics/pfx/particlefx.h"
#include "world/objects/npc.h"
#include "world/world.h"
#include "utils/fileext.h"
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

Animation::Animation(ZenLoad::MdsParser &p, std::string_view name, const bool ignoreErrChunks) {
  AnimData* current=nullptr;

  while(true) {
    ZenLoad::MdsParser::Chunk type=p.parse();
    switch (type) {
      case ZenLoad::MdsParser::CHUNK_EOF: {
        setupIndex();
        return;
        }
      case ZenLoad::MdsParser::CHUNK_MODEL_SCRIPT: {
        // TODO: use model-script?
        break;
        }
      case ZenLoad::MdsParser::CHUNK_ANI: {
        auto& ani      = loadMAN(p.ani,std::string(name)+'-'+p.ani.m_Name+".MAN");
        current        = ani.data.get();
        break;
        }
      case ZenLoad::MdsParser::CHUNK_ANI_ALIAS:{
        ref.emplace_back(std::move(p.alias));
        current = nullptr;
        break;
        }
      case ZenLoad::MdsParser::CHUNK_ANI_COMB:{
        current = nullptr;
        char name[256]={};
        std::snprintf(name,sizeof(name),"%s%d",p.comb.m_Asc.c_str(),1+(p.comb.m_LastFrame-1)/2);

        bool found=false;
        for(size_t r=0;r<sequences.size();++r) { // reverse search: expect to find animations right before aniComb
          auto& i = sequences[sequences.size()-r-1];
          if(i.askName==name) {
            auto d = i.data;
            sequences.emplace_back();
            Animation::Sequence& ani = sequences.back();
            ani.name     = p.comb.m_Name;
            ani.askName  = p.comb.m_Asc;
            ani.layer    = p.comb.m_Layer;
            ani.flags    = Flags(p.comb.m_Flags);
            ani.blendIn  = uint64_t(1000*p.comb.m_BlendIn);
            ani.blendOut = uint64_t(1000*p.comb.m_BlendOut);
            ani.next     = std::move(p.comb.m_Next);
            ani.data     = d; // set first as default
            ani.comb.resize(p.comb.m_LastFrame);
            found=true;
            break;
            }
          }
        if(!found)
          Log::d("comb not found: ",p.comb.m_Name," -> ",p.comb.m_Asc); // error
        break;
        }        
      case ZenLoad::MdsParser::CHUNK_ANI_BLEND:{
        break;
        }

      case ZenLoad::MdsParser::CHUNK_EVENT_SFX: {
        if(current) {
          if(current->sfx.size()==0) {
            current->sfx = std::move(p.sfx);
            } else {
            current->sfx.insert(current->sfx.end(), p.sfx.begin(), p.sfx.end());
            p.sfx.clear();
            }
          }
        break;
        }
      case ZenLoad::MdsParser::CHUNK_EVENT_SFX_GRND: {
        if(current) {
          if(current->gfx.size()==0) {
            current->gfx = std::move(p.gfx);
            } else {
            current->gfx.insert(current->gfx.end(), p.gfx.begin(), p.gfx.end());
            p.gfx.clear();
            }
          }
        break;
        }

      case ZenLoad::MdsParser::CHUNK_EVENT_PFX: {
        if(current) {
          if(current->pfx.size()==0) {
            current->pfx = std::move(p.pfx);
            } else {
            current->pfx.insert(current->pfx.end(), p.pfx.begin(), p.pfx.end());
            p.pfx.clear();
            }
          }
        break;
        }
      case ZenLoad::MdsParser::CHUNK_EVENT_PFX_STOP: {
        if(current) {
          if(current->pfxStop.size()==0) {
            current->pfxStop = std::move(p.pfxStop);
            } else {
            current->pfxStop.insert(current->pfxStop.end(), p.pfxStop.begin(), p.pfxStop.end());
            p.pfxStop.clear();
            }
          }
        break;
        }

      case ZenLoad::MdsParser::CHUNK_EVENT_TAG: {
        if(current) {
          if(current->events.size()==0) {
            current->events = std::move(p.eventTag);
            } else {
            current->events.insert(current->events.end(), p.eventTag.begin(), p.eventTag.end());
            p.eventTag.clear();
            }
          }
        break;
        }

      case ZenLoad::MdsParser::CHUNK_EVENT_MMSTARTANI: {
        if(current) {
          if(current->mmStartAni.size()==0) {
            current->mmStartAni = std::move(p.mmStartAni);
            } else {
            current->mmStartAni.insert(current->mmStartAni.end(), p.mmStartAni.begin(), p.mmStartAni.end());
            p.mmStartAni.clear();
            }
          }
        break;
        }

      case ZenLoad::MdsParser::CHUNK_MODEL_TAG: {
        if(current)
          current->tag = std::move(p.modelTag);
        break;
        }
      case ZenLoad::MdsParser::CHUNK_REGISTER_MESH: {
        for(auto& i:p.meshesASC) {
          MeshAndThree m;
          m.mds = i;
          mesh.push_back(m);
          }
        p.meshesASC.clear();
        break;
        }
      case ZenLoad::MdsParser::CHUNK_MESH_AND_TREE: {
        meshDef.mds      = p.meshAndThree.m_Name;
        meshDef.disabled = p.meshAndThree.m_Disabled;
        break;
        }
      case ZenLoad::MdsParser::CHUNK_ERROR:
        if(!ignoreErrChunks)
          throw std::runtime_error("animation load error");
        break;
      default:{
        static std::unordered_set<int> v;
        if(v.find(type)==v.end()){
          //Log::d("not implemented anim chunk: ",int(type));
          v.insert(type);
          }
        break;
        }
      }
    }
  }

const Animation::Sequence* Animation::sequence(std::string_view name) const {
  auto it = std::lower_bound(sequences.begin(),sequences.end(),name,[](const Sequence& s,std::string_view n){
    return s.name<n;
    });

  if(it!=sequences.end() && it->name==name)
    return &(*it);
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

const std::string& Animation::defaultMesh() const {
  if(meshDef.mds.size()>0 && !meshDef.disabled)
    return meshDef.mds;
  static std::string nop;
  return nop;
  }

Animation::Sequence& Animation::loadMAN(const ZenLoad::zCModelScriptAni& hdr, const std::string& name) {
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
      if(s.askName==r.m_Alias)
        ani.data = s.data;

    if(ani.data==nullptr) {
      Log::d("alias not found: ",r.m_Name," -> ",r.m_Alias);
      continue;
      }

    ani.name     = r.m_Name;
    ani.layer    = r.m_Layer;
    ani.flags    = Flags(r.m_Flags);
    ani.blendIn  = uint64_t(1000*r.m_BlendIn);
    ani.blendOut = uint64_t(1000*r.m_BlendOut);
    ani.reverse  = r.m_Dir!=ZenLoad::MSB_FORWARD;
    ani.next     = r.m_Next;
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
      char name[256]={};
      std::snprintf(name,sizeof(name),"%s%d",s.askName.c_str(),int(i+1));
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
    i.nextPtr = sequence(i.next.c_str());
    i.owner   = this;
    }
  // for(auto& i:sequences)
  //   Log::i(i.name);
  }


Animation::Sequence::Sequence(const ZenLoad::zCModelScriptAni& hdr, const std::string &fname) {
  if(!Resources::hasFile(fname))
    return;

  const VDFS::FileIndex& idx = Resources::vdfsIndex();
  ZenLoad::ZenParser            zen(fname,idx);
  ZenLoad::ModelAnimationParser p(zen);

  data = std::make_shared<AnimData>();
  askName    = hdr.m_Name;
  layer      = hdr.m_Layer;
  flags      = Flags(hdr.m_Flags);
  blendIn    = uint64_t(1000*hdr.m_BlendIn);
  blendOut   = uint64_t(1000*hdr.m_BlendOut);
  next       = hdr.m_Next;
  reverse    = hdr.m_Dir!=ZenLoad::MSB_FORWARD;

  data->firstFrame = uint32_t(hdr.m_FirstFrame);
  data->lastFrame  = uint32_t(hdr.m_LastFrame);

  while(true) {
    ZenLoad::ModelAnimationParser::EChunkType type = p.parse();
    switch(type) {
      case ZenLoad::ModelAnimationParser::CHUNK_EOF:{
        setupMoveTr();
        return;
        }
      case ZenLoad::ModelAnimationParser::CHUNK_HEADER: {
        name            = p.getHeader().aniName;
        layer           = p.getHeader().layer;
        data->fpsRate   = p.getHeader().fpsRate;
        data->numFrames = p.getHeader().numFrames;
        break;
        }
      case ZenLoad::ModelAnimationParser::CHUNK_RAWDATA:
        data->nodeIndex = std::move(p.getNodeIndex());
        data->samples   = p.getSamples();
        break;
      case ZenLoad::ModelAnimationParser::CHUNK_ERROR:
        throw std::runtime_error("animation load error");
      }
    }
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
  if(animCls==Animation::Transition)
    return false;
  if(size_t(comboLen*2+1)<data->defWindow.size() && isInComboWindow(now-sTime,comboLen))
    return false;
  return true;
  }

bool Animation::Sequence::isInComboWindow(uint64_t t, uint16_t comboLen) const {
  uint16_t id = uint16_t(comboLen*2u);
  return (t<data->defWindow[id+0] || data->defWindow[id+1]<=t);
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

bool Animation::Sequence::isAtackAnim() const {
  for(auto& e:data->events)
    if(e.m_Def==ZenLoad::DEF_OPT_FRAME)
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
    if(e.m_Def==ZenLoad::DEF_OPT_FRAME)
      for(auto i:e.m_Int)
        if(int64_t(i)>frame)
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

void Animation::Sequence::processSfx(uint64_t barrier, uint64_t sTime, uint64_t now, Npc& npc) const {
  uint64_t frameA=0,frameB=0;
  bool     invert=false;
  if(!extractFrames(frameA,frameB,invert,barrier,sTime,now))
    return;

  auto& d = *data;
  for(auto& i:d.sfx) {
    uint64_t fr = frameClamp(i.m_Frame,d.firstFrame,d.numFrames,d.lastFrame);
    if(((frameA<=fr && fr<frameB) ^ invert) ||
       i.m_Frame==int32_t(d.lastFrame))
      npc.emitSoundEffect(i.m_Name,i.m_Range,i.m_EmptySlot);
    }
  if(!npc.isInAir()) {
    for(auto& i:d.gfx){
      uint64_t fr = frameClamp(i.m_Frame,d.firstFrame,d.numFrames,d.lastFrame);
      if((frameA<=fr && fr<frameB) ^ invert)
        npc.emitSoundGround(i.m_Name,i.m_Range,i.m_EmptySlot);
      }
    }
  }

void Animation::Sequence::processPfx(uint64_t barrier, uint64_t sTime, uint64_t now, MdlVisual& visual, World& world) const {
  uint64_t frameA=0,frameB=0;
  bool     invert=false;
  if(!extractFrames(frameA,frameB,invert,barrier,sTime,now))
    return;

  auto& d = *data;
  for(auto& i:d.pfx){
    uint64_t fr = frameClamp(i.m_Frame,d.firstFrame,d.numFrames,d.lastFrame);
    if(((frameA<=fr && fr<frameB) ^ invert) ||
       i.m_Frame==int32_t(d.lastFrame)) {
      if(i.m_Name.empty())
        continue;
      Effect e(PfxEmitter(world,i.m_Name),i.m_Pos);
      e.setActive(true);
      visual.startEffect(world,std::move(e),i.m_Num,false);
      }
    }
  for(auto& i:d.pfxStop){
    uint64_t fr = frameClamp(i.m_Frame,d.firstFrame,d.numFrames,d.lastFrame);
    if(((frameA<=fr && fr<frameB) ^ invert) ||
       i.m_Frame==int32_t(d.lastFrame)) {
      visual.stopEffect(i.m_Num);
      }
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
    if(e.m_Def==ZenLoad::DEF_OPT_FRAME) {
      for(auto i:e.m_Int) {
        uint64_t fr = frameClamp(i,d.firstFrame,d.numFrames,d.lastFrame);
        if((frameA<=fr && fr<frameB) ^ invert)
          processEvent(e,ev,uint64_t(float(fr)*1000.f/fpsRate)+sTime);
        }
      } else {
      uint64_t fr = frameClamp(e.m_Frame,d.firstFrame,d.numFrames,d.lastFrame);
      if((frameA<=fr && fr<frameB) ^ invert)
        processEvent(e,ev,uint64_t(float(fr)*1000.f/fpsRate)+sTime);
      }
    }

  for(auto& i:d.gfx){
    uint64_t fr = frameClamp(i.m_Frame,d.firstFrame,d.numFrames,d.lastFrame);
    if((frameA<=fr && fr<frameB) ^ invert)
      ev.groundSounds++;
    }

  for(auto& i:d.mmStartAni){
    uint64_t fr = frameClamp(i.m_Frame,d.firstFrame,d.numFrames,d.lastFrame);
    if((frameA<=fr && fr<frameB) ^ invert) {
      EvMorph e;
      e.anim = i.m_Animation.c_str();
      e.node = i.m_Node.c_str();
      ev.morph.push_back(e);
      }
    }
  }

void Animation::Sequence::processEvent(const ZenLoad::zCModelEvent &e, Animation::EvCount &ev, uint64_t time) {
  switch(e.m_Def) {
    case ZenLoad::DEF_NULL:
    case ZenLoad::DEF_LAST:
    case ZenLoad::DEF_DAM_MULTIPLY:
    case ZenLoad::DEF_HIT_LIMB:
    case ZenLoad::DEF_HIT_DIR:
    case ZenLoad::DEF_HIT_END:
      break;
    case ZenLoad::DEF_OPT_FRAME:
      ev.def_opt_frame++;
      break;
    case ZenLoad::DEF_FIGHTMODE:
      ev.weaponCh = e.m_Fmode;
      break;
    case ZenLoad::DEF_DRAWSOUND:
    case ZenLoad::DEF_UNDRAWSOUND:
      break;
    case ZenLoad::DEF_PAR_FRAME:
      break;
    case ZenLoad::DEF_WINDOW:
      break;
    case ZenLoad::DEF_CREATE_ITEM:
    case ZenLoad::DEF_EXCHANGE_ITEM:{
      EvTimed ex;
      ex.def     = e.m_Def;
      ex.item    = e.m_Item;
      ex.slot[0] = e.m_Slot;
      ex.time    = time;
      ev.timed.push_back(ex);
      break;
      }
    case ZenLoad::DEF_INSERT_ITEM:
    case ZenLoad::DEF_REMOVE_ITEM:
    case ZenLoad::DEF_DESTROY_ITEM:
    case ZenLoad::DEF_PLACE_ITEM: {
      EvTimed ex;
      ex.def     = e.m_Def;
      ex.slot[0] = e.m_Slot;
      ex.time    = time;
      ev.timed.push_back(ex);
      break;
      }
    case ZenLoad::DEF_PLACE_MUNITION:
    case ZenLoad::DEF_REMOVE_MUNITION: {
      EvTimed ex;
      ex.def     = e.m_Def;
      ex.slot[0] = e.m_Slot;
      ex.time    = time;
      ev.timed.push_back(ex);
      break;
      }
    case ZenLoad::DEF_SWAPMESH: {
      EvTimed ex;
      ex.def     = e.m_Def;
      ex.slot[0] = e.m_Slot;
      ex.slot[1] = e.m_Slot2;
      ex.time    = time;
      ev.timed.push_back(ex);
      break;
      }
    case ZenLoad::DEF_DRAWTORCH:
    case ZenLoad::DEF_INV_TORCH:
    case ZenLoad::DEF_DROP_TORCH: {
      EvTimed ex;
      ex.def     = e.m_Def;
      ex.slot[0] = e.m_Slot;
      ex.time    = time;
      ev.timed.push_back(ex);
      break;
      }
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

void Animation::Sequence::schemeName(char scheme[]) const {
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

void Animation::Sequence::setupMoveTr() {
  data->setupMoveTr();
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
    if(events[i].m_Def==ZenLoad::DEF_OPT_FRAME) {
      hasOptFrame = std::min(hasOptFrame, events[i].m_Int[0]);
      }

  for(size_t i=0; i<events.size(); ++i)
    if(events[i].m_Def==ZenLoad::DEF_OPT_FRAME && events[i].m_Int[0]!=hasOptFrame) {
      events[i] = std::move(events.back());
      events.pop_back();
      }

  for(auto& r:events) {
    if(r.m_Def==ZenLoad::DEF_HIT_END)
      setupTime(defHitEnd,r.m_Int,fpsRate);
    if(r.m_Def==ZenLoad::DEF_PAR_FRAME)
      setupTime(defParFrame,r.m_Int,fpsRate);
    if(r.m_Def==ZenLoad::DEF_WINDOW)
      setupTime(defWindow,r.m_Int,fpsRate);
    }
  }

#include "animation.h"

#include <Tempest/Log>

#include <zenload/modelAnimationParser.h>
#include <zenload/zCModelPrototype.h>
#include <zenload/zenParser.h>

#include "world/npc.h"
#include "resources.h"

using namespace Tempest;

static void setupTime(std::vector<uint64_t>& t0,const std::vector<int32_t>& inp,float fps){
  t0.resize(inp.size());
  for(size_t i=0;i<inp.size();++i){
    t0[i] = uint64_t(float(inp[i])*1000.f/fps);
    }
  }

static uint64_t frameClamp(int32_t frame,uint32_t first,uint32_t last) {
  if(frame<int(first))
    return 0;
  if(frame>int(last))
    return last-first;
  return uint64_t(frame)-first;
  }

Animation::Animation(ZenLoad::MdsParser &p,const std::string& name,const bool ignoreErrChunks) {
  AnimData* current=nullptr;

  while(true) {
    ZenLoad::MdsParser::Chunk type=p.parse();
    switch (type) {
      case ZenLoad::MdsParser::CHUNK_EOF: {
        setupIndex();
        return;
        }
      case ZenLoad::MdsParser::CHUNK_ANI: {
        auto& ani      = loadMAN(name+'-'+p.ani.m_Name+".MAN");
        //auto& ani      = loadMAN(p.ani.m_Asc);
        current        = ani.data.get();

        ani.askName    = p.ani.m_Name;
        ani.layer      = p.ani.m_Layer;
        ani.flags      = Flags(p.ani.m_Flags);
        ani.next       = p.ani.m_Next;
        ani.reverse    = p.ani.m_Dir!=ZenLoad::MSB_FORWARD;
        current->firstFrame = uint32_t(p.ani.m_FirstFrame);
        current->lastFrame  = uint32_t(p.ani.m_LastFrame);
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
        std::snprintf(name,sizeof(name),"%s%d",p.comb.m_Asc.c_str(),1);

        bool found=false;
        for(size_t r=0;r<sequences.size();++r) { // reverse search: expect to find animations right before aniComb
          auto& i = sequences[sequences.size()-r-1];
          if(i.askName==name) {
            auto d = i.data;
            sequences.emplace_back();
            Animation::Sequence& ani = sequences.back();
            ani.name    = sequences[sequences.size()-r-1].name;
            ani.askName = name;
            ani.layer   = p.comb.m_Layer;
            ani.flags   = Flags(p.comb.m_Flags);
            ani.next    = p.comb.m_Next;
            ani.data    = d; // set first as default
            found=true;
            break;
            }
          }
        if(!found)
          Log::d("comb not found: ",p.comb.m_Name," -> ",p.comb.m_Asc); // error
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
      case ZenLoad::MdsParser::CHUNK_MODEL_TAG: {
        if(current)
          current->tag = std::move(p.modelTag);
        break;
        }
      case ZenLoad::MdsParser::CHUNK_EVENT_PFX: {
        p.pfx.clear();
        break;
        }
      case ZenLoad::MdsParser::CHUNK_EVENT_PFX_STOP: {
        p.pfxStop.clear();
        break;
        }
      case ZenLoad::MdsParser::CHUNK_EVENT_TAG: {
        if(current){
          if(current->events.size()==0) {
            current->events = std::move(p.eventTag);
            } else {
            current->events.insert(current->events.end(), p.eventTag.begin(), p.eventTag.end());
            p.eventTag.clear();
            }
          }
        break;
        }
      case ZenLoad::MdsParser::CHUNK_MESH_AND_TREE:
      case ZenLoad::MdsParser::CHUNK_REGISTER_MESH:
        break;
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

const Animation::Sequence* Animation::sequence(const char *name) const {
  auto it = std::lower_bound(sequences.begin(),sequences.end(),name,[](const Sequence& s,const char* n){
    return s.name<n;
    });

  if(it!=sequences.end() && it->name==name)
    return &(*it);
  return nullptr;
  }

const Animation::Sequence *Animation::sequenceAsc(const char *name) const {
  if(auto s = sequence(name))
    return s; //gothic2 format

  auto it = std::lower_bound(sequences.begin(),sequences.end(),name,[](const Sequence& s,const char* n){
    return s.askName<n;
    });

  if(it!=sequences.end() && it->askName==name)
    return &(*it);
  return nullptr;
  }

void Animation::debug() const {
  for(auto& i:sequences)
    Log::d(i.name);
  }

Animation::Sequence& Animation::loadMAN(const std::string& name) {
  sequences.emplace_back(name);
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

    ani.name    = r.m_Name;
    ani.layer   = r.m_Layer;
    ani.flags   = Flags(r.m_Flags);
    ani.reverse = r.m_Dir!=ZenLoad::MSB_FORWARD;
    ani.next    = r.m_Next;
    sequences.emplace_back(std::move(ani));
    }
  ref.clear();

  std::sort(sequences.begin(),sequences.end(),[](const Sequence& a,const Sequence& b){
    return a.name<b.name;
    });

  for(auto& i:sequences) {
    if((i.next==i.askName && !i.next.empty()) || i.next==i.name)
      i.animCls = Loop;
    if(i.name.find("S_")==0)
      i.shortName = &i.name[2];
    if(i.name=="T_1HRUN_2_1H"   || i.name=="T_BOWRUN_2_BOW" ||
       i.name=="T_2HRUN_2_2H"   || i.name=="T_CBOWRUN_2_CBOW" ||
       i.name=="T_MAGRUN_2_MAG" || i.name=="T_FISTRUN_2_FIST")
      i.next = "";
    }
  // for(auto& i:sequences)
  //   Log::i(i.name);
  }


Animation::Sequence::Sequence(const std::string &fname) {
  if(!Resources::hasFile(fname))
    return;

  const VDFS::FileIndex& idx = Resources::vdfsIndex();
  ZenLoad::ZenParser            zen(fname,idx);
  ZenLoad::ModelAnimationParser p(zen);

  data = std::make_shared<AnimData>();
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

bool Animation::Sequence::isFinished(uint64_t t,uint16_t comboLen) const {
  if(isRotate())
    return true;// FIXME: proper rotate

  if(comboLen<data->defHitEnd.size()) {
    if(t>data->defHitEnd[comboLen])
      return true;
    }
  return float(t)>totalTime();
  }

float Animation::Sequence::totalTime() const {
  return float(data->numFrames)*1000.f/data->fpsRate;
  }


float Animation::Sequence::atkTotalTime(uint32_t comboLvl) const {
  if(comboLvl<data->defHitEnd.size()) {
    uint64_t time = data->defHitEnd[comboLvl];
    return float(time);
    }
  return totalTime();
  }

bool Animation::Sequence::canInterrupt() const {
  if(animCls==Animation::Transition)
    return false;
  if(!data->defHitEnd.empty())
    return false;
  return true;
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
    if(frameA<frameB && frameB==numFrames)
      frameB = d.lastFrame; //HACK
    }

  if(reverse) {
    frameA = numFrames-frameA-1;
    frameB = numFrames-frameB-1;
    std::swap(frameA,frameB);
    }

  invert = (frameB<frameA);
  if(invert)
    std::swap(frameA,frameB);
  return true;
  }

void Animation::Sequence::processSfx(uint64_t barrier, uint64_t sTime, uint64_t now, Npc &npc) const {
  uint64_t frameA=0,frameB=0;
  bool     invert=false;
  if(!extractFrames(frameA,frameB,invert,barrier,sTime,now))
    return;

  auto& d = *data;
  for(auto& i:d.sfx){
    uint64_t fr = frameClamp(i.m_Frame,d.firstFrame,d.lastFrame);
    if((frameA<=fr && fr<frameB) ^ invert)
      npc.emitSoundEffect(i.m_Name.c_str(),i.m_Range,i.m_EmptySlot);
    }
  if(!npc.isInAir()) {
    for(auto& i:d.gfx){
      uint64_t fr = frameClamp(i.m_Frame,d.firstFrame,d.lastFrame);
      if((frameA<=fr && fr<frameB) ^ invert)
        npc.emitSoundGround(i.m_Name.c_str(),i.m_Range,i.m_EmptySlot);
      }
    }
  }

void Animation::Sequence::processEvents(uint64_t barrier, uint64_t sTime, uint64_t now, EvCount& ev) const {
  if(data->events.size()==0)
    return;

  uint64_t frameA=0,frameB=0;
  bool     invert=false;
  if(!extractFrames(frameA,frameB,invert,barrier,sTime,now))
    return;

  auto& d       = *data;
  float fpsRate = d.fpsRate;

  for(auto& e:d.events) {
    if(e.m_Def==ZenLoad::DEF_OPT_FRAME) {
      for(auto i:e.m_Int){
        uint64_t fr = frameClamp(i,d.firstFrame,d.lastFrame);
        if((frameA<=fr && fr<frameB) ^ invert)
          processEvent(e,ev,uint64_t(float(fr)*1000.f/fpsRate)+sTime);
        }
      } else {
      uint64_t fr = frameClamp(e.m_Frame,d.firstFrame,d.lastFrame);
      if((frameA<=fr && fr<frameB) ^ invert)
        processEvent(e,ev,uint64_t(float(fr)*1000.f/fpsRate)+sTime);
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
      ex.item    = e.m_Item.c_str();
      ex.slot[0] = e.m_Slot.c_str();
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
      ex.slot[0] = e.m_Slot.c_str();
      ex.time    = time;
      ev.timed.push_back(ex);
      break;
      }
    case ZenLoad::DEF_PLACE_MUNITION:
    case ZenLoad::DEF_REMOVE_MUNITION: {
      EvTimed ex;
      ex.def     = e.m_Def;
      ex.slot[0] = e.m_Slot.c_str();
      ex.time    = time;
      ev.timed.push_back(ex);
      break;
      }
    case ZenLoad::DEF_SWAPMESH:
    case ZenLoad::DEF_DRAWTORCH:
    case ZenLoad::DEF_INV_TORCH:
    case ZenLoad::DEF_DROP_TORCH:
      break;
    }
  }

ZMath::float3 Animation::Sequence::translation(uint64_t dt) const {
  float k = float(dt)/totalTime();
  ZMath::float3 p = data->moveTr.position;
  p.x*=k;
  p.y*=k;
  p.z*=k;
  return p;
  }

ZMath::float3 Animation::Sequence::speed(uint64_t at,uint64_t dt) const {
  ZMath::float3 f={};

  auto a = translateXZ(at+dt), b=translateXZ(at);
  f.x = a.x-b.x;
  f.y = a.y-b.y;
  f.z = a.z-b.z;

  if(reverse)
    return ZMath::float3{-f.x,-f.y,-f.z};
  return f;
  }

ZMath::float3 Animation::Sequence::translateXZ(uint64_t at) const {
  auto&    d         = *data;
  uint32_t numFrames = d.numFrames;
  if(numFrames==0 || d.tr.size()==0) {
    ZMath::float3 n={0,0,0};
    return n;
    }
  if(animCls==Transition && !isFly()){
    uint64_t all=uint64_t(totalTime());
    if(at>all)
      at = all;
    }

  uint64_t fr     = uint64_t(data->fpsRate*float(at));
  float    a      = float(fr%1000)/1000.f;
  uint64_t frameA = fr/1000;
  uint64_t frameB = frameA+1;

  auto  mA = frameA/d.tr.size();
  auto  pA = d.tr[size_t(frameA%d.tr.size())];

  auto  mB = frameB/d.tr.size();
  auto  pB = d.tr[size_t(frameB%d.tr.size())];

  float m = float(mA)+float(mB-mA)*a;
  ZMath::float3 p=pA;
  p.x += (pB.x-pA.x)*a;
  p.y += (pB.y-pA.y)*a;
  p.z += (pB.z-pA.z)*a;

  p.x += m*data->moveTr.position.x;
  p.y += m*data->moveTr.position.y;
  p.z += m*data->moveTr.position.z;
  return p;
  }

void Animation::Sequence::setupMoveTr() {
  data->setupMoveTr();
  }

void Animation::AnimData::setupMoveTr() {
  size_t sz = nodeIndex.size();

  if(samples.size()>0 && samples.size()>=sz) {
    auto& a = samples[0].position;
    auto& b = samples[samples.size()-sz].position;
    moveTr.position.x = b.x-a.x;
    moveTr.position.y = b.y-a.y;
    moveTr.position.z = b.z-a.z;

    tr.resize(samples.size()/sz);
    for(size_t i=0,r=0;i<samples.size();i+=sz,++r){
      auto& p  = tr[r];
      auto& bi = samples[i].position;
      p.x = bi.x-a.x;
      p.y = bi.y-a.y;
      p.z = bi.z-a.z;
      }
    }

  if(samples.size()>0){
    translate=samples[0].position;
    }
  }

void Animation::AnimData::setupEvents(float fpsRate) {
  if(fpsRate<=0.f)
    return;

  for(auto& r:events) {
    if(r.m_Def==ZenLoad::DEF_HIT_END)
      setupTime(defHitEnd,r.m_Int,fpsRate);
    if(r.m_Def==ZenLoad::DEF_PAR_FRAME)
      setupTime(defParFrame,r.m_Int,fpsRate);
    if(r.m_Def==ZenLoad::DEF_WINDOW)
      setupTime(defWindow,r.m_Int,fpsRate);
    }
  }

#include "animation.h"

#include <Tempest/Log>

#include <zenload/modelAnimationParser.h>
#include <zenload/zCModelPrototype.h>
#include <zenload/zenParser.h>

#include "resources.h"

using namespace Tempest;

Animation::Animation(ZenLoad::MdsParser &p,const std::string& name,const bool ignoreErrChunks) {
  Sequence* current=nullptr;
  while(true) {
    ZenLoad::MdsParser::Chunk type=p.parse();
    switch (type) {
      case ZenLoad::MdsParser::CHUNK_EOF: {
        setupIndex();
        return;
        }
      case ZenLoad::MdsParser::CHUNK_ANI: {
        // p.ani().m_Name;
        // p.ani().m_Layer;
        // p.ani().m_BlendIn;
        // p.ani().m_BlendOut;
        // p.ani().m_Flags;
        // p.ani().m_LastFrame;
        // p.ani().m_Dir;

        auto& ani      = loadMAN(name+'-'+p.ani.m_Name+".MAN");
        current        = &ani;
        ani.flags      = Flags(p.ani.m_Flags);
        ani.nextStr    = p.ani.m_Next;
        ani.firstFrame = uint32_t(p.ani.m_FirstFrame);
        ani.lastFrame  = uint32_t(p.ani.m_LastFrame);
        if(ani.nextStr==ani.name)
          ani.animCls=Loop;
        if(ani.name=="S_2HATTACK")
          Log::i("");
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
          Log::d("not implemented anim chunk: ",int(type));
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

void Animation::debug() const {
  for(auto& i:sequences)
    Log::d(i.name);
  }

Animation::Sequence& Animation::loadMAN(const std::string& name) {
  sequences.emplace_back(name);
  return sequences.back();
  }

static void setupTime(std::vector<uint64_t>& t0,const std::vector<int32_t>& inp,float fps){
  t0.resize(inp.size());
  for(size_t i=0;i<inp.size();++i){
    t0[i] = uint64_t(inp[i]*1000/fps);
    }
  }

void Animation::setupIndex() {
  // for(auto& i:sequences)
  //   Log::i(i.name);
  for(auto& sq:sequences) {
    if(sq.fpsRate<=0.f)
      continue;
    for(auto& r:sq.events) {
      if(r.m_Def==ZenLoad::DEF_HIT_END)
        setupTime(sq.defHitEnd,r.m_Int,sq.fpsRate);
      if(r.m_Def==ZenLoad::DEF_OPT_FRAME)
        setupTime(sq.defOptFrame,r.m_Int,sq.fpsRate);
      if(r.m_Def==ZenLoad::DEF_DRAWSOUND)
        setupTime(sq.defDraw,r.m_Int,sq.fpsRate);
      if(r.m_Def==ZenLoad::DEF_UNDRAWSOUND)
        setupTime(sq.defUndraw,r.m_Int,sq.fpsRate);
      }
    }

  std::sort(sequences.begin(),sequences.end(),[](const Sequence& a,const Sequence& b){
    return a.name<b.name;
    });

  for(auto& i:sequences) {
    for(auto& r:sequences)
      if(r.name==i.nextStr){
        i.next = &r;
        break;
        }
    }
  }

Animation::Sequence::Sequence(const std::string &name) {
  if(!Resources::hasFile(name))
    return;

  const VDFS::FileIndex& idx = Resources::vdfsIndex();
  ZenLoad::ZenParser            zen(name,idx);
  ZenLoad::ModelAnimationParser p(zen);

  while(true) {
    ZenLoad::ModelAnimationParser::EChunkType type = p.parse();
    switch(type) {
      case ZenLoad::ModelAnimationParser::CHUNK_EOF:{
        setupMoveTr();
        return;
        }
      case ZenLoad::ModelAnimationParser::CHUNK_HEADER: {
        this->name      = p.getHeader().aniName;
        this->fpsRate   = p.getHeader().fpsRate;
        this->numFrames = p.getHeader().numFrames;
        this->layer     = p.getHeader().layer;

        if(this->name.size()>1){
          if(this->name.find("_2_")!=std::string::npos)
            animCls=Transition;
          else if(this->name[0]=='T' && this->name[1]=='_')
            animCls=Transition;
          else if(this->name[0]=='R' && this->name[1]=='_')
            animCls=Transition;
          else if(this->name[0]=='S' && this->name[1]=='_')
            animCls=Loop;

          //if(this->name=="S_JUMP" || this->name=="S_JUMPUP")
          //  animCls=Transition;
          }
        break;
        }
      case ZenLoad::ModelAnimationParser::CHUNK_RAWDATA:
        nodeIndex = std::move(p.getNodeIndex());
        samples   = p.getSamples();
        break;
      case ZenLoad::ModelAnimationParser::CHUNK_ERROR:
        throw std::runtime_error("animation load error");
      }
    }
  }

bool Animation::Sequence::isFinished(uint64_t t) const {
  return t>=totalTime();
  }

bool Animation::Sequence::isAtackFinished(uint64_t t) const {
  for(auto& i:defHitEnd)
    if(t>i)
      return true;
  return t>=totalTime();// || t>=1000;
  }

float Animation::Sequence::totalTime() const {
  return numFrames*1000/fpsRate;
  }

void Animation::Sequence::processEvents(uint64_t barrier, uint64_t sTime, uint64_t now, EvCount& ev) const {
  for(auto& i:defOptFrame)
    if(barrier<i+sTime && i+sTime<=now)
      ev.def_opt_frame++;
  for(auto& i:defDraw)
    if(barrier<i+sTime && i+sTime<=now)
      ev.def_draw++;
  for(auto& i:defUndraw)
    if(barrier<i+sTime && i+sTime<=now)
      ev.def_undraw++;
  }

ZMath::float3 Animation::Sequence::translation(uint64_t dt) const {
  float k = float(dt)/totalTime();
  ZMath::float3 p = moveTr.position;
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

  return f;
  }

ZMath::float3 Animation::Sequence::translateXZ(uint64_t at) const {
  if(numFrames==0 || tr.size()==0) {
    ZMath::float3 n={};
    return n;
    }
  if(animCls==Transition && !isFly()){
    uint64_t all=uint64_t(totalTime());
    if(at>all)
      at = all;
    }

  uint64_t fr     = uint64_t(fpsRate*at);
  float    a      = (fr%1000)/1000.f;
  uint64_t frameA = fr/1000;
  uint64_t frameB = frameA+1;

  auto  mA = frameA/tr.size();
  auto  pA = tr[frameA%tr.size()];

  auto  mB = frameB/tr.size();
  auto  pB = tr[frameB%tr.size()];

  float m = mA+(mB-mA)*a;
  ZMath::float3 p=pA;
  p.x += (pB.x-pA.x)*a;
  p.y += (pB.y-pA.y)*a;
  p.z += (pB.z-pA.z)*a;

  p.x += m*moveTr.position.x;
  p.y += m*moveTr.position.y;
  p.z += m*moveTr.position.z;
  return p;
  }

void Animation::Sequence::setupMoveTr() {
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

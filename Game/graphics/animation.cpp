#include "animation.h"

#include <Tempest/Log>

#include <zenload/modelAnimationParser.h>
#include <zenload/zenParser.h>

#include "resources.h"

using namespace Tempest;

Animation::Animation(ZenLoad::ModelScriptBinParser &p,const std::string& name) {
  while(true) {
    ZenLoad::ModelScriptParser::EChunkType type=p.parse();
    switch (type) {
      case ZenLoad::ModelScriptParser::CHUNK_EOF:
        return;
      case ZenLoad::ModelScriptParser::CHUNK_ANI: {
        // p.ani().m_Name;
        // p.ani().m_Layer;
        // p.ani().m_BlendIn;
        // p.ani().m_BlendOut;
        // p.ani().m_Flags;
        // p.ani().m_FirstFrame;
        // p.ani().m_LastFrame;
        // p.ani().m_Dir;

        auto& ani = loadMAN(name+'-'+p.ani().m_Name+".MAN");
        ani.flags = Flags(p.ani().m_Flags);
        ani.next  = p.ani().m_Next;
        /*
        for(auto& sfx : p.sfx())
          animationAddEventSFX(anim, sfx);
        p.sfx().clear();

        for(auto& sfx : p.sfxGround())
          animationAddEventSFXGround(anim, sfx);
        p.sfxGround().clear();

        for(auto& tag : p.tag())
          animationAddEventTag(anim, tag);
        p.tag().clear();

        for(auto& pfx:p.pfx())
          animationAddEventPFX(anim, pfx);
        p.pfx().clear();

        for(auto& pfxStop:p.pfxStop())
          animationAddEventPFXStop(anim, pfxStop);
        p.pfxStop().clear();
        */
        break;
        }

      case ZenLoad::ModelScriptParser::CHUNK_EVENT_SFX: {
        //animationAddEventSFX(anim, p.sfx().back());
        p.sfx().clear();
        break;
        }
      case ZenLoad::ModelScriptParser::CHUNK_EVENT_SFX_GRND: {
        // animationAddEventSFXGround(anim, p.sfx().back());
        p.sfxGround().clear();
        break;
        }
      case ZenLoad::ModelScriptParser::CHUNK_EVENT_PFX: {
        // animationAddEventPFX(anim, p.pfx().back());
        p.pfx().clear();
        break;
        }
      case ZenLoad::ModelScriptParser::CHUNK_EVENT_PFX_STOP: {
        // animationAddEventPFXStop(anim, p.pfxStop().back());
        p.pfxStop().clear();
        break;
        }
      case ZenLoad::ModelScriptParser::CHUNK_ERROR:
        throw std::runtime_error("animation load error");
      default:
        // Log::d("not implemented anim tag");
        break;
      }
    }
  }

const Animation::Sequence &Animation::sequence(const char *name) const {
  for(auto& i:sequences)
    if(i.name==name)
      return i;
  static Sequence s("<null>");
  return s;
  }

Animation::Sequence& Animation::loadMAN(const std::string& name) {
  sequences.emplace_back(name);
  return sequences.back();
  }

Animation::Sequence::Sequence(const std::string &name) {
  const VDFS::FileIndex& idx = Resources::vdfsIndex();
  if(!idx.hasFile(name))
    return;

  ZenLoad::ZenParser            zen(name,idx);
  ZenLoad::ModelAnimationParser p(zen);

  while(true) {
    ZenLoad::ModelAnimationParser::EChunkType type = p.parse();
    switch(type) {
      case ZenLoad::ModelAnimationParser::CHUNK_EOF:{
        setupMoveTr();
        return;
        }
      case ZenLoad::ModelAnimationParser::CHUNK_HEADER:
        this->name      = p.getHeader().aniName;
        this->fpsRate   = p.getHeader().fpsRate;
        this->numFrames = p.getHeader().numFrames;
        break;
      case ZenLoad::ModelAnimationParser::CHUNK_RAWDATA:
        nodeIndex = std::move(p.getNodeIndex());
        samples   = p.getSamples();
        break;
      case ZenLoad::ModelAnimationParser::CHUNK_ERROR:
        throw std::runtime_error("animation load error");
      }
    }
  }

ZMath::float3 Animation::Sequence::speed(uint64_t dt) const {
  if(numFrames==0) {
    ZMath::float3 f={};
    return f;
    }

  uint64_t fr     = uint64_t(fpsRate*dt);
  float    a      = (fr%1000)/1000.f;
  uint64_t frameA = (fr/1000  )%numFrames;
  uint64_t frameB = (fr/1000+1)%numFrames;

  ZMath::float3 x,y,f;
  x = moveTr[size_t(frameA)].position*a;
  y = moveTr[size_t(frameB)].position*(1.f-a);

  f.x = x.x*a+y.x*(1.f-a);
  f.y = x.y*a+y.y*(1.f-a);
  f.z = x.z*a+y.z*(1.f-a);
  return f;
  }

void Animation::Sequence::setupMoveTr() {
  float  rate = 1.0;//fpsRate*1.5;
  size_t sz   = nodeIndex.size();

  for(size_t i=0;i+sz<samples.size();i+=sz){
    auto& m0 = samples[i];
    auto& m1 = samples[i+sz];

    ZenLoad::zCModelAniSample tr={};
    tr.position.x = m1.position.x-m0.position.x;
    tr.position.y = m1.position.y-m0.position.y;
    tr.position.z = m1.position.z-m0.position.z;

    tr.position.x*=rate;
    tr.position.y*=rate;
    tr.position.z*=rate;
    moveTr.emplace_back(tr);
    }

  if(samples.size()==0)
    return;

  while(moveTr.size()<numFrames) {
    if(moveTr.size()==0){
      ZenLoad::zCModelAniSample s={};
      moveTr.push_back(s);
      } else {
      moveTr.push_back(moveTr.back());
      }
    }
  }

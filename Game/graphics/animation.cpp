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
      case ZenLoad::ModelScriptParser::CHUNK_EOF: {
        setupIndex();
        return;
        }
      case ZenLoad::ModelScriptParser::CHUNK_ANI: {
        // p.ani().m_Name;
        // p.ani().m_Layer;
        // p.ani().m_BlendIn;
        // p.ani().m_BlendOut;
        // p.ani().m_Flags;
        // p.ani().m_LastFrame;
        // p.ani().m_Dir;

        auto& ani      = loadMAN(name+'-'+p.ani().m_Name+".MAN");
        ani.flags      = Flags(p.ani().m_Flags);
        ani.nextStr    = p.ani().m_Next;
        ani.firstFrame = uint32_t(p.ani().m_FirstFrame);
        ani.lastFrame  = uint32_t(p.ani().m_LastFrame);
        if(ani.nextStr==ani.name)
          ani.animCls=Loop;
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

const Animation::Sequence* Animation::sequence(const char *name) const {
  for(auto& i:sequences)
    if(i.name==name)
      return &i;
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

void Animation::setupIndex() {
  for(auto& i:sequences) {
    for(auto& r:sequences)
      if(r.name==i.nextStr){
        i.next = &r;
        break;
        }
    }
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
      case ZenLoad::ModelAnimationParser::CHUNK_HEADER: {
        this->name      = p.getHeader().aniName;
        this->fpsRate   = p.getHeader().fpsRate;
        this->numFrames = p.getHeader().numFrames;

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

float Animation::Sequence::totalTime() const {
  return numFrames*1000/fpsRate;
  }

ZMath::float3 Animation::Sequence::speed(uint64_t at,uint64_t dt) const {
  float allTime=numFrames*1000/fpsRate;
  float k = -(dt/allTime);

  ZMath::float3 f={};
  f.x = moveTr.position.x*k;
  f.y = moveTr.position.y*k;
  f.z = moveTr.position.z*k;

  if(at>allTime-1)
    at=uint64_t(allTime-1);
  f.y = translateY(at)-translateY(at-dt);

  return f;
  }

float Animation::Sequence::translateY(uint64_t at) const {
  if(numFrames==0)
    return 0.f;

  uint64_t fr     = uint64_t(fpsRate*at);
  float    a      = (fr%1000)/1000.f;
  uint64_t frameA = (fr/1000  );
  uint64_t frameB = (fr/1000+1);

  if(animCls==Animation::Loop){
    frameA%=numFrames;
    frameB%=numFrames;
    } else {
    frameA = std::min<uint64_t>(frameA,numFrames-1);
    frameB = std::min<uint64_t>(frameB,numFrames-1);
    }

  const size_t idSize=nodeIndex.size();
  if(idSize==0 || samples.size()%idSize!=0)
    return 0.0;

  auto* sampleA = &samples[size_t(frameA*idSize)];
  auto* sampleB = &samples[size_t(frameB*idSize)];

  float pos = sampleA->position.y+(sampleB->position.y-sampleA->position.y)*a;

  auto& sample0 = samples[0].position;

  return pos-sample0.y;
  }

void Animation::Sequence::setupMoveTr() {
  size_t sz = nodeIndex.size();

  if(samples.size()>0 && samples.size()>=sz) {
    auto& a = samples[0].position;
    auto& b = samples[samples.size()-sz].position;
    moveTr.position.x = b.x-a.x;
    moveTr.position.y = b.y-a.y;
    moveTr.position.z = b.z-a.z;
    }

  if(samples.size()>0){
    translate=samples[0].position;
    }
  }

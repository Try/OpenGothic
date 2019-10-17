#pragma once

#include <zenload/modelScriptParser.h>
#include <memory>

class Npc;

class Animation final {
  public:
    enum Flags : uint32_t {
      None         = 0,
      Move         = 0x00000001,
      Rotate       = 0x00000002,
      QueueAnim    = 0x00000004,
      Fly          = 0x00000008,
      Idle         = 0x00000010
      };

    enum AnimClass : uint8_t {
      UnknownAnim=0,
      Transition,
      Loop
      };

    struct EvCount final {
      uint8_t              def_opt_frame=0;
      uint8_t              def_draw=0;
      uint8_t              def_undraw=0;
      ZenLoad::EFightMode  weaponCh=ZenLoad::FM_LAST;
      };

    struct AnimData final {
      ZMath::float3                               translate={};
      ZenLoad::zCModelAniSample                   moveTr={};

      std::vector<ZenLoad::zCModelAniSample>      samples;
      std::vector<uint32_t>                       nodeIndex;
      std::vector<ZMath::float3>                  tr;

      uint32_t                                    firstFrame=0;
      uint32_t                                    lastFrame =0;
      float                                       fpsRate   =60.f;
      uint32_t                                    numFrames =0;

      std::vector<ZenLoad::zCModelScriptEventSfx> sfx, gfx;
      std::vector<ZenLoad::zCModelEvent>          tag;
      std::vector<ZenLoad::zCModelEvent>          events;

      std::vector<uint64_t>                       defHitEnd;   // hit-end time
      std::vector<uint64_t>                       defParFrame; // block timings

      void                                        setupMoveTr();
      void                                        setupEvents(float fpsRate);
      };

    struct Sequence final {
      Sequence()=default;
      Sequence(const std::string& name);

      bool                                   isRotate() const { return bool(flags&Flags::Rotate); }
      bool                                   isMove()   const { return bool(flags&Flags::Move);   }
      bool                                   isFly()    const { return bool(flags&Flags::Fly);    }
      bool                                   isIdle()   const { return bool(flags&Flags::Idle);   }
      bool                                   isFinished(uint64_t t) const;
      bool                                   canInterrupt() const;
      bool                                   isAtackFinished(uint64_t t) const;
      bool                                   isParWindow(uint64_t t) const;
      float                                  totalTime() const;

      void                                   processEvents(uint64_t barrier, uint64_t sTime, uint64_t now, EvCount& ev) const;
      void                                   emitSfx(Npc &npc, uint64_t now, uint64_t fr) const;

      ZMath::float3                          translation(uint64_t dt) const;
      ZMath::float3                          speed(uint64_t at, uint64_t dt) const;
      ZMath::float3                          translateXZ(uint64_t at) const;

      std::string                            name, shortName;
      uint32_t                               layer  =0;
      Flags                                  flags  =Flags::None;
      AnimClass                              animCls=Transition;
      bool                                   reverse=false;
      std::string                            next;

      std::shared_ptr<AnimData>              data;

      private:
        void setupMoveTr();
        static void processEvent(const ZenLoad::zCModelEvent& e, EvCount& ev);
      };

    Animation(ZenLoad::MdsParser &p, const std::string &name, bool ignoreErrChunks);

    const Sequence *sequence(const char* name) const;
    void            debug() const;

  private:
    Sequence& loadMAN(const std::string &name);
    void      setupIndex();

    std::vector<Sequence>                       sequences;
    std::vector<ZenLoad::zCModelScriptAniAlias> ref;
  };

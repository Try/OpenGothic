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

    struct EvTimed final {
      ZenLoad::EModelScriptAniDef def     = ZenLoad::DEF_NULL;
      const char*                 item    = nullptr;
      const char*                 slot[2] = {};
      uint64_t                    time    = 0;
      };

    struct EvCount final {
      uint8_t              def_opt_frame=0;
      ZenLoad::EFightMode  weaponCh=ZenLoad::FM_LAST;
      std::vector<EvTimed> timed;
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
      std::vector<ZenLoad::zCModelScriptEventPfx> pfx;
      std::vector<ZenLoad::zCModelScriptEventPfxStop> pfxStop;
      std::vector<ZenLoad::zCModelEvent>          tag;
      std::vector<ZenLoad::zCModelEvent>          events;

      std::vector<uint64_t>                       defHitEnd;   // hit-end time
      std::vector<uint64_t>                       defParFrame;
      std::vector<uint64_t>                       defWindow;

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
      bool                                   isFinished(uint64_t t, uint16_t comboLen) const;
      float                                  atkTotalTime(uint32_t comboLvl) const;
      bool                                   canInterrupt() const;
      bool                                   isDefParWindow(uint64_t t) const;
      bool                                   isDefWindow(uint64_t t) const;
      float                                  totalTime() const;

      bool                                   isPrehit(uint64_t sTime, uint64_t now) const;
      void                                   processEvents(uint64_t barrier, uint64_t sTime, uint64_t now, EvCount& ev) const;
      void                                   processSfx   (uint64_t barrier, uint64_t sTime, uint64_t now, Npc &npc) const;
      void                                   processPfx   (uint64_t barrier, uint64_t sTime, uint64_t now, Npc &npc) const;

      ZMath::float3                          translation(uint64_t dt) const;
      ZMath::float3                          speed(uint64_t at, uint64_t dt) const;
      ZMath::float3                          translateXZ(uint64_t at) const;

      std::string                            name, askName;
      const char*                            shortName = nullptr;
      uint32_t                               layer  =0;
      Flags                                  flags  =Flags::None;
      AnimClass                              animCls=Transition;
      bool                                   reverse=false;
      std::string                            next;

      std::vector<const Sequence*>           comb;
      std::shared_ptr<AnimData>              data;

      private:
        void setupMoveTr();
        static void                          processEvent(const ZenLoad::zCModelEvent& e, EvCount& ev, uint64_t time);
        bool                                 extractFrames(uint64_t &frameA, uint64_t &frameB, bool &invert, uint64_t barrier, uint64_t sTime, uint64_t now) const;
      };

    Animation(ZenLoad::MdsParser &p, const std::string &name, bool ignoreErrChunks);

    const Sequence *sequence(const char* name) const;
    const Sequence *sequenceAsc(const char* name) const;
    void            debug() const;

  private:
    Sequence& loadMAN(const std::string &name);
    void      setupIndex();

    std::vector<Sequence>                       sequences;
    std::vector<ZenLoad::zCModelScriptAniAlias> ref;
  };

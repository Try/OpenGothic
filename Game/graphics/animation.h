#pragma once

#include <zenload/modelScriptParser.h>

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

    struct Sequence final {
      Sequence(const std::string& name);

      bool                                   isMove() const { return bool(flags&Flags::Move); }
      bool                                   isFly()  const { return bool(flags&Flags::Fly);  }
      bool                                   isFinished(uint64_t t) const;

      std::string                            name;
      float                                  fpsRate=60.f;
      uint32_t                               numFrames=0;
      uint32_t                               firstFrame=0;
      uint32_t                               lastFrame =0;
      Flags                                  flags=Flags::None;
      AnimClass                              animCls=UnknownAnim;
      ZMath::float3                          translate={};

      std::vector<ZenLoad::zCModelAniSample> samples;
      std::vector<uint32_t>                  nodeIndex;

      std::string                            nextStr;
      const Sequence*                        next=nullptr;
      ZenLoad::zCModelAniSample              moveTr={};

      ZMath::float3                          speed(uint64_t at, uint64_t dt) const;
      float                                  translateY(uint64_t at) const;

      private:
        void setupMoveTr();
      };

    Animation(ZenLoad::ModelScriptBinParser& p, const std::string &name);

    const Sequence *sequence(const char* name) const;
    void            debug() const;

  private:
    std::vector<Sequence> sequences;
    Sequence& loadMAN(const std::string &name);
    void setupIndex();
  };

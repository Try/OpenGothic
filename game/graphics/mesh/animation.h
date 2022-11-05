#pragma once

#include <phoenix/model_script.hh>
#include <phoenix/animation.hh>

#include <Tempest/Vec>
#include <memory>

class Npc;
class MdlVisual;
class World;

class Animation final {
  public:
    enum AnimClass : uint8_t {
      UnknownAnim=0,
      Transition,
      Loop
      };

    struct EvTimed final {
      phoenix::mds::event_tag_type def = phoenix::mds::event_tag_type::unknown;
      std::string_view             item;
      std::string_view             slot[2] = {};
      uint64_t                     time    = 0;
      };

    struct EvMorph final {
      std::string_view node;
      std::string_view anim;
      };

    struct EvCount final {
      uint8_t                        def_opt_frame=0;
      uint8_t                        groundSounds=0;
      phoenix::mds::event_fight_mode weaponCh = phoenix::mds::event_fight_mode::invalid;
      std::vector<EvTimed> timed;
      std::vector<EvMorph> morph;
      };

    struct AnimData final {
      Tempest::Vec3                               translate={};
      Tempest::Vec3                               moveTr={};

      std::vector<phoenix::animation_sample>      samples;
      std::vector<uint32_t>                       nodeIndex;
      std::vector<Tempest::Vec3>                  tr;
      bool                                        hasMoveTr=false;

      uint32_t                                    firstFrame=0;
      uint32_t                                    lastFrame =0;
      float                                       fpsRate   =60.f;
      uint32_t                                    numFrames =0;

      std::vector<phoenix::mds::event_sfx_ground> gfx;
      std::vector<phoenix::mds::event_sfx>        sfx;
      std::vector<phoenix::mds::event_pfx>        pfx;
      std::vector<phoenix::mds::event_pfx_stop>   pfxStop;
      std::vector<phoenix::mds::model_tag>        tag;
      std::vector<phoenix::mds::event_tag>        events;

      std::vector<phoenix::mds::event_morph_animate> mmStartAni;

      std::vector<uint64_t>                       defHitEnd;   // hit-end time
      std::vector<uint64_t>                       defParFrame;
      std::vector<uint64_t>                       defWindow;

      void                                        setupMoveTr();
      void                                        setupEvents(float fpsRate);
      };

    struct Sequence final {
      Sequence()=default;
      Sequence(const phoenix::mds::animation& hdr, const std::string& name);

      bool                                   isRotate() const { return bool(flags & phoenix::mds::af_rotate); }
      bool                                   isMove()   const { return bool(flags & phoenix::mds::af_move);   }
      bool                                   isFly()    const { return bool(flags & phoenix::mds::af_fly);    }
      bool                                   isIdle()   const { return bool(flags & phoenix::mds::af_idle);   }
      bool                                   isFinished(uint64_t now, uint64_t sTime, uint16_t comboLen) const;
      float                                  atkTotalTime(uint16_t comboLen) const;
      bool                                   canInterrupt(uint64_t now, uint64_t sTime, uint16_t comboLen) const;
      bool                                   isInComboWindow(uint64_t t, uint16_t comboLen) const;
      bool                                   isDefParWindow(uint64_t t) const;
      bool                                   isDefWindow(uint64_t t) const;
      float                                  totalTime() const;

      bool                                   isAtackAnim() const;
      bool                                   isPrehit(uint64_t sTime, uint64_t now) const;
      void                                   processEvents(uint64_t barrier, uint64_t sTime, uint64_t now, EvCount& ev) const;
      void                                   processSfx   (uint64_t barrier, uint64_t sTime, uint64_t now, Npc &npc) const;
      void                                   processPfx   (uint64_t barrier, uint64_t sTime, uint64_t now, MdlVisual& visual, World& world) const;

      Tempest::Vec3                          speed(uint64_t at, uint64_t dt) const;
      Tempest::Vec3                          translateXZ(uint64_t at) const;
      void                                   schemeName(char buf[64]) const;

      std::string                            name, askName;
      const char*                            shortName = nullptr;
      uint32_t                               layer     = 0;
      phoenix::mds::animation_flags          flags     = phoenix::mds::af_none;
      uint64_t                               blendIn   = 0;
      uint64_t                               blendOut  = 0;
      AnimClass                              animCls   = Transition;
      bool                                   reverse   = false;

      std::string                            next;
      const Sequence*                        nextPtr = nullptr;
      const Animation*                       owner   = nullptr;

      std::vector<const Sequence*>           comb;
      std::shared_ptr<AnimData>              data;

      private:
        void                                 setupMoveTr();
        static void                          processEvent(const phoenix::mds::event_tag& e, EvCount& ev, uint64_t time);
        bool                                 extractFrames(uint64_t &frameA, uint64_t &frameB, bool &invert, uint64_t barrier, uint64_t sTime, uint64_t now) const;
      };


    Animation(phoenix::model_script &p, std::string_view name, bool ignoreErrChunks);

    const Sequence*    sequence(std::string_view name) const;
    const Sequence*    sequenceAsc(std::string_view name) const;
    void               debug() const;
    const std::string& defaultMesh() const;

  private:
    Sequence&          loadMAN(const phoenix::mds::animation& hdr, const std::string &name);
    void               setupIndex();

    std::vector<Sequence>                       sequences;
    std::vector<phoenix::mds::animation_alias>  ref;
    std::vector<std::string>                    mesh;
    phoenix::mds::skeleton                      meshDef;
  };

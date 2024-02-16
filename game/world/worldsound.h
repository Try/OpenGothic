#pragma once

#include <Tempest/SoundDevice>
#include <Tempest/SoundEffect>
#include <Tempest/Point>

#include <zenkit/vobs/Zone.hh>
#include <zenkit/vobs/Sound.hh>

#include <mutex>

#include "gamemusic.h"

class GameSession;
class TriggerEvent;
class World;
class Npc;
class Sound;
class SoundFx;

class WorldSound final {
  public:
    WorldSound(GameSession& game, World& world);
    ~WorldSound();

    void    setDefaultZone(const zenkit::VZoneMusic& vob);
    void    addZone       (const zenkit::VZoneMusic& vob);
    void    addSound      (const zenkit::VSound&     vob);

    Sound   addDlgSound(std::string_view s, const Tempest::Vec3& pos, float range, uint64_t &timeLen);

    void    aiOutput(const Tempest::Vec3& pos, std::string_view outputname);

    void    tick(Npc& player);
    bool    execTriggerEvent(const TriggerEvent& e);

    bool    isInListenerRange(const Tempest::Vec3& pos, float sndRgn) const;
    bool    canSeeSource(const Tempest::Vec3& npc) const;

    static const float talkRange;

  private:
    struct WSound;
    struct Zone;

    struct Effect {
      Tempest::SoundEffect eff;
      Tempest::Vec3        pos;
      float                vol     = 1.f;
      float                occ     = 1.f;
      float                maxDist = 0.f;
      bool                 loop    = false;
      bool                 active  = true;
      bool                 ambient = false;

      void setOcclusion(float occ);
      void setVolume(float v);
      };

    using PEffect = std::shared_ptr<Effect>;

    void    tickSoundZone(Npc& player);
    void    tickSlot(std::vector<PEffect>& eff);
    void    tickSlot(Effect& slot);
    void    initSlot(Effect& slot);
    bool    setMusic(std::string_view zone, GameMusic::Tags tags);

    Sound   implAddSound(const SoundFx& s, const Tempest::Vec3& pos, float rangeMax);
    Sound   implAddSound(Tempest::SoundEffect&& s, const Tempest::Vec3& pos, float rangeMax);

    GameSession&                            game;
    World&                                  owner;

    std::vector<Zone>                       zones;
    std::unique_ptr<Zone>                   def;

    uint64_t                                nextSoundUpdate=0;
    Zone*                                   currentZone = nullptr;
    GameMusic::Tags                         currentTags = GameMusic::Tags::Std;

    Tempest::Vec3                           plPos;

    std::unordered_map<std::string,PEffect> freeSlot;
    std::vector<PEffect>                    effect;
    std::vector<PEffect>                    effect3d; // snd_play3d
    std::vector<WSound>                     worldEff;

    std::mutex                              sync;

    static const float maxDist;

  friend class Sound;
  };

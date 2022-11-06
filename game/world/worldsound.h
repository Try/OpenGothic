#pragma once

#include <Tempest/SoundDevice>
#include <Tempest/SoundEffect>
#include <Tempest/Point>

#include <phoenix/vobs/zone.hh>
#include <phoenix/vobs/sound.hh>

#include <mutex>

#include "game/gametime.h"
#include "gamemusic.h"

class GameSession;
class World;
class Npc;
class Sound;
class SoundFx;

class WorldSound final {
  public:
    WorldSound(GameSession& game, World& world);
    ~WorldSound();

    void    setDefaultZone(const phoenix::vobs::zone_music &vob);
    void    addZone       (const phoenix::vobs::zone_music &vob);
    void    addSound      (const phoenix::vobs::sound &vob);

    Sound   addDlgSound(std::string_view s, float x, float y, float z, float range, uint64_t &timeLen);

    void    aiOutput(const Tempest::Vec3& pos, std::string_view outputname);

    void    tick(Npc& player);
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

    Sound   implAddSound(const SoundFx& s, float x, float y, float z, float rangeRef, float rangeMax);
    Sound   implAddSound(Tempest::SoundEffect&& s, float x, float y, float z, float rangeRef, float rangeMax);

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

#pragma once

#include "world/worldsound.h"

class Sound final {
  public:
    enum Type : uint8_t {
      T_Regular,
      T_3D,
      };
    Sound();
    Sound(World& owner, Type t, std::string_view s, const Tempest::Vec3& pos, float range, bool freeSlot);

    Sound(Sound&& other);
    Sound& operator = (Sound&& other);
    ~Sound();

    Tempest::Vec3 position()   const;
    bool          isEmpty()    const;
    bool          isFinished() const;

    void          setOcclusion(float occ);
    void          setVolume(float v);
    float         volume() const;

    void          setPosition(const Tempest::Vec3& pos);
    void          setPosition(float x,float y,float z);

    void          setLooping(bool l);
    void          setAmbient(bool a);
    void          setActive(bool a);
    void          play();

    uint64_t      effectPrefferedTime() const;

  private:
    Sound(const std::shared_ptr<WorldSound::Effect>& val);

    std::shared_ptr<WorldSound::Effect> val;
    Tempest::Vec3                       pos;

  friend class WorldSound;
  };


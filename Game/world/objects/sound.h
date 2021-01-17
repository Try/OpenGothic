#pragma once

#include "world/worldsound.h"

class Sound final {
  public:
    Sound();
    Sound(Sound&& other);
    Sound& operator = (Sound&& other);
    ~Sound();

    Tempest::Vec3 position()   const;
    bool          isEmpty()    const;
    bool          isFinished() const;

    void          setOcclusion(float occ);
    void          setVolume(float v);

    void          setMaxDistance(float v);
    void          setRefDistance(float v);

    void          setPosition(float x,float y,float z);

    void          setLooping(bool l);
    void          setActive(bool a);
    void          play();

  private:
    Sound(const std::shared_ptr<WorldSound::Effect>& val);

    std::shared_ptr<WorldSound::Effect> val;
    Tempest::Vec3                       pos;

  friend class WorldSound;
  };


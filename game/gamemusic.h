#pragma once

#include <zenkit/addon/daedalus.hh>

#include <memory>
#include <Tempest/SoundDevice>
#include <Tempest/SoundEffect>

class GameMusic final {
  public:
    GameMusic();
    GameMusic(const GameMusic&)=delete;
    ~GameMusic();

    static GameMusic& inst();

    enum Music : uint8_t {
      SysLoading
      };

    enum Tags : uint8_t {
      Day = 0,
      Ngt = 1<<0,

      Std = 0,
      Fgt = 1<<1,
      Thr = 1<<2
      };

    static Tags mkTags(Tags daytime,Tags mode);

    void      setEnabled(bool e);
    bool      isEnabled() const;
    void      setMusic(Music m);
    void      setMusic(const zenkit::IMusicTheme &theme, Tags t);
    void      stopMusic();

  private:
    struct MusicProvider;
    struct OpenGothicMusicProvider;
    struct GothicKitMusicProvider;

    void      setupSettings();

    static GameMusic* instance;

    int                  provider = 0;
    Tempest::SoundDevice device;
    Tempest::SoundEffect sound;
    MusicProvider*       impl = nullptr;
  };

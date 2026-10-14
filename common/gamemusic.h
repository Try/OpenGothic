#pragma once

#include <zenkit/addon/daedalus.hh>

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

    // Diathic: our main-menu track (assets/diathic/menu.ogg) instead of the SYS_Menu theme; any other music stops it
    void      playMenuTrack();
    void      tick(); // loops the menu track

  private:
    struct MusicProvider;
    struct OpenGothicMusicProvider;
    struct GothicKitMusicProvider;

    void      setupSettings();

    static GameMusic* instance;

    struct {
      zenkit::IMusicTheme theme = {};
      Tags                tags  = Tags::Std;
      } currentMusic;

    void      stopMenuTrack();

    int                  provider = -1;
    Tempest::SoundDevice device;
    Tempest::SoundEffect sound;
    MusicProvider*       impl = nullptr;

    Tempest::SoundEffect menuTrack;
    bool                 menuTrackOn     = false;
    bool                 menuTrackPaused = false; // music switched off in the options
  };

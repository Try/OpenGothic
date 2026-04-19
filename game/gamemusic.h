#pragma once

#include <zenkit/addon/daedalus.hh>

#include <Tempest/SoundDevice>
#include <Tempest/SoundEffect>

#include <string>
#include <vector>

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

    // Per-channel diagnostic info (for console commands)
    struct ChannelInfo {
      size_t      index        = 0;      // sequential index shown in 'music list'
      uint32_t    pChannel     = 0;      // DirectMusic channel number
      uint32_t    dwPatch      = 0;      // MIDI patch (bank<<16 | program)
      std::string name;                  // instrument name from DLS
      float       volume       = 1.f;
      float       pan          = 0.5f;
      size_t      activeVoices = 0;
      bool        isMuted      = false;
      bool        isSolo       = false;
      };

    void      setEnabled(bool e);
    bool      isEnabled() const;
    void      setMusic(Music m);
    void      setMusic(const zenkit::IMusicTheme &theme, Tags t);
    void      stopMusic();

    // Channel diagnostics / isolation
    void      getChannelStates(std::vector<ChannelInfo>& out) const;
    void      setChannelSolo (size_t index);
    void      setChannelMuted(size_t index, bool muted);
    void      clearChannelIsolation();

  private:
    struct MusicProvider;
    struct DXMusicProvider;
    struct OpenGothicMusicProvider;
    struct GothicKitMusicProvider;

    void      setupSettings();

    static GameMusic* instance;

    struct {
      zenkit::IMusicTheme theme = {};
      Tags                tags  = Tags::Std;
      } currentMusic;

    int                  provider = -1;
    Tempest::SoundDevice device;
    Tempest::SoundEffect sound;
    MusicProvider*       impl = nullptr;
  };

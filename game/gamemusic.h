#pragma once

#include <phoenix/ext/daedalus_classes.hh>

#include <memory>

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
    void      setMusic(const phoenix::c_music_theme &theme, Tags t);
    void      stopMusic();

  private:
    struct Impl;
    struct MusicProducer;

    void      setupSettings();

    static GameMusic* instance;
    std::unique_ptr<Impl> impl;
  };


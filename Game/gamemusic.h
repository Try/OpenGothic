#pragma once

#include <memory>

namespace Daedalus {
namespace GEngineClasses {
class C_MusicTheme;
}
}

class GameMusic final {
  public:
    GameMusic();
    GameMusic(const GameMusic&)=delete;
    ~GameMusic();

    enum Music : uint8_t {
      SysMenu,
      SysLoading
      };

    void      setMusic(const Daedalus::GEngineClasses::C_MusicTheme &theme);
    void      stopMusic();

  private:
    struct Impl;
    struct MusicProducer;
    std::unique_ptr<Impl> impl;
  };


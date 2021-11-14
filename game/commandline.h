#pragma once

#include <Tempest/Dir>

#include <cstdint>
#include <string>

class VersionInfo;

class CommandLine {
  public:
    CommandLine(int argc,const char** argv);
    static const CommandLine& inst();

    enum GraphicBackend : uint8_t {
      Vulkan,
      DirectX12
      };
    auto                graphicsApi() const -> GraphicBackend;
    std::u16string_view rootPath() const;
    std::u16string_view scriptPath() const;
    std::u16string      nestedPath(const std::initializer_list<const char16_t*> &name, Tempest::Dir::FileType type) const;

    bool                isDebugMode()  const { return isDebug;  }
    bool                isRamboMode()  const { return isRambo;  }
    bool                isWindowMode() const { return isWindow; }
    bool                doStartMenu()  const { return !noMenu; }

    std::string         saveDef;
    std::string         wrldDef;
    std::string         modDef;
    bool                noFrate = false;

  private:
    bool                validateGothicPath() const;

    GraphicBackend      graphics = GraphicBackend::Vulkan;
    std::u16string      gpath, gscript;
    bool                noMenu   = false;
    bool                isWindow = false;
    bool                isDebug  = false;
    bool                isRambo  = false;
  };


#pragma once

#include <Tempest/Platform>
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
    std::u16string_view modPath() const { return gmod; }
    std::u16string      nestedPath(const std::initializer_list<const char16_t*> &name, Tempest::Dir::FileType type) const;

    bool                isDebugMode()   const { return isDebug;  }
    bool                isWindowMode()  const { return isWindow; }
    bool                isRayQuery()    const { return isRQuery; }
    bool                isMeshShading() const { return isMeshSh; }
    bool                doStartMenu()   const { return !noMenu;  }
    bool                doForceG1()     const { return forceG1;  }
    bool                doForceG2()     const { return forceG2;  }
    std::string_view    defaultSave()   const { return saveDef;  }

    std::string         wrldDef;
    bool                noFrate = false;

  private:
    bool                validateGothicPath() const;

    GraphicBackend      graphics = GraphicBackend::Vulkan;
    std::u16string      gpath, gscript, gmod;
    std::string         saveDef;
    bool                noMenu   = false;
    bool                isWindow = false;
    bool                isDebug  = false;
#if defined(__OSX__)
    bool                isRQuery = false;
#else
    bool                isRQuery = true;
#endif
    bool                isMeshSh = true;
    bool                forceG1  = false;
    bool                forceG2  = false;
  };


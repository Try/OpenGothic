#pragma once

#include <Tempest/Platform>
#include <Tempest/Dir>

#include <cstdint>
#include <string>

#include "game/constants.h"

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
    std::u16string      scriptPath() const;
    std::u16string      scriptPath(ScriptLang lang) const;
    std::u16string      cutscenePath() const;
    std::u16string      cutscenePath(ScriptLang lang) const;
    std::u16string_view modPath() const { return gmod; }
    std::u16string      nestedPath(const std::initializer_list<const char16_t*> &name, Tempest::Dir::FileType type) const;

    bool                isDevMode()        const { return devmode;   }
    bool                isValidationMode() const { return isDebug;   }
    bool                isWindowMode()     const { return isWindow;  }
    bool                isRayQuery()       const { return isRQuery;  }
    bool                isRtGi()           const { return isGi;      }
    bool                isMeshShading()    const { return isMeshSh;  }
    bool                doStartMenu()      const { return !noMenu;   }
    bool                doForceG1()        const { return forceG1;   }
    bool                doForceG2()        const { return forceG2;   }
    bool                doForceG2NR()      const { return forceG2NR; }
    std::string_view    defaultSave()      const { return saveDef;   }

    std::string         wrldDef;

  private:
    bool                validateGothicPath() const;

    GraphicBackend      graphics = GraphicBackend::Vulkan;
    std::u16string      gpath, gmod;
    std::u16string      gscript;
    std::u16string      gcutscene;
    std::string         saveDef;
    bool                devmode   = false;
    bool                noMenu    = false;
    bool                isWindow  = false;
    bool                isDebug   = false;
#if defined(__OSX__)
    bool                isRQuery  = false;
    bool                isMeshSh  = false;
#else
    bool                isRQuery  = true;
    bool                isMeshSh  = true;
#endif
    bool                isGi      = false;
    bool                forceG1   = false;
    bool                forceG2   = false;
    bool                forceG2NR = false;
  };


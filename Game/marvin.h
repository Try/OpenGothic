#pragma once

#include <string>
#include <map>
#include <functional>

class Gothic;
class World;

class Marvin {
  public:
    Marvin(Gothic& gothic);

    void autoComplete(std::string& inputString);
    bool exec(const std::string& v);

  private:
    typedef std::function<bool(Marvin*, const std::string&)> CommandFunction;
    typedef std::map<std::string, CommandFunction> CommandMap;

    Gothic& gothic;
    CommandMap commandHandlers;

    CommandMap::iterator recognize(const std::string& inputString);

    bool handleCheatFull(const std::string& arguments);
    bool handleCameraAutoSwitch(const std::string& arguments);
    bool handleCameraMode(const std::string& arguments);
    bool handleToggleCamDebug(const std::string& arguments);
    bool handleToggleCamera(const std::string& arguments);
    bool handleInsert(const std::string& arguments);
  };


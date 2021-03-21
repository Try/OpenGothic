#include "marvin.h"

#include <initializer_list>
#include <cstdint>

#include "world/objects/npc.h"
#include "camera.h"
#include "gothic.h"
#include "utils/stringutil.h"

Marvin::Marvin(Gothic& gothic)
: gothic( gothic )
, commandHandlers()
{
  commandHandlers["cheat full"] = CommandFunction(&Marvin::handleCheatFull);
  commandHandlers["camera autoswitch"] = CommandFunction(&Marvin::handleCameraAutoSwitch);
  commandHandlers["camera mode"] = CommandFunction(&Marvin::handleCameraMode);
  commandHandlers["toggle camdebug"] = CommandFunction(&Marvin::handleToggleCamDebug);
  commandHandlers["toggle camera"] = CommandFunction(&Marvin::handleToggleCamera);
  commandHandlers["insert"] = CommandFunction(&Marvin::handleInsert);
  }

void Marvin::autoComplete(std::string& inputString) {
  inputString = StringUtil::trimFront(inputString, ' ');
  CommandMap::iterator it;

  size_t matchCount = 0;
  std::string matchCommand;

  for(it = commandHandlers.begin(); it != commandHandlers.end(); it++) {
    const std::string command(it->first);

    if(inputString.length() < command.length()) {
      std::string restOfCommand( command.substr(inputString.length()));
      std::string::size_type spacePos = restOfCommand.find(" ");
      std::string commandToken(command);

      if( spacePos != std::string::npos ) {
        commandToken = command.substr(0, inputString.length() + spacePos);
      }


      if(commandToken.substr(0,inputString.length()) == inputString) {
        if(matchCommand != commandToken) {
          matchCommand = commandToken;
          matchCount++;
        }
      }
    }
  }

  if(matchCount == 1) {
    inputString = matchCommand;
  }
  }

bool Marvin::exec(const std::string& inputString) {
  bool ret = false;
  size_t matchCount = 0;
  std::string matchCommand("");

  std::string trimedInputString = StringUtil::trimFront(inputString, ' ');

  CommandMap::iterator it;
  for(it = commandHandlers.begin(); it != commandHandlers.end(); it++) {
    std::string command(it->first);

    if(trimedInputString.length() >= command.length()) {
      if(trimedInputString.substr(0, command.length()) == command) {
        matchCommand = command;
        matchCount++;
      }
    }
  }

  if( matchCount == 1 ) {
    CommandMap::iterator it = commandHandlers.find(matchCommand);
    if( it != commandHandlers.end() ) {
      CommandFunction commandFunction = it->second;
      if( trimedInputString.length() > matchCommand.length() + 1)
      {
        std::string arguments(trimedInputString.substr(matchCommand.length() + 1));
        ret = commandFunction(this, arguments);
      } else {
        ret = commandFunction(this, "");
      }
    }
  }

  return ret;
  }

bool Marvin::handleCheatFull(const std::string& arguments) {
  if(auto pl = gothic.player())
    pl->changeAttribute(Npc::ATR_HITPOINTS,pl->attribute(Npc::ATR_HITPOINTSMAX),false);

  return true;
  }

bool Marvin::handleCameraAutoSwitch(const std::string& arguments) {
  return true;
  }

bool Marvin::handleCameraMode(const std::string& arguments) {
  return true;
  }

bool Marvin::handleToggleCamDebug(const std::string& arguments) {
  if(auto c = gothic.camera())
    c->toogleDebug();

  return true;
  }

bool Marvin::handleToggleCamera(const std::string& arguments) {
  if(auto c = gothic.camera())
    c->setToogleEnable(!c->isToogleEnabled());

  return true;
  }

bool Marvin::handleInsert(const std::string& arguments) {
  bool ret = false;

  World* world = gothic.world();
  Npc* player = gothic.player();

  if( world == nullptr || player == nullptr ) {
    ret = false;
  } else{
    ret = world->addItemOrNpcBySymbolName(arguments, player->position());
  }

  return ret;
  }

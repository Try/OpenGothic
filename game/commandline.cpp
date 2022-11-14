#include "commandline.h"

#include <Tempest/Log>
#include <Tempest/TextCodec>
#include <cstring>

#include "gothic.h"

using namespace Tempest;

static CommandLine* instance = nullptr;

CommandLine::CommandLine(int argc, const char** argv) {
  instance = this;
  if(argc<1)
    return;

  std::string_view mod;
  for(int i=1;i<argc;++i) {
    std::string_view arg = argv[i];
    if(arg.find("-game:")==0) {
      if(!mod.empty())
        Log::e("-game specified twice");
      mod = arg.substr(6);
      }
    if(arg=="-g") {
      ++i;
      if(i<argc)
        gpath.assign(argv[i],argv[i]+std::strlen(argv[i]));
      }
    else if(arg=="-save") {
      ++i;
      if(i<argc){
        if(std::strcmp(argv[i],"q")==0) {
          saveDef = "save_slot_0.sav";
          } else {
          saveDef = std::string("save_slot_")+argv[i]+".sav";
          }
        }
      }
    else if(arg=="-w") {
      ++i;
      if(i<argc)
        wrldDef = argv[i];
      }
    else if(arg=="-window") {
      isWindow = true;
      }
    else if(arg=="-nomenu") {
      noMenu   = true;
      }
    else if(arg=="-nofrate") {
      noFrate  = true;
      }
    else if(arg=="-g1") {
      forceG1 = true;
      }
    else if(arg=="-g2") {
      forceG2 = true;
      }
    else if(arg=="-dx12") {
      graphics = GraphicBackend::DirectX12;
      }
    else if(arg=="-validation" || arg=="-v") {
      isDebug  = true;
      }
    else if(arg=="-rt") {
      ++i;
      if(i<argc)
        isRQuery = (std::string_view(argv[i])!="0" && std::string_view(argv[i])!="false");
      }
    else if(arg=="-ms") {
      ++i;
      if(i<argc)
        isMeshSh = (std::string_view(argv[i])!="0" && std::string_view(argv[i])!="false");
      }
    }
  gmod = TextCodec::toUtf16(std::string(mod));
  }

const CommandLine& CommandLine::inst() {
  return *instance;
  }

CommandLine::GraphicBackend CommandLine::graphicsApi() const {
  return graphics;
  }

std::u16string_view CommandLine::rootPath() const {
  return gpath;
  }

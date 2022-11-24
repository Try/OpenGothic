#include "commandline.h"

#include <Tempest/Log>
#include <Tempest/TextCodec>
#include <cstring>

#include "gothic.h"

#include "utils/installdetect.h"
#include "utils/fileutil.h"

using namespace Tempest;
using namespace FileUtil;

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

  if(gpath.empty()) {
    InstallDetect inst;
    gpath = inst.detectG2();
    }

  for(auto& i:gpath)
    if(i=='\\')
      i='/';

  if(gpath.size()>0 && gpath.back()!='/')
    gpath.push_back('/');

  gscript = nestedPath({u"_work",u"Data",u"Scripts",u"_compiled"},Dir::FT_Dir);
  gmod    = TextCodec::toUtf16(std::string(mod));
  if(!gmod.empty())
    gmod = nestedPath({u"system",gmod.c_str()},Dir::FT_File);

  if(!validateGothicPath()) {
    Log::e("invalid gothic path: \"",TextCodec::toUtf8(gpath),"\"");
    throw std::logic_error("gothic not found!"); //TODO: user-friendly message-box
    }
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

std::u16string_view CommandLine::scriptPath() const {
  return gscript;
  }

std::u16string CommandLine::nestedPath(const std::initializer_list<const char16_t*>& name, Tempest::Dir::FileType type) const {
  return FileUtil::nestedPath(gpath, name, type);
  }

bool CommandLine::validateGothicPath() const {
  if(gpath.empty())
    return false;
  if(!FileUtil::exists(gscript))
    return false;
  if(!FileUtil::exists(nestedPath({u"Data"},Dir::FT_Dir)))
    return false;
  if(!FileUtil::exists(nestedPath({u"_work",u"Data"},Dir::FT_Dir)))
    return false;
  return true;
  }

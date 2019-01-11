#include "gothic.h"

#include <cstring>

Gothic::Gothic(const int argc, const char **argv) {
  if(argc<1)
    return;

  for(int i=1;i<argc;++i){
    if(std::strcmp(argv[i],"-g")==0){
      ++i;
      if(i<argc) {
        gpath=argv[i];
        for(auto& i:gpath)
          if(i=='\\')
            i='/';
        }
      }
    }

  if(gpath.size()>0 && gpath.back()!='/')
    gpath.push_back('/');
  }

std::unique_ptr<Daedalus::DaedalusVM> Gothic::createVm(const char *datFile) {
  auto vm = std::make_unique<Daedalus::DaedalusVM>(gpath+datFile);
  Daedalus::registerGothicEngineClasses(*vm);
  return vm;
  }

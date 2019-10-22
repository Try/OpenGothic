#include "visualfxdefinitions.h"

#include <Tempest/Log>

#include "graphics/visualfx.h"
#include "gothic.h"

using namespace Tempest;

VisualFxDefinitions::VisualFxDefinitions(Gothic& gothic) {
  vm = gothic.createVm(u"VisualFx.dat");
  }

VisualFxDefinitions::~VisualFxDefinitions() {
  vm->clearReferences(Daedalus::IC_Pfx);
  }

const VisualFx *VisualFxDefinitions::get(const char *name) {
  auto it = vfx.find(name);
  if(it!=vfx.end())
    return it->second.get();
  auto def = implGet(name);
  if(def==nullptr)
    return nullptr;
  std::unique_ptr<VisualFx> p{new VisualFx(std::move(*def))};
  auto ret = vfx.insert(std::make_pair<std::string,std::unique_ptr<VisualFx>>(name,std::move(p)));
  return ret.first->second.get();
  }

Daedalus::GEngineClasses::CFx_Base *VisualFxDefinitions::implGet(const char *name) {
  static Daedalus::GEngineClasses::CFx_Base ret={};
  if(!vm)
    return nullptr;

  auto id = vm->getDATFile().getSymbolIndexByName(name);
  if(id==size_t(-1)) {
    Log::e("invalid visual effect: \"",name,"\"");
    return nullptr;
    }

  vm->initializeInstance(ret, id, Daedalus::IC_Vfx);
  vm->clearReferences(Daedalus::IC_Vfx);
  return &ret;
  }

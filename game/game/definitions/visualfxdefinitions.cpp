#include "visualfxdefinitions.h"

#include <Tempest/Log>

#include "graphics/visualfx.h"
#include "gothic.h"

using namespace Tempest;

VisualFxDefinitions::VisualFxDefinitions() {
  vm = Gothic::inst().createVm(u"VisualFx.dat");
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
  auto ret = vfx.insert(std::make_pair<std::string,std::unique_ptr<VisualFx>>(name,nullptr));
  ret.first->second.reset(new VisualFx(std::move(*def),*vm,name));
  auto& vfx = *ret.first->second;

  vfx.emFXCreate      = get(def->emFXCreate_S.c_str());
  vfx.emFXCollStat    = get(def->emFXCollStat_S.c_str());
  vfx.emFXCollDyn     = get(def->emFXCollDyn_S.c_str());
  vfx.emFXCollDynPerc = get(def->emFXCollDynPerc_S.c_str());

  return &vfx;
  }

Daedalus::GEngineClasses::CFx_Base *VisualFxDefinitions::implGet(const char *name) {
  static Daedalus::GEngineClasses::CFx_Base ret={};
  if(!vm || name==nullptr || name[0]=='\0')
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

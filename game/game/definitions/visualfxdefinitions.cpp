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

const VisualFx *VisualFxDefinitions::get(std::string_view name) {
  std::string cname = std::string(name);
  auto it = vfx.find(cname);
  if(it!=vfx.end())
    return it->second.get();

  Daedalus::GEngineClasses::CFx_Base def;
  if(!implGet(cname,def))
    return nullptr;

  auto ret = vfx.insert(std::make_pair<std::string,std::unique_ptr<VisualFx>>(std::move(cname),nullptr));
  ret.first->second.reset(new VisualFx(def,*vm,name));

  auto& vfx = *ret.first->second;
  vfx.dbgName = name.data();

  return &vfx;
  }

bool VisualFxDefinitions::implGet(std::string_view name,
                                  Daedalus::GEngineClasses::CFx_Base& ret) {
  if(!vm || name.empty())
    return false;

  char buf[256] = {};
  std::snprintf(buf,sizeof(buf),"%.*s",int(name.size()),name.data());
  auto id = vm->getDATFile().getSymbolIndexByName(buf);
  if(id==size_t(-1)) {
    Log::e("invalid visual effect: \"",buf,"\"");
    return false;
    }

  vm->initializeInstance(ret, id, Daedalus::IC_Vfx);
  vm->clearReferences(Daedalus::IC_Vfx);
  return true;
  }

#include "visualfxdefinitions.h"

#include <Tempest/Log>

#include "graphics/visualfx.h"
#include "gothic.h"

using namespace Tempest;

VisualFxDefinitions::VisualFxDefinitions() {
  vm = Gothic::inst().createPhoenixVm("VisualFx.dat");
  }

VisualFxDefinitions::~VisualFxDefinitions() {
  }

const VisualFx* VisualFxDefinitions::get(std::string_view name) {
  std::string cname = std::string(name);
  auto it = vfx.find(cname);
  if(it!=vfx.end())
    return it->second.get();

  auto def = implGet(cname);
  if(def == nullptr)
    return nullptr;

  auto ret = vfx.insert(std::make_pair<std::string,std::unique_ptr<VisualFx>>(std::move(cname),nullptr));
  ret.first->second.reset(new VisualFx(*def,*vm,name));

  auto& vfx = *ret.first->second;
  vfx.dbgName = name.data();

  return &vfx;
  }

std::shared_ptr<phoenix::c_fx_base> VisualFxDefinitions::implGet(std::string_view name) {
  if(!vm || name.empty())
    return nullptr;

  char buf[256] = {};
  std::snprintf(buf,sizeof(buf),"%.*s",int(name.size()),name.data());
  auto id = vm->find_symbol_by_name(buf);
  if(id==nullptr) {
    Log::e("invalid visual effect: \"",buf,"\"");
    return nullptr;
    }

  try {
    return vm->init_instance<phoenix::c_fx_base>(id);
    } catch (const phoenix::script_error&) {
    return nullptr;
    }
  }

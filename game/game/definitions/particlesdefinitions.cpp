#include "particlesdefinitions.h"

#include <Tempest/Log>

#include "graphics/pfx/particlefx.h"
#include "utils/fileext.h"
#include "gothic.h"

using namespace Tempest;

ParticlesDefinitions::ParticlesDefinitions() {
  vm = Gothic::inst().createPhoenixVm("ParticleFx.dat");
  }

ParticlesDefinitions::~ParticlesDefinitions() {
  }

const ParticleFx* ParticlesDefinitions::get(std::string_view name, bool relaxed) {
  if(name.empty())
    return nullptr;

  while(FileExt::hasExt(name,"PFX"))
    name = name.substr(0,name.size()-4);

  std::lock_guard<std::recursive_mutex> guard(sync);
  return implGet(name,relaxed);
  }

const ParticleFx* ParticlesDefinitions::get(const ParticleFx* base, const VisualFx::Key* key) {
  if(base==nullptr || key==nullptr)
    return base;
  std::lock_guard<std::recursive_mutex> guard(sync);
  return implGet(*base,*key);
  }

const ParticleFx* ParticlesDefinitions::implGet(std::string_view name, bool relaxed) {
  auto cname = std::string(name);
  auto it    = pfx.find(cname);
  if(it!=pfx.end())
    return it->second.get();
  auto decl=implGetDirect(name, relaxed);
  if(!decl)
    return nullptr;
  std::unique_ptr<ParticleFx> p{new ParticleFx(*decl,name)};
  auto elt = pfx.insert(std::make_pair(std::move(cname),std::move(p)));

  return elt.first->second.get();
  }

const ParticleFx* ParticlesDefinitions::implGet(const ParticleFx& base, const VisualFx::Key& key) {
  auto it = pfxKey.find(&key);
  if(it!=pfxKey.end())
    return it->second.get();

  std::unique_ptr<ParticleFx> p{new ParticleFx(base,key)};
  auto elt = pfxKey.insert(std::make_pair(&key,std::move(p)));

  return elt.first->second.get();
  }

std::shared_ptr<phoenix::c_particle_fx> ParticlesDefinitions::implGetDirect(std::string_view name, bool relaxed) {
  if(!vm || name.empty())
    return nullptr;

  char buf[256] = {};
  std::snprintf(buf,sizeof(buf),"%.*s",int(name.size()),name.data());
  auto id = vm->find_symbol_by_name(buf);
  if(id==nullptr) {
    if(!relaxed)
      Log::e("invalid particle system: \"",buf,"\"");
    return nullptr;
    }

  return vm->init_instance<phoenix::c_particle_fx>(id);
  }

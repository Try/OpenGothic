#include "cameradefinitions.h"

#include "gothic.h"

CameraDefinitions::CameraDefinitions() {
  auto vm = Gothic::inst().createPhoenixVm("Camera.dat");
  auto parent = vm->loaded_script().find_symbol_by_name("CCAMSYS_DEF");

  for (auto& sym : vm->loaded_script().symbols()) {
    if (sym.type() == phoenix::daedalus::dt_instance && sym.parent() == parent->index()) {
      auto cam = vm->init_instance<phoenix::daedalus::c_camera>(&sym);
      cameras.emplace_back(sym.name(), std::move(cam));
    }
  }

  camModDialog    = getCam("CAMMODDIALOG");
  camModInventory = getCam("CAMMODINVENTORY");
  camModNormal    = getCam("CAMMODNORMAL");
  camModBack      = getCam("CAMMODLOOKBACK");
  camModFp        = getCam("CAMMODFIRSTPERSON");
  camModDeath     = getCam("CAMMODDEATH");
  camModMelee     = getCam("CAMMODMELEE");
  camModRange     = getCam("CAMMODRANGED");
  camModMage      = getCam("CAMMODMAGIC");
  camModSwim      = getCam("CAMMODSWIM");
  camModDive      = getCam("CAMMODDIVE");
  }

const phoenix::daedalus::c_camera& CameraDefinitions::mobsiCam(std::string_view tag, std::string_view pos) const {
  char name[256]={};

  if(!pos.empty()) {
    std::snprintf(name,sizeof(name),"CAMMODMOB%.*s_%.*s",int(tag.size()),tag.data(), int(pos.size()),pos.data());
    if(auto* c = find(name))
      return *c;
    }

  std::snprintf(name,sizeof(name),"CAMMODMOB%.*s",int(tag.size()),tag.data());
  if(auto* c = find(name))
    return *c;
  if(auto* c = find("CAMMODMOBDEFAULT"))
    return *c;
  return *camModNormal;
  }

phoenix::daedalus::c_camera* CameraDefinitions::getCam(std::string_view name) {
  for(auto& i:cameras)
    if(i.first==name)
      return i.second.get();

  return nullptr;
  }

const phoenix::daedalus::c_camera* CameraDefinitions::find(std::string_view name) const {
  for(auto& i:cameras)
    if(i.first==name)
      return i.second.get();
  return nullptr;
  }

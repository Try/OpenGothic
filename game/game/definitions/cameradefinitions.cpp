#include "cameradefinitions.h"

#include "gothic.h"

CameraDefinitions::CameraDefinitions() {
  auto vm = Gothic::inst().createPhoenixVm("Camera.dat");

  vm->enumerate_instances_by_class_name("CCAMSYS", [this, &vm](phoenix::symbol& s) {
    try {
      auto cam = vm->init_instance<phoenix::c_camera>(&s);
      cameras.emplace_back(s.name(), *cam);
      } catch (const phoenix::script_error&) {
      // There was an error initializing the c_camera. Ignore it.
      }
  });

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

const phoenix::c_camera& CameraDefinitions::mobsiCam(std::string_view tag, std::string_view pos) const {
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
  return camModNormal;
  }

phoenix::c_camera CameraDefinitions::getCam(std::string_view name) {
  for(auto& i:cameras)
    if(i.first==name)
      return i.second;

  return {};
  }

const phoenix::c_camera* CameraDefinitions::find(std::string_view name) const {
  for(auto& i:cameras)
    if(i.first==name)
      return &i.second;
  return nullptr;
  }

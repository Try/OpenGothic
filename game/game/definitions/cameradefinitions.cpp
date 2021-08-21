#include "cameradefinitions.h"

#include "gothic.h"

CameraDefinitions::CameraDefinitions() {
  auto vm = Gothic::inst().createVm("Camera.dat");
  vm->getDATFile().iterateSymbolsOfClass("CCamSys",[this,&vm](size_t i,Daedalus::PARSymbol& s){
    Camera cam={};

    vm->initializeInstance(cam, i, Daedalus::IC_CamSys);
    vm->clearReferences(Daedalus::IC_CamSys);

    cam.name = s.name;
    cameras.push_back(cam);
    });

  camModDialog    = loadCam("CAMMODDIALOG");
  camModInventory = loadCam("CAMMODINVENTORY");
  camModNormal    = loadCam("CAMMODNORMAL");
  camModBack      = loadCam("CAMMODLOOKBACK");
  camModFp        = loadCam("CAMMODFIRSTPERSON");
  camModDeath     = loadCam("CAMMODDEATH");
  camModMelee     = loadCam("CAMMODMELEE");
  camModRange     = loadCam("CAMMODRANGED");
  camModMage      = loadCam("CAMMODMAGIC");
  camModSwim      = loadCam("CAMMODSWIM");
  camModDive      = loadCam("CAMMODDIVE");

  vm->clearReferences(Daedalus::IC_CamSys);
  }

const Daedalus::GEngineClasses::CCamSys& CameraDefinitions::mobsiCam(std::string_view tag, std::string_view pos) const {
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

Daedalus::GEngineClasses::CCamSys CameraDefinitions::loadCam(std::string_view name) {
  for(auto& i:cameras)
    if(i.name==name)
      return i;
  return Daedalus::GEngineClasses::CCamSys();
  }

const CameraDefinitions::Camera* CameraDefinitions::find(std::string_view name) const {
  for(auto& i:cameras)
    if(i.name==name)
      return &i;
  return nullptr;
  }

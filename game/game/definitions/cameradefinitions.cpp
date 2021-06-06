#include "cameradefinitions.h"

#include "gothic.h"

CameraDefinitions::CameraDefinitions() {
  auto vm = Gothic::inst().createVm(u"Camera.dat");
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
  camModDeath     = loadCam("CAMMODDEATH");
  camModMelee     = loadCam("CAMMODMELEE");
  camModRange     = loadCam("CAMMODRANGED");
  camModMage      = loadCam("CAMMODMAGIC");
  camModSwim      = loadCam("CAMMODSWIM");
  camModDive      = loadCam("CAMMODDIVE");

  vm->clearReferences(Daedalus::IC_CamSys);
  }

const Daedalus::GEngineClasses::CCamSys& CameraDefinitions::mobsiCam(const char* tag, const char* pos) const {
  char name[256]={};

  if(pos!=nullptr) {
    std::snprintf(name,sizeof(name),"CAMMODMOB%s%s",tag,pos);
    if(auto* c = find(name))
      return *c;
    }

  std::snprintf(name,sizeof(name),"CAMMODMOB%s",tag);
  if(auto* c = find(name))
    return *c;
  if(auto* c = find("CAMMODMOBDEFAULT"))
    return *c;
  return camModNormal;
  }

Daedalus::GEngineClasses::CCamSys CameraDefinitions::loadCam(const char *name) {
  for(auto& i:cameras)
    if(i.name==name)
      return i;
  return Daedalus::GEngineClasses::CCamSys();
  }

const CameraDefinitions::Camera* CameraDefinitions::find(const char* name) const {
  for(auto& i:cameras)
    if(i.name==name)
      return &i;
  return nullptr;
  }

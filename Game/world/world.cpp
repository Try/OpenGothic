#include "world.h"

#include "gothic.h"
#include "resources.h"

#include <zenload/zCMesh.h>
#include <fstream>
#include <functional>
#include <daedalus/DaedalusDialogManager.h>

#include <Tempest/Log>

World::World(Gothic& gothic,const RendererStorage &storage, std::string file)
  :name(std::move(file)),gothic(&gothic) {
  using namespace Daedalus::GameState;

  ZenLoad::ZenParser parser(name,Resources::vdfsIndex());

  // TODO: update loader
  parser.readHeader();

  // TODO: update loader
  ZenLoad::oCWorldData world;
  parser.readWorld(world);

  ZenLoad::PackedMesh mesh;
  ZenLoad::zCMesh* worldMesh = parser.getWorldMesh();
  worldMesh->packMesh(mesh, 1.f, false);

  static_assert(sizeof(Resources::Vertex)==sizeof(ZenLoad::WorldVertex),"invalid landscape vertex format");
  const Resources::Vertex* vert=reinterpret_cast<const Resources::Vertex*>(mesh.vertices.data());
  vbo = Resources::loadVbo<Resources::Vertex>(vert,mesh.vertices.size());

  for(auto& i:mesh.subMeshes){
    blocks.emplace_back();
    Block& b = blocks.back();

    b.ibo     = Resources::loadIbo(i.indices.data(),i.indices.size());
    b.texture = Resources::loadTexture(i.material.texture);
    }

  std::sort(blocks.begin(),blocks.end(),[](const Block& a,const Block& b){ return a.texture<b.texture; });

  for(auto& vob:world.rootVobs)
    loadVob(vob);

  wayNet = std::move(world.waynet);

  wview.reset(new WorldView(*this,storage));

  vm.reset(new WorldScript(*this,gothic,"/_work/data/Scripts/_compiled/GOTHIC.DAT"));
  vm->initDialogs(gothic);
  initScripts(true);
  }

const ZenLoad::zCWaypointData *World::findPoint(const char *name) const {
  for(auto& i:wayNet.waypoints)
    if(i.wpName==name)
      return &i;
  for(auto& i:freePoints)
    if(i.wpName==name)
      return &i;
  return nullptr;
  }

StaticObjects::Mesh World::getView(const std::string &visual) {
  return view()->getView(visual);
  }

void World::updateAnimation() {
  auto& w=*vm;
  for(size_t i=0;i<w.npcCount();++i)
    w.npc(i).updateAnimation();
  }

int32_t World::runFunction(const std::string& fname, bool clearDataStack) {
  return vm->runFunction(fname,clearDataStack);
  }

void World::initScripts(bool firstTime) {
  auto dot=name.rfind('.');
  std::string startup = (firstTime ? "startup_" : "init_")+(dot==std::string::npos ? name : name.substr(0,dot));

  if(vm->hasSymbolName(startup))
    vm->runFunction(startup,true);
  }

void World::loadVob(const ZenLoad::zCVobData &vob) {
  for(auto& i:vob.childVobs)
    loadVob(i);

  if(vob.objectClass=="zCVob" ||
     vob.objectClass=="oCMobContainer:oCMobInter:oCMOB:zCVob"||
     vob.objectClass=="oCMobInter:oCMOB:zCVob" ||
     vob.objectClass=="oCMOB:zCVob" ||
     vob.objectClass=="oCMobDoor:oCMobInter:oCMOB:zCVob" ||
     vob.objectClass=="zCPFXControler:zCVob" ||
     vob.objectClass=="oCMobFire:oCMobInter:oCMOB:zCVob" ||
     vob.objectClass=="oCMobSwitch:oCMobInter:oCMOB:zCVob") {
    addStatic(vob);
    }
  else if(vob.objectClass=="zCVobAnimate:zCVob"){ // ork flags
    addStatic(vob); //TODO: morph animation
    }
  else if(vob.objectClass=="zCVobLevelCompo:zCVob"){
    return;
    }
  else if(vob.objectClass=="zCMover:zCTrigger:zCVob"){
    addStatic(vob); // castle gate
    }
  else if(vob.objectClass=="oCTriggerScript:zCTrigger:zCVob" ||
          vob.objectClass=="oCTriggerChangeLevel:zCTrigger:zCVob" ||
          vob.objectClass=="zCTrigger:zCVob"){
    }
  else if(vob.objectClass=="zCZoneZFog:zCVob" ||
          vob.objectClass=="zCZoneZFogDefault:zCZoneZFog:zCVob"){
    }
  else if(vob.objectClass=="zCZoneVobFarPlaneDefault:zCZoneVobFarPlane:zCVob" ||
          vob.objectClass=="zCVobLensFlare:zCVob"){
    return;
    }
  else if(vob.objectClass=="zCVobStartpoint:zCVob") {
    ZenLoad::zCWaypointData point={};
    point.wpName   = vob.vobName;
    point.position = vob.position;
    startPoints.push_back(point);
    }
  else if(vob.objectClass=="zCVobSpot:zCVob") {
    ZenLoad::zCWaypointData point={};
    point.wpName   = vob.vobName;
    point.position = vob.position;
    freePoints.push_back(point);
    }
  else if(vob.objectClass=="oCItem:zCVob") {
    }
  else if(vob.objectClass=="zCVobSound" ||
          vob.objectClass=="zCVobSound:zCVob" ||
          vob.objectClass=="zCVobSoundDaytime:zCVobSound:zCVob") {
    }
  else if(vob.objectClass=="oCZoneMusic:zCVob" ||
          vob.objectClass=="oCZoneMusicDefault:oCZoneMusic:zCVob") {
    }
  else if(vob.objectClass=="zCVobLight:zCVob") {
    }
  else {
    static std::unordered_set<std::string> cls;
    if(cls.find(vob.objectClass)==cls.end()){
      cls.insert(vob.objectClass);
      Tempest::Log::d("unknown vob class ",vob.objectClass);
      }
    }
  }

void World::addStatic(const ZenLoad::zCVobData &vob) {
  Dodad d;
  d.mesh = Resources::loadMesh(vob.visual);
  if(d.mesh) {
    float v[16]={};
    std::memcpy(v,vob.worldMatrix.m,sizeof(v));
    d.objMat = Tempest::Matrix4x4(v);

    staticObj.emplace_back(d);
    }
  }

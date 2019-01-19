#include "resources.h"

#include <Tempest/MemReader>
#include <Tempest/Pixmap>
#include <Tempest/Device>
#include <Tempest/Dir>

#include <Tempest/Log>

#include <zenload/zCModelMeshLib.h>
#include <zenload/zCMorphMesh.h>
#include <zenload/zCProgMeshProto.h>
#include <zenload/ztex2dds.h>

#include "graphics/staticmesh.h"
#include "gothic.h"

using namespace Tempest;

Resources* Resources::inst=nullptr;

Resources::Resources(Gothic &gothic, Tempest::Device &device)
  : device(device), asset("data",device),gothic(gothic) {
  inst=this;

  menuFnt = Font("data/font/menu.ttf");
  menuFnt.setPixelSize(44);

  mainFnt = Font("data/font/main.ttf");
  mainFnt.setPixelSize(24);

  fallback = device.loadTexture("data/fallback.png");

  // TODO: priority for mod files
  Dir::scan(gothic.path()+"Data/",[this](const std::string& vdf,Dir::FileType t){
    if(t==Dir::FT_File)
      gothicAssets.loadVDF(this->gothic.path() + "Data/" + vdf);
    });
  gothicAssets.finalizeLoad();
  }

Resources::~Resources() {
  inst=nullptr;
  }

void Resources::addVdf(const char *vdf) {
  gothicAssets.loadVDF(gothic.path() + vdf);
  }

Tempest::Texture2d* Resources::implLoadTexture(const std::string& name) {
  if(name.size()==0)
    return nullptr;

  auto it=texCache.find(name);
  if(it!=texCache.end())
    return it->second.get();

  std::vector<uint8_t> data=getFileData(name);
  if(data.empty())
    return &fallback;

  try {
    Tempest::MemReader rd(data.data(),data.size());
    Tempest::Pixmap    pm(rd);
    //pm.save("debug.png");

    std::unique_ptr<Texture2d> t{new Texture2d(device.loadTexture(pm))};
    Texture2d* ret=t.get();
    texCache[name] = std::move(t);
    return ret;
    }
  catch(...){
    Log::e("unable to load texture \"",name,"\"");
    return &fallback;
    }
  }

StaticMesh *Resources::implLoadStMesh(const std::string &name) {
  if(name.size()==0)
    return nullptr;

  auto it=stMeshCache.find(name);
  if(it!=stMeshCache.end())
    return it->second.get();

  try {
    ZenLoad::PackedMesh packed;
    loadMesh(packed,name);
    std::unique_ptr<StaticMesh> t{new StaticMesh(packed)};
    StaticMesh* ret=t.get();
    stMeshCache[name] = std::move(t);
    return ret;
    }
  catch(...){
    Log::e("unable to load mesh \"",name,"\"");
    return nullptr;
    }
  }

void Resources::loadMesh(ZenLoad::PackedMesh& packed,std::string name) {
  std::vector<uint8_t> data;
  std::vector<uint8_t> dds;

  // Check if this isn't the compiled version
  if(name.rfind("-C")==std::string::npos) {
    // Strip the extension ".***"
    // Add "compiled"-extension
    if(name.rfind(".3DS")==name.size()-4)
      std::memcpy(&name[name.size()-3],"MRM",3); else
    if(name.rfind(".MMS")==name.size()-4)
      std::memcpy(&name[name.size()-3],"MMB",3); else
    if(name.rfind(".MDS")==name.size()-4)
      std::memcpy(&name[name.size()-3],"MDL",3); else
    if(name.rfind(".ASK")==name.size()-4)
      std::memcpy(&name[name.size()-3],"MDL",3);
    }

  if(name.rfind(".MRM")==name.size()-4) {
    // Try to load the mesh
    ZenLoad::zCProgMeshProto zmsh(name,gothicAssets);
    // Failed?
    if(zmsh.getNumSubmeshes() == 0)
      return;
    // Pack the mesh
    zmsh.packMesh(packed,1.f);
    }
  else if(name.rfind(".MMB")==name.size()-4) {
    ZenLoad::zCMorphMesh zmm(name,gothicAssets);
    // Failed?
    if(zmm.getMesh().getNumSubmeshes()==0)
      return;
    // Pack the mesh
    zmm.getMesh().packMesh(packed,1.f);
    }
  else if(name.rfind(".MDMS")==name.size()-5) {
    name=name.substr(0,name.size()-1);
    ZenLoad::zCModelMeshLib zlib(name,gothicAssets, 1.f);

    // Failed?
    if(!zlib.isValid())
      return;

    ZenLoad::PackedSkeletalMesh sp;
    zlib.packMesh(sp, 1.f);
    for(auto& m:zlib.getMeshes())
      m.getMesh().packMesh(packed, 1.f);
    }
  else if(name.rfind(".MDL")==name.size()-4){
    ZenLoad::zCModelMeshLib     library(name,gothicAssets,1.f);
    ZenLoad::PackedSkeletalMesh packed;
    ZenLoad::PackedSkeletalMesh sp;

    library.packMesh(sp,1.f);
    for(auto& m:library.getMeshes())
      m.packMesh(packed, 1.f);
    }
  }

Tempest::Texture2d* Resources::loadTexture(const char *name) {
  return inst->implLoadTexture(name);
  }

Tempest::Texture2d *Resources::loadTexture(const std::string &name) {
  return inst->implLoadTexture(name);
  }

StaticMesh *Resources::loadStMesh(const std::string &name) {
  return inst->implLoadStMesh(name);
  }

std::vector<uint8_t> Resources::getFileData(const char *name) {
  std::vector<uint8_t> data;
  inst->gothicAssets.getFileData(name,data);
  return data;
  }

std::vector<uint8_t> Resources::getFileData(const std::string &name) {
  std::vector<uint8_t> data;
  inst->gothicAssets.getFileData(name,data);
  if(data.size()!=0)
    return data;

  if(name.rfind(".TGA")==name.size()-4){
    auto n = name;
    n.resize(n.size()-4);
    n+="-C.TEX";
    inst->gothicAssets.getFileData(n,data);
    std::vector<uint8_t> ztex;
    ZenLoad::convertZTEX2DDS(data,ztex);
    //ZenLoad::convertDDSToRGBA8(data, ztex);
    return ztex;
    }
  return data;
  }

VDFS::FileIndex& Resources::vdfsIndex() {
  return inst->gothicAssets;
  }

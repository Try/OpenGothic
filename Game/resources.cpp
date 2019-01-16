#include "resources.h"

#include <Tempest/MemReader>
#include <Tempest/Pixmap>
#include <Tempest/Device>
#include <Tempest/Dir>

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
    //TODO: log
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
    //TODO: log
    return nullptr;
  }
  }

void Resources::loadMesh(ZenLoad::PackedMesh& packed,std::string name) {
  std::vector<uint8_t> data;
  std::vector<uint8_t> dds;

  // Check if this isn't the compiled version
  if(name.rfind("-C")==std::string::npos) {
    if(name.rfind(".3DS")==name.size()-4) {
      // Strip the ".3DS"
      std::memcpy(&name[name.size()-3],"MRM",3);
      // Add "compiled"-extension
//      vname += ".MRM";
      } else
    if(name.rfind(".MMS")==name.size()-4) {
      // Strip the ".MMS"
      // Add "compiled"-extension
      std::memcpy(&name[name.size()-3],"MMB",3);
      }
    }

  if(name.rfind(".MRM")==name.size()-4) {
    // Try to load the mesh
    ZenLoad::zCProgMeshProto zmsh(name,gothicAssets);
    // Failed?
    if(zmsh.getNumSubmeshes() == 0)
      return;
    // Pack the mesh
    zmsh.packMesh(packed,0.01f);
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

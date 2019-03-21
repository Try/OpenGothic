#include "resources.h"

#include <Tempest/MemReader>
#include <Tempest/Pixmap>
#include <Tempest/Device>
#include <Tempest/Dir>
#include <Tempest/Application>

#include <Tempest/Log>

#include <zenload/modelScriptParser.h>
#include <zenload/zCModelMeshLib.h>
#include <zenload/zCMorphMesh.h>
#include <zenload/zCProgMeshProto.h>
#include <zenload/zenParser.h>
#include <zenload/ztex2dds.h>

#include <fstream>

#include "graphics/submesh/staticmesh.h"
#include "graphics/submesh/animmesh.h"
#include "graphics/skeleton.h"
#include "graphics/protomesh.h"
#include "graphics/animation.h"
#include "graphics/attachbinder.h"
#include "physics/physicmeshshape.h"

#include "gothic.h"

using namespace Tempest;

Resources* Resources::inst=nullptr;

static void emplaceTag(char* buf, char tag){
  for(size_t i=1;buf[i];++i){
    if(buf[i]==tag && buf[i-1]=='_' && buf[i+1]=='0'){
      buf[i  ]='%';
      buf[i+1]='s';
      ++i;
      return;
      }
    }
  }

Resources::Resources(Gothic &gothic, Tempest::Device &device)
  : device(device), asset("data",device),gothic(gothic) {
  inst=this;

  menuFnt = Font("data/font/menu.ttf");
  menuFnt.setPixelSize(44);

  mainFnt = Font("data/font/main.ttf");
  mainFnt.setPixelSize(24);

  dlgFnt  = Font("data/font/main.ttf");
  dlgFnt.setPixelSize(32);

  fallback = device.loadTexture("data/fallback.png");

  // TODO: priority for *.mod files
  std::vector<std::u16string> archives;
  Dir::scan(gothic.path()+u"Data/",[this,&archives](const std::u16string& vdf,Dir::FileType t){
    if(t==Dir::FT_File)
      archives.push_back(this->gothic.path() + u"Data/" + vdf);
    });

  // addon archives first!
  std::sort(archives.begin(),archives.end(),[](const std::u16string& ua,const std::u16string& ub){
    std::string a(ua.begin(),ua.end()); //FIXME: unicode
    std::string b(ub.begin(),ub.end()); //FIXME: unicode
    return VDFS::FileIndex::getLastModTime(a) > VDFS::FileIndex::getLastModTime(b);
    });

  for(auto& i:archives) {
    std::string acii(i.begin(),i.end()); //FIXME: unicode
    gothicAssets.loadVDF(acii);
    }
  gothicAssets.finalizeLoad();
  }

Resources::~Resources() {
  inst=nullptr;
  }

Font Resources::fontByName(const std::string &fontName) {
  if(fontName=="FONT_OLD_10_WHITE.TGA"){
    return Resources::font();
    }
  else if(fontName=="FONT_OLD_10_WHITE_HI.TGA"){
    return Resources::font();
    }
  else if(fontName=="FONT_OLD_20_WHITE.TGA"){
    return Resources::menuFont();
    } else {
    return Resources::menuFont();
    }
  }

const Texture2d *Resources::fallbackTexture() {
  return &inst->fallback;
  }

VDFS::FileIndex& Resources::vdfsIndex() {
  return inst->gothicAssets;
  }

void Resources::addVdf(const char16_t *vdf) {
  auto path = gothic.path() + vdf;
  std::string acii(path.begin(),path.end()); //FIXME: unicode
  gothicAssets.loadVDF(acii);
  }

Tempest::Texture2d* Resources::implLoadTexture(const std::string& name) {
  if(name.size()==0)
    return nullptr;

  auto it=texCache.find(name);
  if(it!=texCache.end())
    return it->second.get();

  std::vector<uint8_t> data=getFileData(name);
  if(data.empty())
    return nullptr;

  try {
    Tempest::MemReader rd(data.data(),data.size());
    Tempest::Pixmap    pm(rd);

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

ProtoMesh* Resources::implLoadMesh(const std::string &name) {
  if(name.size()==0)
    return nullptr;

  auto it=aniMeshCache.find(name);
  if(it!=aniMeshCache.end())
    return it->second.get();

  if(name=="hum_body_Naked0.MDM")//"Sna_Body.MDM"
    Log::d("");

  try {
    ZenLoad::PackedMesh        sPacked;
    ZenLoad::zCModelMeshLib    library;
    auto                       code=loadMesh(sPacked,library,name);
    std::unique_ptr<ProtoMesh> t{code==MeshLoadCode::Static ? new ProtoMesh(sPacked) : new ProtoMesh(library)};
    ProtoMesh* ret=t.get();
    aniMeshCache[name] = std::move(t);
    if(code==MeshLoadCode::Static && sPacked.subMeshes.size()>0)
      phyMeshCache[ret].reset(PhysicMeshShape::load(sPacked));
    if(code==MeshLoadCode::Error)
      throw std::runtime_error("load failed");
    return ret;
    }
  catch(...){
    Log::e("unable to load mesh \"",name,"\"");
    return nullptr;
    }
  }

Skeleton* Resources::implLoadSkeleton(std::string name) {
  if(name.size()==0)
    return nullptr;

  if(name.rfind(".MDS")==name.size()-4 ||
     name.rfind(".mds")==name.size()-4)
    std::memcpy(&name[name.size()-3],"MDH",3);

  auto it=skeletonCache.find(name);
  if(it!=skeletonCache.end())
    return it->second.get();

  try {
    ZenLoad::zCModelMeshLib library(name,gothicAssets,1.f);
    std::unique_ptr<Skeleton> t{new Skeleton(library,name)};
    Skeleton* ret=t.get();
    skeletonCache[name] = std::move(t);
    if(!hasFile(name))
      throw std::runtime_error("load failed");
    return ret;
    }
  catch(...){
    Log::e("unable to load skeleton \"",name,"\"");
    return nullptr;
    }
  }

Animation *Resources::implLoadAnimation(std::string name) {
  if(name.size()==0)
    return nullptr;

  if(name.rfind(".MDS")==name.size()-4 ||
     name.rfind(".mds")==name.size()-4 ||
     name.rfind(".MDH")==name.size()-4)
    std::memcpy(&name[name.size()-3],"MSB",3);

  auto it=animCache.find(name);
  if(it!=animCache.end())
    return it->second.get();

  try {
    ZenLoad::ZenParser            zen(name,gothicAssets);
    ZenLoad::ModelScriptBinParser p(zen);

    std::unique_ptr<Animation> t{new Animation(p,name.substr(0,name.size()-4))};
    Animation* ret=t.get();
    animCache[name] = std::move(t);
    if(!hasFile(name))
      throw std::runtime_error("load failed");
    return ret;
    }
  catch(...){
    Log::e("unable to load animation \"",name,"\"");
    return nullptr;
    }
  }

bool Resources::hasFile(const std::string &fname) {
  std::lock_guard<std::recursive_mutex> g(inst->sync);
  return inst->gothicAssets.hasFile(fname);
  }

const Texture2d *Resources::loadTexture(const char *name) {
  std::lock_guard<std::recursive_mutex> g(inst->sync);
  return inst->implLoadTexture(name);
  }

const Tempest::Texture2d* Resources::loadTexture(const std::string &name) {
  std::lock_guard<std::recursive_mutex> g(inst->sync);
  return inst->implLoadTexture(name);
  }

const Texture2d *Resources::loadTexture(const std::string &name, int32_t iv, int32_t ic) {
  if(name.size()>=128)// || (iv==0 && ic==0))
    return loadTexture(name);

  char v[16]={};
  char c[16]={};
  char buf1[128]={};
  char buf2[128]={};

  std::snprintf(v,sizeof(v),"V%d",iv);
  std::snprintf(c,sizeof(c),"C%d",ic);
  std::strcpy(buf1,name.c_str());

  emplaceTag(buf1,'V');
  std::snprintf(buf2,sizeof(buf2),buf1,v);

  emplaceTag(buf2,'C');
  std::snprintf(buf1,sizeof(buf1),buf2,c);

  return loadTexture(buf1);
  }

const ProtoMesh *Resources::loadMesh(const std::string &name) {
  std::lock_guard<std::recursive_mutex> g(inst->sync);
  return inst->implLoadMesh(name);
  }

const Skeleton *Resources::loadSkeleton(const std::string &name) {
  std::lock_guard<std::recursive_mutex> g(inst->sync);
  return inst->implLoadSkeleton(name);
  }

const Animation *Resources::loadAnimation(const std::string &name) {
  std::lock_guard<std::recursive_mutex> g(inst->sync);
  return inst->implLoadAnimation(name);
  }

const PhysicMeshShape *Resources::physicMesh(const ProtoMesh *view) {
  std::lock_guard<std::recursive_mutex> g(inst->sync);
  if(view==nullptr)
    return nullptr;
  auto it = inst->phyMeshCache.find(view);
  if(it!=inst->phyMeshCache.end())
    return it->second.get();
  return nullptr;
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

Resources::MeshLoadCode Resources::loadMesh(ZenLoad::PackedMesh& sPacked, ZenLoad::zCModelMeshLib& library, std::string name) {
  std::vector<uint8_t> data;
  std::vector<uint8_t> dds;

  // Check if this isn't the compiled version
  if(name.rfind("-C")==std::string::npos) {
    // Strip the extension ".***"
    // Add "compiled"-extension
    if(name.rfind(".3DS")==name.size()-4)
      std::memcpy(&name[name.size()-3],"MRM",3); else
    if(name.rfind(".3ds")==name.size()-4)
      std::memcpy(&name[name.size()-3],"MRM",3); else
    if(name.rfind(".mms")==name.size()-4)
      std::memcpy(&name[name.size()-3],"MMB",3); else
    if(name.rfind(".MMS")==name.size()-4)
      std::memcpy(&name[name.size()-3],"MMB",3); else
    if(name.rfind(".ASC")==name.size()-4)
      std::memcpy(&name[name.size()-3],"MDL",3); else
    if(name.rfind(".asc")==name.size()-4)
      std::memcpy(&name[name.size()-3],"MDL",3);
    }

  if(name.rfind(".MRM")==name.size()-4) {
    ZenLoad::zCProgMeshProto zmsh(name,gothicAssets);
    if(zmsh.getNumSubmeshes()==0)
      return MeshLoadCode::Error;
    zmsh.packMesh(sPacked,1.f);
    return MeshLoadCode::Static;
    }

  if(name.rfind(".MMB")==name.size()-4) {
    ZenLoad::zCMorphMesh zmm(name,gothicAssets);
    if(zmm.getMesh().getNumSubmeshes()==0)
      return MeshLoadCode::Error;
    zmm.getMesh().packMesh(sPacked,1.f);
    return MeshLoadCode::Static;
    // return MeshLoadCode::Dynamic;
    }

  if(name.rfind(".MDMS")==name.size()-5 ||
     name.rfind(".MDS") ==name.size()-4 ||
     name.rfind(".MDL") ==name.size()-4 ||
     name.rfind(".MDM") ==name.size()-4 ||
     name.rfind(".mds") ==name.size()-4){
    library = loadMDS(name);
    return MeshLoadCode::Dynamic;
    }

  return MeshLoadCode::Error;
  }

ZenLoad::zCModelMeshLib Resources::loadMDS(std::string &name) {
  if(name.rfind(".MDMS")==name.size()-5){
    name.resize(name.size()-1);
    return ZenLoad::zCModelMeshLib(name,gothicAssets,1.f);
    }
  if(hasFile(name))
    return ZenLoad::zCModelMeshLib(name,gothicAssets,1.f);
  std::memcpy(&name[name.size()-3],"MDL",3);
  if(hasFile(name))
    return ZenLoad::zCModelMeshLib(name,gothicAssets,1.f);
  std::memcpy(&name[name.size()-3],"MDM",3);
  if(hasFile(name)) {
    ZenLoad::zCModelMeshLib lib(name,gothicAssets,1.f);
    std::memcpy(&name[name.size()-3],"MDH",3);
    if(hasFile(name)) {
      auto data=getFileData(name);
      if(!data.empty()){
        ZenLoad::ZenParser parser(data.data(), data.size());
        lib.loadMDH(parser,1.f);
        }
      }
    return lib;
    }
  std::memcpy(&name[name.size()-3],"MDH",3);
  if(hasFile(name))
    return ZenLoad::zCModelMeshLib(name,gothicAssets,1.f);
  return ZenLoad::zCModelMeshLib();
  }

const AttachBinder *Resources::bindMesh(const ProtoMesh &anim, const Skeleton &s, const char *defBone) {
  std::lock_guard<std::recursive_mutex> g(inst->sync);

  if(anim.submeshId.size()==0){
    static AttachBinder empty;
    return &empty;
    }
  BindK k = BindK(&s,&anim,defBone ? defBone : "");

  auto it = inst->bindCache.find(k);
  if(it!=inst->bindCache.end())
    return it->second.get();

  std::unique_ptr<AttachBinder> ret(new AttachBinder(s,anim,defBone));
  auto p = ret.get();
  inst->bindCache[k] = std::move(ret);
  return p;
  }

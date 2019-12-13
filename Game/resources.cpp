#include "resources.h"

#include <Tempest/MemReader>
#include <Tempest/Pixmap>
#include <Tempest/Device>
#include <Tempest/Dir>
#include <Tempest/Application>
#include <Tempest/Sound>
#include <Tempest/SoundEffect>
#include <Tempest/TextCodec>
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
#include "dmusic/music.h"
#include "dmusic/directmusic.h"
#include "utils/fileext.h"
#include "utils/gthfont.h"

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
  : device(device), gothic(gothic) {
  inst=this;

  static std::array<VertexFsq,6> fsqBuf =
   {{
      {-1,-1},{ 1,1},{1,-1},
      {-1,-1},{-1,1},{1, 1}
   }};
  fsq = Resources::loadVbo(fsqBuf.data(),fsqBuf.size());

  dxMusic.reset(new Dx8::DirectMusic());
  // G2
  dxMusic->addPath(gothic.nestedPath({u"_work",u"Data",u"Music",u"newworld"},  Dir::FT_Dir));
  dxMusic->addPath(gothic.nestedPath({u"_work",u"Data",u"Music",u"AddonWorld"},Dir::FT_Dir));
  // G1
  dxMusic->addPath(gothic.nestedPath({u"_work",u"Data",u"Music",u"dungeon"},  Dir::FT_Dir));
  dxMusic->addPath(gothic.nestedPath({u"_work",u"Data",u"Music",u"menu_men"}, Dir::FT_Dir));
  dxMusic->addPath(gothic.nestedPath({u"_work",u"Data",u"Music",u"orchestra"},Dir::FT_Dir));

  fBuff .reserve(8*1024*1024);
  ddsBuf.reserve(8*1024*1024);

  {
  Pixmap pm(1,1,Pixmap::Format::RGBA);
  uint8_t* pix = reinterpret_cast<uint8_t*>(pm.data());
  pix[0]=255;
  pix[3]=255;
  fallback = device.loadTexture(pm);
  }

  {
  Pixmap pm(1,1,Pixmap::Format::RGBA);
  fbZero = device.loadTexture(pm);
  }

  // TODO: priority for *.mod files
  std::vector<std::u16string> archives;
  detectVdf(archives,gothic.nestedPath({u"Data"},Dir::FT_Dir));

  // addon archives first!
  std::sort(archives.begin(),archives.end(),[](const std::u16string& a,const std::u16string& b){
    int aIsMod = (a.rfind(u".mod")==a.size()-4) ? 1 : 0;
    int bIsMod = (b.rfind(u".mod")==b.size()-4) ? 1 : 0;
    return std::make_tuple(aIsMod,VDFS::FileIndex::getLastModTime(a)) >
           std::make_tuple(bIsMod,VDFS::FileIndex::getLastModTime(b));
    });

  for(auto& i:archives)
    gothicAssets.loadVDF(i);
  gothicAssets.finalizeLoad();

  //auto v = getFileData("font_old_20_white.tga");
  //Tempest::WFile f("../../internal/font_old_20_white.tga");
  //f.write(v.data(),v.size());
  }

Resources::~Resources() {
  inst=nullptr;
  }

void Resources::detectVdf(std::vector<std::u16string> &ret, const std::u16string &root) {
  Dir::scan(root,[this,&root,&ret](const std::u16string& vdf,Dir::FileType t){
    if(t==Dir::FT_File) {
      auto file = root + vdf;
      if(VDFS::FileIndex::getLastModTime(file)>0 || vdf.rfind(u".mod")==vdf.size()-4)
        ret.emplace_back(std::move(file));
      return;
      }

    if(t==Dir::FT_Dir && vdf!=u".." && vdf!=u".") {
      auto dir = root + vdf + u"/";
      detectVdf(ret,dir);
      }
    });
  }

const GthFont& Resources::dialogFont() {
  return font("font_old_10_white.tga",FontType::Normal);
  }

const GthFont& Resources::font() {
  return font("font_old_10_white.tga",FontType::Normal);
  }

const GthFont &Resources::font(Resources::FontType type) {
  return font("font_old_10_white.tga",type);
  }

const GthFont &Resources::font(const char* fname, FontType type) {
  std::lock_guard<std::recursive_mutex> g(inst->sync);
  return inst->implLoadFont(fname,type);
  }

const Texture2d& Resources::fallbackTexture() {
  return inst->fallback;
  }

const Texture2d &Resources::fallbackBlack() {
  return inst->fbZero;
  }

VDFS::FileIndex& Resources::vdfsIndex() {
  return inst->gothicAssets;
  }

const Tempest::VertexBuffer<Resources::VertexFsq> &Resources::fsqVbo() {
  return inst->fsq;
  }

Tempest::Texture2d* Resources::implLoadTexture(std::string name) {
  if(name.size()==0)
    return nullptr;

  auto it=texCache.find(name);
  if(it!=texCache.end())
    return it->second.get();

  if(getFileData(name.c_str(),fBuff))
    return implLoadTexture(std::move(name),fBuff);

  if(FileExt::hasExt(name,"TGA")){
    auto n = name;
    n.resize(n.size()+2);
    std::memcpy(&n[0]+n.size()-6,"-C.TEX",6);
    if(!getFileData(n.c_str(),fBuff))
      return nullptr;
    ddsBuf.clear();
    ZenLoad::convertZTEX2DDS(fBuff,ddsBuf);
    return implLoadTexture(std::move(name),ddsBuf);
    }

  return nullptr;
  }

Texture2d *Resources::implLoadTexture(std::string&& name,const std::vector<uint8_t> &data) {
  try {
    Tempest::MemReader rd(data.data(),data.size());
    Tempest::Pixmap    pm(rd);

    std::unique_ptr<Texture2d> t{new Texture2d(device.loadTexture(pm))};
    Texture2d* ret=t.get();
    texCache[std::move(name)] = std::move(t);
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

  if(name=="BSSHARP_OC.MDS")//"Sna_Body.MDM"
    Log::d("");

  if(FileExt::hasExt(name,"TGA")){
    static std::unordered_set<std::string> dec;
    if(dec.find(name)==dec.end()) {
      Log::e("decals are not implemented yet \"",name,"\"");
      dec.insert(name);
      }
    return nullptr;
    }

  try {
    ZenLoad::PackedMesh        sPacked;
    ZenLoad::zCModelMeshLib    library;
    auto                       code=loadMesh(sPacked,library,name);
    std::unique_ptr<ProtoMesh> t{code==MeshLoadCode::Static ? new ProtoMesh(sPacked,name) : new ProtoMesh(library,name)};
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

  FileExt::exchangeExt(name,"MDS","MDH") ||
  FileExt::exchangeExt(name,"ASC","MDL");

  auto it=skeletonCache.find(name);
  if(it!=skeletonCache.end())
    return it->second.get();

  if(name=="BARBQ_SCAV.MDL")
    Log::e("");

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
  if(name.size()<4)
    return nullptr;

  auto it=animCache.find(name);
  if(it!=animCache.end())
    return it->second.get();

  try {
    Animation* ret=nullptr;
    if(gothic.version().game==2){
      FileExt::exchangeExt(name,"MDS","MSB") ||
      FileExt::exchangeExt(name,"MDH","MSB");

      if(name=="Orc.MSB")
        Log::d("");

      ZenLoad::ZenParser            zen(name,gothicAssets);
      ZenLoad::MdsParserBin         p(zen);

      std::unique_ptr<Animation> t{new Animation(p,name.substr(0,name.size()-4),false)};
      ret=t.get();
      animCache[name] = std::move(t);
      } else {
      FileExt::exchangeExt(name,"MDH","MDS");
      ZenLoad::ZenParser zen(name,gothicAssets);
      ZenLoad::MdsParserTxt p(zen);

      std::unique_ptr<Animation> t{new Animation(p,name.substr(0,name.size()-4),true)};
      ret=t.get();
      animCache[name] = std::move(t);
      }
    if(!hasFile(name))
      throw std::runtime_error("load failed");
    return ret;
    }
  catch(...){
    Log::e("unable to load animation \"",name,"\"");
    return nullptr;
    }
  }

SoundEffect *Resources::implLoadSound(const char* name) {
  if(name==nullptr || *name=='\0')
    return nullptr;

  auto it=sndCache.find(name);
  if(it!=sndCache.end())
    return it->second.get();

  if(!getFileData(name,fBuff))
    return nullptr;

  try {
    Tempest::MemReader rd(fBuff.data(),fBuff.size());

    auto s = sound.load(rd);
    std::unique_ptr<SoundEffect> t{new SoundEffect(std::move(s))};
    SoundEffect* ret=t.get();
    sndCache[name] = std::move(t);
    return ret;
    }
  catch(...){
    Log::e("unable to load sound \"",name,"\"");
    return nullptr;
    }
  }

Dx8::Music Resources::implLoadDxMusic(const char* name) {
  auto u = Tempest::TextCodec::toUtf16(name);
  return dxMusic->load(u.c_str());
  }

Sound Resources::implLoadSoundBuffer(const char *name) {
  if(!getFileData(name,fBuff))
    return Sound();
  try {
    Tempest::MemReader rd(fBuff.data(),fBuff.size());
    return Sound(rd);
    }
  catch(...){
    Log::e("unable to load sound \"",name,"\"");
    return Sound();
    }
  }

GthFont &Resources::implLoadFont(const char* fname, FontType type) {
  auto it = gothicFnt.find(std::make_pair(fname,type));
  if(it!=gothicFnt.end())
    return *(*it).second;

  char file[256]={};
  for(size_t i=0;i<256 && fname[i];++i) {
    if(fname[i]=='.')
      break;
    file[i] = fname[i];
    }

  char tex[256]={};
  char fnt[256]={};
  switch(type) {
    case FontType::Normal:
    case FontType::Disabled:
    case FontType::Yellow:
    case FontType::Red:
      std::snprintf(tex,sizeof(tex),"%s.tga",file);
      std::snprintf(fnt,sizeof(fnt),"%s.fnt",file);
      break;
    case FontType::Hi:
      std::snprintf(tex,sizeof(tex),"%s_hi.tga",file);
      std::snprintf(fnt,sizeof(fnt),"%s_hi.fnt",file);
      break;
    }

  auto color = Tempest::Color(1.f);
  switch(type) {
    case FontType::Normal:
    case FontType::Hi:
      color = Tempest::Color(1.f);
      break;
    case FontType::Disabled:
      color = Tempest::Color(1.f,1.f,1.f,0.6f);
      break;
    case FontType::Yellow:
      color = Tempest::Color(1.f,1.f,0.1f,1.f);
      //color = Tempest::Color(0.81f,0.78f,0.01f,1.f);
      break;
    case FontType::Red:
      color = Tempest::Color(1.f,0.f,0.f,1.f);
      break;
    }

  auto ptr   = std::make_unique<GthFont>(fnt,tex,color,gothicAssets);
  GthFont* f = ptr.get();
  gothicFnt[std::make_pair(fname,type)] = std::move(ptr);
  return *f;
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

Texture2d Resources::loadTexture(const Pixmap &pm) {
  std::lock_guard<std::recursive_mutex> g(inst->sync);
  return inst->device.loadTexture(pm);
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

SoundEffect *Resources::loadSound(const char *name) {
  std::lock_guard<std::recursive_mutex> g(inst->sync);
  return inst->implLoadSound(name);
  }

SoundEffect *Resources::loadSound(const std::string &name) {
  std::lock_guard<std::recursive_mutex> g(inst->sync);
  return inst->implLoadSound(name.c_str());
  }

Sound Resources::loadSoundBuffer(const std::string &name) {
  std::lock_guard<std::recursive_mutex> g(inst->sync);
  return inst->implLoadSoundBuffer(name.c_str());
  }

Sound Resources::loadSoundBuffer(const char *name) {
  std::lock_guard<std::recursive_mutex> g(inst->sync);
  return inst->implLoadSoundBuffer(name);
  }

Dx8::Music Resources::loadDxMusic(const char* name) {
  std::lock_guard<std::recursive_mutex> g(inst->sync);
  return inst->implLoadDxMusic(name);
  }

bool Resources::getFileData(const char *name, std::vector<uint8_t> &dat) {
  dat.clear();
  return inst->gothicAssets.getFileData(name,dat);
  }

std::vector<uint8_t> Resources::getFileData(const char *name) {
  std::vector<uint8_t> data;
  inst->gothicAssets.getFileData(name,data);
  return data;
  }

std::vector<uint8_t> Resources::getFileData(const std::string &name) {
  std::vector<uint8_t> data;
  inst->gothicAssets.getFileData(name,data);
  return data;
  }

Resources::MeshLoadCode Resources::loadMesh(ZenLoad::PackedMesh& sPacked, ZenLoad::zCModelMeshLib& library, std::string name) {
  // Check if this isn't the compiled version
  if(name.rfind("-C")==std::string::npos) {
    // Strip the extension ".***"
    // Add "compiled"-extension
    FileExt::exchangeExt(name,"3DS","MRM") ||
    FileExt::exchangeExt(name,"MMS","MMB") ||
    FileExt::exchangeExt(name,"ASC","MDL");
    }

  if(FileExt::hasExt(name,"MRM")) {
    ZenLoad::zCProgMeshProto zmsh(name,gothicAssets);
    if(zmsh.getNumSubmeshes()==0)
      return MeshLoadCode::Error;
    zmsh.packMesh(sPacked,1.f);
    return MeshLoadCode::Static;
    }

  if(FileExt::hasExt(name,"MMB")) {
    ZenLoad::zCMorphMesh zmm(name,gothicAssets);
    if(zmm.getMesh().getNumSubmeshes()==0)
      return MeshLoadCode::Error;
    zmm.getMesh().packMesh(sPacked,1.f);
    for(auto& i:sPacked.vertices){
      // FIXME: hack with morph mesh-normals
      std::swap(i.Normal.y,i.Normal.z);
      i.Normal.z=-i.Normal.z;
      }
    return MeshLoadCode::Static;
    }

  if(FileExt::hasExt(name,"MDMS") ||
     FileExt::hasExt(name,"MDS")  ||
     FileExt::hasExt(name,"MDL")  ||
     FileExt::hasExt(name,"MDM")){
    library = loadMDS(name);
    return MeshLoadCode::Dynamic;
    }

  return MeshLoadCode::Error;
  }

ZenLoad::zCModelMeshLib Resources::loadMDS(std::string &name) {
  if(FileExt::exchangeExt(name,"MDMS","MDM"))
    return ZenLoad::zCModelMeshLib(name,gothicAssets,1.f);
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

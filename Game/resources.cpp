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
#include "graphics/submesh/pfxemittermesh.h"
#include "graphics/skeleton.h"
#include "graphics/protomesh.h"
#include "graphics/animation.h"
#include "graphics/attachbinder.h"
#include "graphics/material.h"
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
  fsq = Resources::vbo(fsqBuf.data(),fsqBuf.size());

  //sp = sphere(3,1.f);

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

  std::vector<Archive> archives;
  detectVdf(archives,gothic.nestedPath({u"Data"},Dir::FT_Dir));

  // addon archives first!
  std::stable_sort(archives.begin(),archives.end(),[](const Archive& a,const Archive& b){
    int aIsMod = a.isMod ? 1 : -1;
    int bIsMod = b.isMod ? 1 : -1;
    return std::make_tuple(aIsMod,a.time,int(a.ord)) >=
           std::make_tuple(bIsMod,b.time,int(b.ord));
    });

  for(auto& i:archives)
    gothicAssets.loadVDF(i.name);
  gothicAssets.finalizeLoad();

  //for(auto& i:gothicAssets.getKnownFiles())
  //  Log::i(i);

  // auto v = getFileData("TREASURE_ADDON_01.MDL");
  // Tempest::WFile f("../../internal/TREASURE_ADDON_01.MDL");
  // f.write(v.data(),v.size());
  }

Resources::~Resources() {
  inst=nullptr;
  }

const char* Resources::renderer() {
  return inst->device.renderer();
  }

static Sampler2d implShadowSampler() {
  Tempest::Sampler2d smp;
  smp.setClamping(Tempest::ClampMode::ClampToBorder);
  smp.anisotropic = false;
  return smp;
  }

const Sampler2d& Resources::shadowSampler() {
  static Tempest::Sampler2d smp = implShadowSampler();
  return smp;
  }

void Resources::detectVdf(std::vector<Archive>& ret, const std::u16string &root) {
  Dir::scan(root,[this,&root,&ret](const std::u16string& vdf,Dir::FileType t){
    if(t==Dir::FT_File) {
      auto file = root + vdf;
      Archive ar;
      ar.name  = root+vdf;
      ar.time  = vdfTimestamp(ar.name);
      ar.ord   = uint16_t(ret.size());
      ar.isMod = vdf.rfind(u".mod")==vdf.size()-4;

      //if(ar.time>0 || ar.isMod)
      //if(ar.isMod)
        ret.emplace_back(std::move(ar));
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

int64_t Resources::vdfTimestamp(const std::u16string& name) {
  enum {
    VDF_COMMENT_LENGTH   = 256,
    VDF_SIGNATURE_LENGTH = 16,
    };
  uint32_t count=0, timestamp=0;
  char sign[VDF_SIGNATURE_LENGTH]={};

  try {
    RFile fin(name);
    fin.seek(VDF_COMMENT_LENGTH);
    fin.read(sign,VDF_SIGNATURE_LENGTH);
    fin.read(&count,sizeof(count));
    fin.seek(4);
    fin.read(&timestamp,sizeof(timestamp));

    return int64_t(timestamp);
    }
  catch(...) {
    return -1;
    }
  }

Tempest::Texture2d* Resources::implLoadTexture(TextureCache& cache,const char* cname) {
  std::string name = cname;
  if(name.size()==0)
    return nullptr;

  auto it=cache.find(name);
  if(it!=cache.end())
    return it->second.get();

  if(FileExt::hasExt(name,"TGA")){
    name.resize(name.size()+2);
    std::memcpy(&name[0]+name.size()-6,"-C.TEX",6);
    if(hasFile(name)) {
      if(!getFileData(name.c_str(),fBuff)) {
        Log::e("unable to load texture \"",name,"\"");
        return nullptr;
        }
      ddsBuf.clear();
      ZenLoad::convertZTEX2DDS(fBuff,ddsBuf);
      auto t = implLoadTexture(cache,cname,ddsBuf);
      if(t!=nullptr) {
        return t;
        }
      }
    }

  if(getFileData(cname,fBuff))
    return implLoadTexture(cache,cname,fBuff);

  cache[name]=nullptr;
  return nullptr;
  }

Texture2d *Resources::implLoadTexture(TextureCache& cache,std::string&& name,const std::vector<uint8_t> &data) {
  try {
    Tempest::MemReader rd(data.data(),data.size());
    Tempest::Pixmap    pm(rd);

    std::unique_ptr<Texture2d> t{new Texture2d(device.loadTexture(pm))};
    Texture2d* ret=t.get();
    cache[std::move(name)] = std::move(t);
    return ret;
    }
  catch(...){
    return nullptr;
    }
  }

ProtoMesh* Resources::implLoadMesh(const std::string &name) {
  if(name.size()==0)
    return nullptr;

  auto it=aniMeshCache.find(name);
  if(it!=aniMeshCache.end())
    return it->second.get();

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
    std::unique_ptr<ProtoMesh> t{code==MeshLoadCode::Static ? new ProtoMesh(std::move(sPacked),name) : new ProtoMesh(library,name)};
    ProtoMesh* ret=t.get();
    aniMeshCache[name] = std::move(t);
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

Animation* Resources::implLoadAnimation(std::string name) {
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

Dx8::PatternList Resources::implLoadDxMusic(const char* name) {
  auto u = Tempest::TextCodec::toUtf16(name);
  return dxMusic->load(u.c_str());
  }

Sound Resources::implLoadSoundBuffer(const char *name) {
  if(name[0]=='\0')
    return Sound();

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

  char tex[300]={};
  char fnt[300]={};
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

PfxEmitterMesh* Resources::implLoadEmiterMesh(const char* name) {
  auto it = emiMeshCache.find(name);
  if(it!=emiMeshCache.end())
    return it->second.get();

  ZenLoad::PackedMesh        packed;
  ZenLoad::zCModelMeshLib    library;
  auto                       code=loadMesh(packed,library,name);
  (void)code;
  std::unique_ptr<PfxEmitterMesh> ptr{new PfxEmitterMesh(packed)};
  // std::unique_ptr<PfxEmitterMesh> ptr{code==MeshLoadCode::Static ? new PfxEmitterMesh(std::move(sPacked)) :
  //                                                                  new PfxEmitterMesh(library)};
  auto ret = ptr.get();
  emiMeshCache[name] = std::move(ptr);
  return ret;
  }

bool Resources::hasFile(const std::string &fname) {
  std::lock_guard<std::recursive_mutex> g(inst->sync);
  return inst->gothicAssets.hasFile(fname);
  }

const Texture2d *Resources::loadTexture(const char *name) {
  std::lock_guard<std::recursive_mutex> g(inst->sync);
  return inst->implLoadTexture(inst->texCache,name);
  }

const Tempest::Texture2d* Resources::loadTexture(const std::string &name) {
  std::lock_guard<std::recursive_mutex> g(inst->sync);
  return inst->implLoadTexture(inst->texCache,name.c_str());
  }

const Texture2d *Resources::loadTexture(const std::string &name, int32_t iv, int32_t ic) {
  if(name.size()>=128)
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

std::vector<const Texture2d*> Resources::loadTextureAnim(const std::string& name) {
  std::vector<const Texture2d*> ret;
  if(name.find("_A0")==std::string::npos)
    return ret;

  for(int id=0; ; ++id) {
    size_t at = 0;
    char   buf[128]={};
    for(size_t i=0;i<name.size();++i) {
      if(i+2<name.size() && name[i]=='_' && name[i+1]=='A' && name[i+2]=='0'){
        at += std::snprintf(buf+at,sizeof(buf)-at,"_A%d",id);
        i+=2;
        } else {
        buf[at] = name[i];
        at++;
        }
      if(at>sizeof(buf))
        return ret;
      }
    auto t = loadTexture(buf);
    if(t==nullptr)
      return ret;
    ret.push_back(t);
    }
  }

Texture2d Resources::loadTexture(const Pixmap &pm) {
  std::lock_guard<std::recursive_mutex> g(inst->sync);
  return inst->device.loadTexture(pm);
  }

Material Resources::loadMaterial(const ZenLoad::zCMaterialData& src, bool enableAlphaTest) {
  return Material(src,enableAlphaTest);
  }

const ProtoMesh *Resources::loadMesh(const std::string &name) {
  std::lock_guard<std::recursive_mutex> g(inst->sync);
  return inst->implLoadMesh(name);
  }

const PfxEmitterMesh* Resources::loadEmiterMesh(const char* name) {
  if(name==nullptr || name[0]=='\0')
    return nullptr;
  std::lock_guard<std::recursive_mutex> g(inst->sync);
  return inst->implLoadEmiterMesh(name);
  }

const Skeleton *Resources::loadSkeleton(const char* name) {
  std::lock_guard<std::recursive_mutex> g(inst->sync);
  return inst->implLoadSkeleton(name);
  }

const Animation *Resources::loadAnimation(const std::string &name) {
  std::lock_guard<std::recursive_mutex> g(inst->sync);
  return inst->implLoadAnimation(name);
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

Dx8::PatternList Resources::loadDxMusic(const char* name) {
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

Resources::MeshLoadCode Resources::loadMesh(ZenLoad::PackedMesh& sPacked, ZenLoad::zCModelMeshLib& library,
                                            std::string name) {
  if(name=="TREASURE_ADDON_01.ASC") {
    // name = "TREASURE.MRM";
    Log::d("");
    }
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

const AttachBinder *Resources::bindMesh(const ProtoMesh &anim, const Skeleton &s) {
  std::lock_guard<std::recursive_mutex> g(inst->sync);

  if(anim.submeshId.size()==0){
    static AttachBinder empty;
    return &empty;
    }
  BindK k = BindK(&s,&anim);

  auto it = inst->bindCache.find(k);
  if(it!=inst->bindCache.end())
    return it->second.get();

  std::unique_ptr<AttachBinder> ret(new AttachBinder(s,anim));
  auto p = ret.get();
  inst->bindCache[k] = std::move(ret);
  return p;
  }

Tempest::VertexBuffer<Resources::Vertex> Resources::sphere(int passCount, float R){
  std::vector<Resources::Vertex> r;
  r.reserve( size_t(4*pow(3, passCount+1)) );

  static const double pi = 3.141592654;

  Resources::Vertex v1 = {
    {1, 0, -0.5},
    {0,0,0},{0,0},0
    };
  Resources::Vertex v2 = {
    {float(cos(2*pi/3)), float(sin(2*pi/3)), -0.5},
    {0,0,0},{0,0},0
    };
  Resources::Vertex v3 = {
    {float(cos(2*pi/3)), -float(sin(2*pi/3)), -0.5},
    {0,0,0},{0,0},0
    };

  Resources::Vertex v4 = {
    {0, 0, 0.5},
    {0,0,0},{0,0},0
    };

  r.push_back(v1);
  r.push_back(v3);
  r.push_back(v2);

  r.push_back(v1);
  r.push_back(v2);
  r.push_back(v4);

  r.push_back(v2);
  r.push_back(v3);
  r.push_back(v4);

  r.push_back(v1);
  r.push_back(v4);
  r.push_back(v3);

  for(size_t i=0; i<r.size(); ++i){
    Resources::Vertex& v = r[i];
    float l = std::sqrt(v.pos[0]*v.pos[0] + v.pos[1]*v.pos[1] + v.pos[2]*v.pos[2]);

    v.pos[0] /= l;
    v.pos[1] /= l;
    v.pos[2] /= l;
    }

  for(int c=0; c<passCount; ++c){
    size_t maxI = r.size();
    for( size_t i=0; i<maxI; i+=3 ){
      Resources::Vertex x = {
        {
          0.5f*(r[i].pos[0]+r[i+1].pos[0]),
          0.5f*(r[i].pos[1]+r[i+1].pos[1]),
          0.5f*(r[i].pos[2]+r[i+1].pos[2])
        },
        {0,0,0},{0,0},0
      };
      Resources::Vertex y = {
        {
          0.5f*(r[i+2].pos[0]+r[i+1].pos[0]),
          0.5f*(r[i+2].pos[1]+r[i+1].pos[1]),
          0.5f*(r[i+2].pos[2]+r[i+1].pos[2])
        },
        {0,0,0},{0,0},0
      };
      Resources::Vertex z = {
        {
          0.5f*(r[i].pos[0]+r[i+2].pos[0]),
          0.5f*(r[i].pos[1]+r[i+2].pos[1]),
          0.5f*(r[i].pos[2]+r[i+2].pos[2])
        },
        {0,0,0},{0,0},0
      };

      r.push_back( r[i] );
      r.push_back( x );
      r.push_back( z );

      r.push_back( x );
      r.push_back( r[i+1] );
      r.push_back( y );

      r.push_back( y );
      r.push_back( r[i+2] );
      r.push_back( z );

      r[i]   = x;
      r[i+1] = y;
      r[i+2] = z;
      }

    for(size_t i=0; i<r.size(); ++i){
      Resources::Vertex & v = r[i];
      float l = std::sqrt(v.pos[0]*v.pos[0] + v.pos[1]*v.pos[1] + v.pos[2]*v.pos[2]);

      v.pos[0] /= l;
      v.pos[1] /= l;
      v.pos[2] /= l;
      }
    }

  for(size_t i=0; i<r.size(); ++i){
    Resources::Vertex & v = r[i];
    v.norm[0] = v.pos[0];
    v.norm[1] = v.pos[1];
    v.norm[2] = v.pos[2];

    v.pos[0] *= R;
    v.pos[1] *= R;
    v.pos[2] *= R;
    }

  return device.vbo(r);
  }

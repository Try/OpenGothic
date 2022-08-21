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

#include "graphics/mesh/submesh/staticmesh.h"
#include "graphics/mesh/submesh/animmesh.h"
#include "graphics/mesh/submesh/pfxemittermesh.h"
#include "graphics/mesh/submesh/packedmesh.h"
#include "graphics/mesh/skeleton.h"
#include "graphics/mesh/protomesh.h"
#include "graphics/mesh/animation.h"
#include "graphics/mesh/attachbinder.h"
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

Resources::Resources(Tempest::Device &device)
  : dev(device) {
  inst=this;

  ZenLib::Log::SetLogCallback([](ZenLib::Log::EMessageType t, const char* what) {
    switch(t) {
      case ZenLib::Log::EMessageType::MT_Error:
        Log::e(what);
        break;
      case ZenLib::Log::EMessageType::MT_Warning:
        Log::d(what);
        break;
      case ZenLib::Log::EMessageType::MT_Info:
        Log::i(what);
        break;
      }
    });

  static std::array<VertexFsq,6> fsqBuf =
   {{
      {-1,-1},{ 1,1},{1,-1},
      {-1,-1},{-1,1},{1, 1}
   }};
  fsq = Resources::vbo(fsqBuf.data(),fsqBuf.size());

  //sp = sphere(3,1.f);

  dxMusic.reset(new Dx8::DirectMusic());
  // G2
  dxMusic->addPath(Gothic::inst().nestedPath({u"_work",u"Data",u"Music",u"newworld"},  Dir::FT_Dir));
  dxMusic->addPath(Gothic::inst().nestedPath({u"_work",u"Data",u"Music",u"AddonWorld"},Dir::FT_Dir));
  // G1
  dxMusic->addPath(Gothic::inst().nestedPath({u"_work",u"Data",u"Music",u"dungeon"},  Dir::FT_Dir));
  dxMusic->addPath(Gothic::inst().nestedPath({u"_work",u"Data",u"Music",u"menu_men"}, Dir::FT_Dir));
  dxMusic->addPath(Gothic::inst().nestedPath({u"_work",u"Data",u"Music",u"orchestra"},Dir::FT_Dir));

  fBuff .reserve(8*1024*1024);
  ddsBuf.reserve(8*1024*1024);

  {
  Pixmap pm(1,1,Pixmap::Format::RGBA);
  uint8_t* pix = reinterpret_cast<uint8_t*>(pm.data());
  pix[0]=255;
  pix[3]=255;
  fallback = device.texture(pm);
  }

  {
  Pixmap pm(1,1,Pixmap::Format::RGBA);
  fbZero = device.texture(pm);
  }
  }

void Resources::loadVdfs(const std::vector<std::u16string>& modvdfs) {
  std::vector<Archive> archives;
  inst->detectVdf(archives,Gothic::inst().nestedPath({u"Data"},Dir::FT_Dir));

  // Remove all mod files, that are not listed in modvdfs
  archives.erase(std::remove_if(archives.begin(), archives.end(),
                [&modvdfs](const Archive& a){
                  return a.isMod && modvdfs.end() == std::find_if(modvdfs.begin(), modvdfs.end(),
                        [&a](const std::u16string& modname) {
                          const std::u16string_view& full_path = a.name;
                          const std::u16string_view& file_name = modname;
                          return (0 == full_path.compare(full_path.length() - file_name.length(),
                                                         file_name.length(), file_name));
                          });
                  }), archives.end());

  // addon archives first!
  std::stable_sort(archives.begin(),archives.end(),[](const Archive& a,const Archive& b){
    int aIsMod = a.isMod ? 1 : -1;
    int bIsMod = b.isMod ? 1 : -1;
    return std::make_tuple(aIsMod,a.time,int(a.ord)) >=
           std::make_tuple(bIsMod,b.time,int(b.ord));
    });

  for(auto& i:archives)
    inst->gothicAssets.loadVDF(i.name);
  inst->gothicAssets.finalizeLoad();

  //for(auto& i:gothicAssets.getKnownFiles())
  //  Log::i(i);

  // auto v = getFileData("DRAGONISLAND.ZEN");
  // Tempest::WFile f("../../internal/DRAGONISLAND.ZEN");
  // f.write(v.data(),v.size());
  }

Resources::~Resources() {
  inst=nullptr;
  }

bool Resources::hasFile(std::string_view name) {
  std::lock_guard<std::recursive_mutex> g(inst->sync);
  if(name.size()<128) {
    char buf[128] = {};
    std::snprintf(buf,sizeof(buf),"%.*s",int(name.size()),name.data());
    return inst->gothicAssets.hasFile(buf);
    }
  return inst->gothicAssets.hasFile(std::string(name));
  }

bool Resources::getFileData(std::string_view name, std::vector<uint8_t> &dat) {
  dat.clear();
  if(name.size()<128) {
    char buf[128] = {};
    std::snprintf(buf,sizeof(buf),"%.*s",int(name.size()),name.data());
    return inst->gothicAssets.getFileData(buf,dat);
    }
  return inst->gothicAssets.getFileData(std::string(name),dat);
  }

std::vector<uint8_t> Resources::getFileData(std::string_view name) {
  std::vector<uint8_t> data;
  getFileData(name,data);
  return data;
  }

const char* Resources::renderer() {
  return inst->dev.properties().name;
  }

static Sampler2d implShadowSampler() {
  Tempest::Sampler2d smp;
  smp.setClamping(Tempest::ClampMode::ClampToEdge);
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

const GthFont &Resources::font(std::string_view fname, FontType type) {
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

Tempest::Texture2d* Resources::implLoadTexture(TextureCache& cache, std::string_view cname) {
  if(cname.empty())
    return nullptr;

  std::string name = std::string(cname);
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
      auto t = implLoadTexture(cache,std::string(cname),ddsBuf);
      if(t!=nullptr) {
        return t;
        }
      }
    }

  if(getFileData(cname,fBuff))
    return implLoadTexture(cache,std::string(cname),fBuff);

  cache[name]=nullptr;
  return nullptr;
  }

Texture2d *Resources::implLoadTexture(TextureCache& cache, std::string&& name, const std::vector<uint8_t> &data) {
  try {
    Tempest::MemReader rd(data.data(),data.size());
    Tempest::Pixmap    pm(rd);

    std::unique_ptr<Texture2d> t{new Texture2d(dev.texture(pm))};
    Texture2d* ret=t.get();
    cache[std::move(name)] = std::move(t);
    return ret;
    }
  catch(...){
    return nullptr;
    }
  }

ProtoMesh* Resources::implLoadMesh(std::string_view name) {
  if(name.size()==0)
    return nullptr;

  auto cname = std::string(name);
  auto it    = aniMeshCache.find(cname);
  if(it!=aniMeshCache.end())
    return it->second.get();

  auto  t   = implLoadMeshMain(cname);
  auto  ret = t.get();
  aniMeshCache[cname] = std::move(t);
  if(ret==nullptr)
    Log::e("unable to load mesh \"",cname,"\"");
  return ret;
  }

std::unique_ptr<ProtoMesh> Resources::implLoadMeshMain(std::string name) {
  if(FileExt::hasExt(name,"3DS")) {
    FileExt::exchangeExt(name,"3DS","MRM");
    ZenLoad::zCProgMeshProto zmsh(name,gothicAssets);
    if(zmsh.getNumSubmeshes()==0)
      return nullptr;

    PackedMesh packed(zmsh,PackedMesh::PK_Visual);
    return std::unique_ptr<ProtoMesh>{new ProtoMesh(std::move(packed),name)};
    }

  if(FileExt::hasExt(name,"MMS") || FileExt::hasExt(name,"MMB")) {
    FileExt::exchangeExt(name,"MMS","MMB");
    ZenLoad::zCMorphMesh zmm(name,gothicAssets);
    if(zmm.getMesh().getNumSubmeshes()==0)
      return nullptr;

    PackedMesh packed(zmm.getMesh(),PackedMesh::PK_VisualMorph);
    return std::unique_ptr<ProtoMesh>{new ProtoMesh(std::move(packed),zmm.aniList,name)};
    }

  if(FileExt::hasExt(name,"MDS")) {
    auto anim = Resources::loadAnimation(name);
    if(anim==nullptr)
      return nullptr;

    ZenLoad::zCModelMeshLib mdm, mdh;

    auto mesh   = anim->defaultMesh();
    bool hasMdm = false;

    FileExt::exchangeExt(mesh,nullptr,"MDM") ||
    FileExt::exchangeExt(mesh,"ASC",  "MDM");

    if(hasFile(mesh)) {
      ZenLoad::ZenParser parserMdm(mesh,gothicAssets);
      mdm.loadMDM(parserMdm);
      hasMdm = true;
      }

    if(anim->defaultMesh().empty())
      mesh = name;
    FileExt::assignExt(mesh,"MDH");

    ZenLoad::ZenParser parserMdh(mesh,gothicAssets);
    mdh.loadMDH(parserMdh);

    std::unique_ptr<Skeleton> sk{new Skeleton(mdh,anim,name)};
    std::unique_ptr<ProtoMesh> t;
    if(hasMdm)
      t.reset(new ProtoMesh(std::move(mdm),std::move(sk),name)); else
      t.reset(new ProtoMesh(std::move(mdh),std::move(sk),name));
    return t;
    }

  if(FileExt::hasExt(name,"MDM") || FileExt::hasExt(name,"MDL") || FileExt::hasExt(name,"ASC")) {
    FileExt::exchangeExt(name,"ASC","MDM");

    if(!hasFile(name))
      return nullptr;

    ZenLoad::zCModelMeshLib mdm;

    ZenLoad::ZenParser parser(name,gothicAssets);
    if(FileExt::hasExt(name,"MDL"))
      mdm.loadMDL(parser); else
      mdm.loadMDM(parser);

    std::unique_ptr<Skeleton> sk{new Skeleton(mdm,nullptr,name)};
    std::unique_ptr<ProtoMesh> t{new ProtoMesh(std::move(mdm),std::move(sk),name)};
    return t;
    }

  if(FileExt::hasExt(name,"TGA")) {
    Log::e("decals should be loaded by Resources::implDecalMesh instead");
    }

  return nullptr;
  }

PfxEmitterMesh* Resources::implLoadEmiterMesh(std::string_view name) {
  // TODO: reuse code from Resources::implLoadMeshMain
  auto cname = std::string(name);
  auto it    = emiMeshCache.find(cname);
  if(it!=emiMeshCache.end())
    return it->second.get();

  auto& ret = emiMeshCache[cname];

  if(FileExt::hasExt(cname,"3DS")) {
    FileExt::exchangeExt(cname,"3DS","MRM");
    ZenLoad::zCProgMeshProto zmsh(cname,gothicAssets);
    if(zmsh.getNumSubmeshes()==0)
      return nullptr;

    PackedMesh packed(zmsh,PackedMesh::PK_Visual);
    ret = std::unique_ptr<PfxEmitterMesh>(new PfxEmitterMesh(packed));
    return ret.get();
    }

  if(FileExt::hasExt(name,"MDM")) {
    if(!hasFile(name))
      return nullptr;

    ZenLoad::ZenParser parser(cname,gothicAssets);
    ZenLoad::zCModelMeshLib mdm;
    mdm.loadMDM(parser);

    ret = std::unique_ptr<PfxEmitterMesh>(new PfxEmitterMesh(std::move(mdm)));
    return ret.get();
    }

  return nullptr;
  }

ProtoMesh* Resources::implDecalMesh(const ZenLoad::zCVobData& vob) {
  DecalK key;
  key.mat         = Material(vob);
  key.sX          = vob.visualChunk.zCDecal.decalDim.x;
  key.sY          = vob.visualChunk.zCDecal.decalDim.y;
  key.decal2Sided = vob.visualChunk.zCDecal.decal2Sided;

  if(key.mat.tex==nullptr)
    return nullptr;

  auto it = decalMeshCache.find(key);
  if(it!=decalMeshCache.end())
    return it->second.get();

  Resources::Vertex vbo[PackedMesh::MaxVert] = {
    {{-1.f, -1.f, 0.f},{0,0,-1},{0,1}, 0xFFFFFFFF},
    {{ 1.f, -1.f, 0.f},{0,0,-1},{1,1}, 0xFFFFFFFF},
    {{ 1.f,  1.f, 0.f},{0,0,-1},{1,0}, 0xFFFFFFFF},
    {{-1.f,  1.f, 0.f},{0,0,-1},{0,0}, 0xFFFFFFFF},

    {{-1.f, -1.f, 0.f},{0,0, 1},{0,1}, 0xFFFFFFFF},
    {{ 1.f, -1.f, 0.f},{0,0, 1},{1,1}, 0xFFFFFFFF},
    {{ 1.f,  1.f, 0.f},{0,0, 1},{1,0}, 0xFFFFFFFF},
    {{-1.f,  1.f, 0.f},{0,0, 1},{0,0}, 0xFFFFFFFF},
    };
  for(auto& i:vbo) {
    i.pos[0]*=key.sX;
    i.pos[1]*=key.sY;
    }

  std::vector<Resources::Vertex> cvbo(vbo,vbo+PackedMesh::MaxVert);
  std::vector<uint32_t>          cibo;
  if(key.decal2Sided)
    cibo = { 0,1,2, 0,2,3, 4,6,5, 4,7,6 }; else
    cibo = { 0,1,2, 0,2,3 };

  size_t iboSz = cibo.size();
  cibo.resize(PackedMesh::MaxInd);
  for(size_t i=iboSz; i<cibo.size(); ++i)
    cibo[i] = 0;

  std::unique_ptr<ProtoMesh> t{new ProtoMesh(key.mat, std::move(cvbo), std::move(cibo))};

  auto ret = t.get();
  decalMeshCache[key] = std::move(t);
  return ret;
  }

std::unique_ptr<Animation> Resources::implLoadAnimation(std::string name) {
  if(name.size()<4)
    return nullptr;

  if(Gothic::inst().version().game==2)
    FileExt::exchangeExt(name,"MDS","MSB");

  if(FileExt::hasExt(name,"MSB")) {
    ZenLoad::ZenParser    zen(name,gothicAssets);
    ZenLoad::MdsParserBin p(zen);
    return std::unique_ptr<Animation>{new Animation(p,name.substr(0,name.size()-4),false)};
    }

  if(FileExt::hasExt(name,"MDS")) {
    ZenLoad::ZenParser    zen(name,gothicAssets);
    ZenLoad::MdsParserTxt p(zen);
    return std::unique_ptr<Animation>{new Animation(p,name.substr(0,name.size()-4),true)};
    }
  return nullptr;
  }

Dx8::PatternList Resources::implLoadDxMusic(std::string_view name) {
  auto u = Tempest::TextCodec::toUtf16(std::string(name));
  return dxMusic->load(u.c_str());
  }

Tempest::Sound Resources::implLoadSoundBuffer(std::string_view name) {
  if(name.empty())
    return Tempest::Sound();

  if(!getFileData(name,fBuff))
    return Tempest::Sound();
  try {
    Tempest::MemReader rd(fBuff.data(),fBuff.size());
    return Tempest::Sound(rd);
    }
  catch(...) {
    auto cname = std::string (name);
    Log::e("unable to load sound \"",cname,"\"");
    return Tempest::Sound();
    }
  }

GthFont &Resources::implLoadFont(std::string_view name, FontType type) {
  auto cname = std::string(name);
  auto it    = gothicFnt.find(std::make_pair(cname,type));
  if(it!=gothicFnt.end())
    return *(*it).second;

  char file[256]={};
  for(size_t i=0; i<256 && cname[i];++i) {
    if(cname[i]=='.')
      break;
    file[i] = cname[i];
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
  gothicFnt[std::make_pair(std::move(cname),type)] = std::move(ptr);
  return *f;
  }

const Texture2d *Resources::loadTexture(std::string_view name) {
  std::lock_guard<std::recursive_mutex> g(inst->sync);
  return inst->implLoadTexture(inst->texCache,name);
  }

const Texture2d *Resources::loadTexture(std::string_view name, int32_t iv, int32_t ic) {
  if(name.size()>=128)
    return loadTexture(name);

  char v[16]={};
  char c[16]={};
  char buf1[128]={};
  char buf2[128]={};

  std::snprintf(v,sizeof(v),"V%d",iv);
  std::snprintf(c,sizeof(c),"C%d",ic);
  std::snprintf(buf1,sizeof(buf1),"%.*s",int(name.size()),name.data());

  emplaceTag(buf1,'V');
  std::snprintf(buf2,sizeof(buf2),buf1,v);

  emplaceTag(buf2,'C');
  std::snprintf(buf1,sizeof(buf1),buf2,c);

  return loadTexture(buf1);
  }

std::vector<const Texture2d*> Resources::loadTextureAnim(std::string_view name) {
  std::vector<const Texture2d*> ret;
  if(name.find("_A0")==std::string::npos &&
     name.find("_a0")==std::string::npos)
    return ret;

  for(int id=0; ; ++id) {
    size_t at = 0;
    char   buf[128]={};
    for(size_t i=0;i<name.size();++i) {
      if(i+2<name.size() && name[i]=='_' && (name[i+1]=='A' || name[i+1]=='a') && name[i+2]=='0'){
        at += size_t(std::snprintf(buf+at,sizeof(buf)-at,"_A%d",id));
        i+=2;
        } else {
        buf[at] = name[i];
        if('a'<=buf[at] && buf[at]<='z')
          buf[at] = char(buf[at]+'A'-'a');
        at++;
        }
      if(at>sizeof(buf))
        return ret;
      }
   auto t = loadTexture(std::string_view(buf));
    if(t==nullptr) {
      char buf2[128] = {};
      std::snprintf(buf2,sizeof(buf2),"%s.TGA",buf);
      t = loadTexture(std::string_view(buf2));
      if(t==nullptr)
        return ret;
      }
    ret.push_back(t);
    }
  }

Texture2d Resources::loadTexturePm(const Pixmap &pm) {
  if(pm.isEmpty()) {
    Pixmap p2(1,1,Pixmap::Format::R);
    std::memset(p2.data(),0,1);
    return inst->dev.texture(p2);
    }
  return inst->dev.texture(pm);
  }

Material Resources::loadMaterial(const ZenLoad::zCMaterialData& src, bool enableAlphaTest) {
  return Material(src,enableAlphaTest);
  }

const ProtoMesh* Resources::loadMesh(std::string_view name) {
  if(name.size()==0)
    return nullptr;
  std::lock_guard<std::recursive_mutex> g(inst->sync);
  return inst->implLoadMesh(name);
  }

const PfxEmitterMesh* Resources::loadEmiterMesh(std::string_view name) {
  if(name.empty())
    return nullptr;
  std::lock_guard<std::recursive_mutex> g(inst->sync);
  return inst->implLoadEmiterMesh(name);
  }

const Skeleton* Resources::loadSkeleton(std::string_view name) {
  auto s = Resources::loadMesh(name);
  if(s==nullptr)
    return nullptr;
  return s->skeleton.get();
  }

const Animation* Resources::loadAnimation(std::string_view name) {
  auto cname = std::string(name);

  std::lock_guard<std::recursive_mutex> g(inst->sync);
  auto& cache = inst->animCache;
  auto it=cache.find(cname);
  if(it!=cache.end())
    return it->second.get();
  auto t       = inst->implLoadAnimation(cname);
  auto ret     = t.get();
  cache[cname] = std::move(t);
  return ret;
  }

Tempest::Sound Resources::loadSoundBuffer(std::string_view name) {
  std::lock_guard<std::recursive_mutex> g(inst->sync);
  return inst->implLoadSoundBuffer(name);
  }

Dx8::PatternList Resources::loadDxMusic(std::string_view name) {
  std::lock_guard<std::recursive_mutex> g(inst->sync);
  return inst->implLoadDxMusic(name);
  }

const ProtoMesh* Resources::decalMesh(const ZenLoad::zCVobData& vob) {
  std::lock_guard<std::recursive_mutex> g(inst->sync);
  return inst->implDecalMesh(vob);
  }

ZenLoad::oCWorldData Resources::loadVobBundle(std::string_view name) {
  std::lock_guard<std::recursive_mutex> g(inst->sync);
  return inst->implLoadVobBundle(name);
  }

ZenLoad::oCWorldData& Resources::implLoadVobBundle(std::string_view filename) {
  auto cname = std::string(filename);
  auto i     = zenCache.find(cname);
  if(i!=zenCache.end())
    return i->second;

  ZenLoad::oCWorldData bundle;
  try {
    ZenLoad::ZenParser parser(cname,Resources::vdfsIndex());
    parser.readHeader();

    auto fver = ZenLoad::ZenParser::FileVersion::Gothic1;
    if(Gothic::inst().version().game==2)
      fver = ZenLoad::ZenParser::FileVersion::Gothic2;
    parser.readWorld(bundle,fver);
    }
  catch(...) {
    Log::e("unable to load Zen-file: \"",cname,"\"");
    }

  auto ret = zenCache.insert(std::make_pair(filename,std::move(bundle)));
  return ret.first->second;
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

  return dev.vbo(r);
  }

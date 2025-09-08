#pragma once

#include <Tempest/Font>
#include <Tempest/Texture2d>
#include <Tempest/Device>
#include <Tempest/SoundDevice>

#include <zenkit/Vfs.hh>
#include <zenkit/world/VobTree.hh>

#include <tuple>
#include <string_view>
#include <map>

#include "graphics/material.h"
#include "sound/soundfx.h"

struct DmSegment;
struct DmLoader;

class StaticMesh;
class ProtoMesh;
class Skeleton;
class Animation;
class AttachBinder;
class PhysicMeshShape;
class PfxEmitterMesh;
class GthFont;

namespace Dx8 {
class DirectMusic;
class PatternList;
}

class Resources final {
  public:
    explicit Resources(Tempest::Device& device);
    ~Resources();

    enum class FontType : uint8_t {
      Normal,
      Hi,
      Disabled,
      Yellow,
      Red
      };

    enum {
      MaxFramesInFlight = 2,
      ShadowLayers      = 2,
      };

    static const size_t MAX_NUM_SKELETAL_NODES = 96;
    static const size_t MAX_MORPH_LAYERS       = 4;

    struct Vertex {
      float    pos[3];
      float    norm[3];
      float    uv[2];
      uint32_t color;
      };

    struct VertexA {
      float    norm[3];
      float    uv[2];
      uint32_t color/*unused*/;
      float    pos[4][3];
      uint8_t  boneId[4];
      float    weights[4];
      };

    struct VertexFsq {
      float    pos[2];
      };

    struct VertexL {
      Tempest::Vec3 pos;
      Tempest::Vec4 cen;
      Tempest::Vec3 color;
      };

    using VobTree = std::vector<std::shared_ptr<zenkit::VirtualObject>>;

    static Tempest::Device&          device() { return inst->dev; }
    static const char*               renderer();
    static void                      mountWork(const std::filesystem::path& path);
    static void                      loadVdfs(const std::vector<std::u16string> &modvdfs, bool modFilter);

    static const Tempest::Sampler&   shadowSampler();

    static const GthFont&            dialogFont(const float scale);
    static const GthFont&            font(const float scale);
    static const GthFont&            font(FontType type, const float scale);
    static const GthFont&            font(std::string_view fname, FontType type, const float scale);

    static const Tempest::Texture2d& fallbackTexture();
    static const Tempest::Texture2d& fallbackBlack();
    static auto                      fallbackImage() -> const Tempest::StorageImage&;
    static Tempest::Texture2d        loadTextureUncached(std::string_view name, bool forceMips = false);
    static const Tempest::Texture2d* loadTexture(std::string_view name, bool forceMips = false);
    static const Tempest::Texture2d* loadTexture(Tempest::Color color);
    static const Tempest::Texture2d* loadTexture(std::string_view name, int32_t v, int32_t c);
    static       Tempest::Texture2d  loadTexturePm(const Tempest::Pixmap& pm);
    static auto                      loadTextureAnim(std::string_view name) -> std::vector<const Tempest::Texture2d*>;
    static       Material            loadMaterial(const zenkit::Material& src, bool enableAlphaTest);

    static const AttachBinder*       bindMesh       (const ProtoMesh& anim, const Skeleton& s);
    static const ProtoMesh*          loadMesh       (std::string_view name);
    static const PfxEmitterMesh*     loadEmiterMesh (std::string_view name);
    static const Skeleton*           loadSkeleton   (std::string_view name);
    static const Animation*          loadAnimation  (std::string_view name);
    static Tempest::Sound            loadSoundBuffer(std::string_view name);

    static Dx8::PatternList          loadDxMusic(std::string_view name);
    static DmSegment*                loadMusicSegment(char const* name);
    static const ProtoMesh*          decalMesh(const zenkit::VisualDecal& decal);

    static const VobTree*            loadVobBundle(std::string_view name);

    template<class V>
    static Tempest::VertexBuffer<V>  vbo(const V* data,size_t sz){ return inst->dev.vbo(data,sz); }

    template<class I>
    static Tempest::IndexBuffer<I>   ibo(const I* data,size_t sz){ return inst->dev.ibo(data,sz); }

    static Tempest::StorageBuffer    ssbo(const void* data, size_t size)         { return inst->dev.ssbo(data,size); }
    static Tempest::StorageBuffer    ssbo(Tempest::Uninitialized_t, size_t size) { return inst->dev.ssbo(Tempest::Uninitialized,size); }

    template<class V, class I>
    static Tempest::AccelerationStructure
                                     blas(const Tempest::VertexBuffer<V>& b,
                                          const Tempest::IndexBuffer<I>&  i,
                                          size_t offset, size_t size){
      if(!inst->dev.properties().raytracing.rayQuery)
        return Tempest::AccelerationStructure();
      return inst->dev.blas(b,i,offset,size);
      }

    static void resetRecycled(uint8_t fId);
    static void recycle(Tempest::DescriptorArray&& arr);
    static void recycle(Tempest::StorageBuffer&& ssbo);
    static void recycle(Tempest::StorageImage&& img);
    static void recycle(Tempest::ZBuffer&& img);
    static void recycle(Tempest::AccelerationStructure&& rtas);

    static std::vector<uint8_t>      getFileData(std::string_view name);
    static bool                      getFileData(std::string_view name, std::vector<uint8_t>& dat);
    static std::unique_ptr<zenkit::Read> getFileBuffer(std::string_view name);
    static auto                      openReader(std::string_view name, std::unique_ptr<zenkit::Read>& read) -> std::unique_ptr<zenkit::ReadArchive>;
    static bool                      hasFile    (std::string_view fname);

    static const zenkit::Vfs&        vdfsIndex();

    static const Tempest::IndexBuffer<uint16_t>&   cubeIbo();

  private:
    static Resources* inst;

    struct Archive {
      std::u16string name;
      int64_t        time=0;
      uint16_t       ord=0;
      bool           isMod=false;
      };

    struct DecalK {
      Material mat;
      float    sX = 1;
      float    sY = 1;
      bool     decal2Sided = false;
      bool     operator == (const DecalK& other) const {
        return mat        ==other.mat &&
               sX         ==other.sX &&
               sY         ==other.sY &&
               decal2Sided==other.decal2Sided;
        }
      };

    using TextureCache = std::unordered_map<std::string,std::unique_ptr<Tempest::Texture2d>>;

    int64_t               vdfTimestamp(const std::u16string& name);
    void                  detectVdf(std::vector<Archive>& ret, const std::u16string& root);

    Tempest::Texture2d*   implLoadTexture(std::string_view cname, bool forceMips);
    Tempest::Texture2d    implLoadTextureUncached(std::string_view name, bool forceMips);
    Tempest::Texture2d    implLoadTextureUncached(std::string_view name, zenkit::Read& data, bool forceMips);
    ProtoMesh*            implLoadMesh(std::string_view name);
    std::unique_ptr<ProtoMesh> implLoadMeshMain(std::string name);
    std::unique_ptr<Animation> implLoadAnimation(std::string name);
    ProtoMesh*            implDecalMesh(const zenkit::VisualDecal& decal);
    Tempest::Sound        implLoadSoundBuffer(std::string_view name);
    Dx8::PatternList      implLoadDxMusic(std::string_view name);
    DmSegment*            implLoadMusicSegment(char const* name);
    GthFont&              implLoadFont(std::string_view fname, FontType type, const float scale);
    PfxEmitterMesh*       implLoadEmiterMesh(std::string_view name);
    const VobTree*        implLoadVobBundle(std::string_view name);

    Tempest::VertexBuffer<Vertex> sphere(int passCount, float R);

    Tempest::Texture2d fallback, fbZero;
    Tempest::StorageImage fbImg;

    using BindK  = std::tuple<const Skeleton*,const ProtoMesh*>;
    using FontK  = std::tuple<const std::string,FontType,float>;

    struct Hash {
      size_t operator()(const BindK& b) const {
        return std::uintptr_t(std::get<0>(b));
        }
      size_t operator()(const DecalK& b) const {
        return std::uintptr_t(b.mat.tex);
        }
      size_t operator()(const FontK& b) const {
        std::hash<std::string> h;
        return h(std::get<0>(b));
        }
      };

    struct Less {
      size_t operator()(const Tempest::Color& a, const Tempest::Color& b) const {
        auto ta = std::make_tuple(a.r(),a.g(),a.b(),a.a());
        auto tb = std::make_tuple(b.r(),b.g(),b.b(),b.a());
        return ta<tb;
        }
      };

    Tempest::Device&                  dev;
    Tempest::SoundDevice              sound;

    std::recursive_mutex              sync;
    std::unique_ptr<Dx8::DirectMusic> dxMusic;
    DmLoader*                         dmLoader = nullptr;
    zenkit::Vfs                       gothicAssets;

    std::vector<uint8_t>              fBuff, ddsBuf;
    Tempest::IndexBuffer<uint16_t>    cube;

    struct DeleteQueue {
      std::vector<Tempest::StorageBuffer>   ssbo;
      std::vector<Tempest::StorageImage>    img;
      std::vector<Tempest::ZBuffer>         zb;
      std::vector<Tempest::DescriptorArray> arr;
      std::vector<Tempest::AccelerationStructure> rtas;
      };
    DeleteQueue recycled[MaxFramesInFlight];
    uint8_t     recycledId = 0;

    TextureCache                                                      texCache;
    std::map<Tempest::Color,std::unique_ptr<Tempest::Texture2d>,Less> pixCache;
    std::unordered_map<std::string,std::unique_ptr<ProtoMesh>>        aniMeshCache;
    std::unordered_map<DecalK,std::unique_ptr<ProtoMesh>,Hash>        decalMeshCache;
    std::unordered_map<std::string,std::unique_ptr<Skeleton>>         skeletonCache;
    std::unordered_map<std::string,std::unique_ptr<Animation>>        animCache;
    std::unordered_map<BindK,std::unique_ptr<AttachBinder>,Hash>      bindCache;
    std::unordered_map<std::string,std::unique_ptr<PfxEmitterMesh>>   emiMeshCache;
    std::unordered_map<std::string,std::unique_ptr<VobTree>>          zenCache;

    std::recursive_mutex                                              syncFont;
    std::unordered_map<FontK,std::unique_ptr<GthFont>,Hash>           gothicFnt;
  };

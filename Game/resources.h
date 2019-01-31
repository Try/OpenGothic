#pragma once

#include <Tempest/Assets>
#include <Tempest/Font>
#include <Tempest/Texture2d>
#include <Tempest/Device>

#include <vdfs/fileIndex.h>

#include <zenload/zCModelMeshLib.h>
#include <zenload/zTypes.h>

class Gothic;

class StaticMesh;
class ProtoMesh;
class Skeleton;
class Animation;
class AttachBinder;

class Resources {
  public:
    explicit Resources(Gothic& gothic,Tempest::Device& device);
    ~Resources();

    static const size_t MAX_NUM_SKELETAL_NODES = 96;

    struct Vertex {
      float    pos[3];
      float    norm[3];
      float    uv[2];
      uint32_t color;
      };

    struct VertexA {
      float    pos[3];
      float    norm[3];
      float    uv[2];
      float    boneId[4];
      float    weights[4];
      /*
      uint32_t color;
      float    LocalPositions[4][3];
      uint8_t  BoneIndices[4];
      float    weights[4];*/
      };

    static Tempest::Font menuFont() { return inst->menuFnt; }
    static Tempest::Font font()     { return inst->mainFnt; }

    static const Tempest::Texture2d* loadTexture(const char* name);
    static const Tempest::Texture2d* loadTexture(const std::string& name);

    static const AttachBinder*       bindMesh     (const ProtoMesh& anim,const Skeleton& s,const char* defBone);
    static const ProtoMesh*          loadMesh     (const std::string& name);
    static const Skeleton*           loadSkeleton (const std::string& name);
    static const Animation*          loadAnimation(const std::string& name);

    template<class V>
    static Tempest::VertexBuffer<V> loadVbo(const V* data,size_t sz){ return inst->device.loadVbo(data,sz,Tempest::BufferFlags::Static); }

    template<class V>
    static Tempest::IndexBuffer<V>  loadIbo(const V* data,size_t sz){ return inst->device.loadIbo(data,sz,Tempest::BufferFlags::Static); }

    static std::vector<uint8_t> getFileData(const char*        name);
    static std::vector<uint8_t> getFileData(const std::string& name);

    static VDFS::FileIndex& vdfsIndex();

  private:
    static Resources* inst;

    enum class MeshLoadCode : uint8_t {
      Error,
      Static,
      Dynamic
      };

    void                addVdf(const char* vdf);
    Tempest::Texture2d* implLoadTexture(const std::string &name);
    ProtoMesh*          implLoadMesh(const std::string &name);
    Skeleton*           implLoadSkeleton(std::string name);
    Animation*          implLoadAnimation(std::string name);

    MeshLoadCode        loadMesh(ZenLoad::PackedMesh &sPacked, ZenLoad::zCModelMeshLib &lib, std::string  name);
    ZenLoad::zCModelMeshLib loadMDS (std::string& name);
    bool                hasFile(const std::string& fname);

    Tempest::Texture2d fallback;

    using BindK = std::pair<const Skeleton*,const ProtoMesh*>;
    struct Hash {
      size_t operator()(const BindK& b) const {
        return std::uintptr_t(b.first);
        }
      };

    std::unordered_map<std::string,std::unique_ptr<Tempest::Texture2d>> texCache;
    std::unordered_map<std::string,std::unique_ptr<ProtoMesh>>          aniMeshCache;
    std::unordered_map<std::string,std::unique_ptr<Skeleton>>           skeletonCache;
    std::unordered_map<std::string,std::unique_ptr<Animation>>          animCache;
    std::unordered_map<BindK,std::unique_ptr<AttachBinder>,Hash>        bindCache;

    Tempest::Device& device;
    Tempest::Font    menuFnt, mainFnt;
    Tempest::Assets  asset;
    Gothic&          gothic;
    VDFS::FileIndex  gothicAssets;
  };


namespace Tempest {

template<>
inline std::initializer_list<Decl::ComponentType> vertexBufferDecl<Resources::Vertex>() {
  return {Decl::float3,Decl::float3,Decl::float2,Decl::color};
  }

template<>
inline std::initializer_list<Decl::ComponentType> vertexBufferDecl<Resources::VertexA>() {
  return {Decl::float3,Decl::float3,Decl::float2,
          Decl::float4,Decl::float4};
  }

}

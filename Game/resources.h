#pragma once

#include <Tempest/Assets>
#include <Tempest/Font>
#include <Tempest/Texture2d>
#include <Tempest/Device>

#include <vdfs/fileIndex.h>

#include <zenload/zTypes.h>

class Gothic;
class StaticMesh;

class Resources {
  public:
    explicit Resources(Gothic& gothic,Tempest::Device& device);
    ~Resources();

    struct Vertex {
      float    pos[3];
      float    norm[3];
      float    uv[2];
      uint32_t color;
      };

    struct VertexA {
      float    norm[3];
      float    uv[2];
      uint32_t color;
      float    LocalPositions[4][3];
      uint8_t  BoneIndices[4];
      float    weights[4];
      };

    static Tempest::Font menuFont(){ return inst->menuFnt; }
    static Tempest::Font font(){ return inst->mainFnt; }

    static Tempest::Texture2d *loadTexture(const char* name);
    static Tempest::Texture2d *loadTexture(const std::string& name);

    static StaticMesh         *loadStMesh (const std::string& name);

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
    Tempest::Texture2d *implLoadTexture(const std::string &name);
    StaticMesh         *implLoadStMesh (const std::string &name);
    MeshLoadCode        loadMesh(ZenLoad::PackedMesh &sPacked, ZenLoad::PackedSkeletalMesh &aPacked, std::string name);

    Tempest::Texture2d fallback;

    std::unordered_map<std::string,std::unique_ptr<Tempest::Texture2d>> texCache;
    std::unordered_map<std::string,std::unique_ptr<StaticMesh>>         stMeshCache;

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

}

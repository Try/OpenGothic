#include "protomesh.h"

#include <Tempest/Log>

#include "graphics/mesh/submesh/packedmesh.h"
#include "graphics/mesh/skeleton.h"
#include "physics/physicmeshshape.h"

using namespace Tempest;

ProtoMesh::Attach::~Attach() {
  }

ProtoMesh::ProtoMesh(PackedMesh&& pm, std::string_view fname)
  :fname(fname) {
  attach.emplace_back(pm);
  submeshId.resize(attach[0].sub.size());
  auto&  att   = attach[0];
  att.shape.reset(PhysicMeshShape::load(std::move(pm)));

  size_t count = 0;
  for(size_t r=0;r<att.sub.size();++r) {
    if(att.sub[r].material.tex==nullptr) {
      if(!att.sub[r].texName.empty())
        Tempest::Log::e("no texture: ",att.sub[r].texName);
      continue;
      }
    submeshId[count].id    = 0;
    submeshId[count].subId = r;
    count++;
    }
  submeshId.resize(count);

  nodes.emplace_back();
  nodes.back().attachId   = 0;
  nodes.back().submeshIdB = 0;
  nodes.back().submeshIdE = submeshId.size();
  nodes.back().transform.identity();

  bbox[0] = pm.bbox().first;
  bbox[1] = pm.bbox().second;
  setupScheme(fname);
  }

ProtoMesh::ProtoMesh(PackedMesh&& pm, const std::vector<phoenix::morph_animation>& aniList, std::string_view fname)
  : ProtoMesh(std::move(pm),fname) {
  if(attach.size()!=1) {
    Log::d("skip animations for: ",fname);
    return;
    }

  auto& device = Resources::device();

  const size_t ssboAlign      = device.properties().ssbo.offsetAlign;
  const size_t indexSz        = sizeof(int32_t[4])*((pm.verticesId.size()+3)/4);
  const size_t indexSzAligned = ((indexSz+ssboAlign-1)/ssboAlign)*ssboAlign;
  size_t       samplesCnt     = 0;

  for(auto& i:aniList) {
    samplesCnt += i.samples.size();
    }

  morphIndex   = Resources::ssbo(nullptr, indexSzAligned*aniList.size());
  morphSamples = Resources::ssbo(nullptr, samplesCnt*sizeof(Vec4));

  std::vector<int32_t> remapId;
  std::vector<Vec4>    samples;

  samplesCnt = 0;
  morph.resize(aniList.size());
  for(size_t i=0; i<aniList.size(); ++i) {
    remap(aniList[i],pm.verticesId,remapId,samples,samplesCnt);

    morphIndex  .update(remapId.data(), i*indexSzAligned,        remapId.size()*sizeof(remapId[0]));
    morphSamples.update(samples.data(), samplesCnt*sizeof(Vec4), samples.size()*sizeof(Vec4)      );

    morph[i] = mkAnimation(aniList[i]);
    morph[i].index = (i*indexSzAligned)/sizeof(int32_t);

    samplesCnt += samples.size();
    }

  if(morph.size()>0) {
    for(auto& a:attach) {
      a.morph.anim    = &morph;
      a.morph.index   = &morphIndex;
      a.morph.samples = &morphSamples;
      }
    }
  }

ProtoMesh::ProtoMesh(const phoenix::model &library, std::unique_ptr<Skeleton>&& sk, std::string_view fname)
  :skeleton(std::move(sk)), fname(fname) {
  for(auto& m:library.mesh.attachments) {
    PackedMesh pack(m.second,PackedMesh::PK_Visual);
    attach.emplace_back(pack);
    auto& att = attach.back();
    att.name = m.first;
    att.shape.reset(PhysicMeshShape::load(std::move(pack)));
    }

  nodes.resize(skeleton==nullptr ? 0 : skeleton->nodes.size());
  for(size_t i=0;i<nodes.size();++i) {
    Node& n   = nodes[i];
    auto& src = skeleton->nodes[i];
    for(size_t r=0;r<attach.size();++r)
      if(attach[r].name==src.name){
        n.attachId = r;
        break;
        }
    n.parentId  = src.parent;
    n.transform = src.tr;
    }

  for(auto& i:nodes)
    if(i.parentId<nodes.size())
      nodes[i.parentId].hasChild=true;

  size_t subCount=0;
  for(auto& i:nodes)
    if(i.attachId<attach.size()) {
      attach[i.attachId].hasNode = true;
      subCount+=attach[i.attachId].sub.size();
      }
  for(auto& a:attach)
    if(!a.hasNode)
      subCount+=a.sub.size();
  submeshId.resize(subCount);

  subCount=0;
  for(auto& i:nodes) {
    i.submeshIdB = subCount;
    if(i.attachId<attach.size()) {
      auto& att = attach[i.attachId];
      for(size_t r=0;r<att.sub.size();++r){
        if(att.sub[r].material.tex==nullptr) {
          if(!att.sub[r].texName.empty())
            Tempest::Log::e("no texture: ",att.sub[r].texName);
          continue;
          }
        submeshId[subCount].id    = i.attachId;
        submeshId[subCount].subId = r;
        subCount++;
        }
      }
    i.submeshIdE = subCount;
    }
  submeshId.resize(subCount);

  for(size_t i=0;i<library.mesh.meshes.size();++i){
    auto& mesh = library.mesh.meshes[i];
    PackedMesh pkg(mesh);
    skined.emplace_back(pkg);
    }

  if(skeleton!=nullptr) {
    for(size_t i=0;i<skeleton->nodes.size();++i) {
      auto& n=skeleton->nodes[i];
      if(n.name.find("ZS_POS")==0){
        Pos p;
        p.name      = n.name;
        p.node      = i;
        p.transform = n.tr;
        pos.push_back(p);
        }
      }
    }
  setupScheme(fname);
  }

ProtoMesh::ProtoMesh(const phoenix::model_hierarchy& library, std::unique_ptr<Skeleton>&& sk, std::string_view fname)
      :skeleton(std::move(sk)), fname(fname) {
  nodes.resize(skeleton == nullptr ? 0 : skeleton->nodes.size());
  for(size_t i = 0; i < nodes.size(); ++i) {
    Node& n = nodes[i];
    auto& src = skeleton->nodes[i];
    n.parentId = src.parent;
    n.transform = src.tr;
    }

  for(auto& i : nodes)
    if(i.parentId < nodes.size())
      nodes[i.parentId].hasChild = true;

  size_t subCount = 0;
  for(auto& i : nodes) {
    i.submeshIdB = subCount;
    i.submeshIdE = subCount;
    }
  submeshId.resize(subCount);

  if(skeleton != nullptr) {
    for(size_t i = 0; i < skeleton->nodes.size(); ++i) {
      auto& n = skeleton->nodes[i];
      if(n.name.find("ZS_POS") == 0) {
        Pos p;
        p.name = n.name;
        p.node = i;
        p.transform = n.tr;
        pos.push_back(p);
        }
      }
    }
  setupScheme(fname);
  }

ProtoMesh::ProtoMesh(const phoenix::model_mesh& library, std::unique_ptr<Skeleton>&& sk, std::string_view fname)
  :skeleton(std::move(sk)), fname(fname){
  for(auto& m:library.attachments) {
    PackedMesh pack(m.second,PackedMesh::PK_Visual);
    attach.emplace_back(pack);
    auto& att = attach.back();
    att.name = m.first;
    att.shape.reset(PhysicMeshShape::load(std::move(pack)));
    }

  nodes.resize(skeleton==nullptr ? 0 : skeleton->nodes.size());
  for(size_t i=0;i<nodes.size();++i) {
    Node& n   = nodes[i];
    auto& src = skeleton->nodes[i];
    for(size_t r=0;r<attach.size();++r)
      if(attach[r].name==src.name){
        n.attachId = r;
        break;
        }
    n.parentId  = src.parent;
    n.transform = src.tr;
    }

  for(auto& i:nodes)
    if(i.parentId<nodes.size())
      nodes[i.parentId].hasChild=true;

  size_t subCount=0;
  for(auto& i:nodes)
    if(i.attachId<attach.size()) {
      attach[i.attachId].hasNode = true;
      subCount+=attach[i.attachId].sub.size();
      }
  for(auto& a:attach)
    if(!a.hasNode)
      subCount+=a.sub.size();
  submeshId.resize(subCount);

  subCount=0;
  for(auto& i:nodes) {
    i.submeshIdB = subCount;
    if(i.attachId<attach.size()) {
      auto& att = attach[i.attachId];
      for(size_t r=0;r<att.sub.size();++r){
        if(att.sub[r].material.tex==nullptr) {
          if(!att.sub[r].texName.empty())
            Tempest::Log::e("no texture: ",att.sub[r].texName);
          continue;
          }
        submeshId[subCount].id    = i.attachId;
        submeshId[subCount].subId = r;
        subCount++;
        }
      }
    i.submeshIdE = subCount;
    }
  submeshId.resize(subCount);

  for(const auto &i : library.meshes){
    PackedMesh pkg(i);
    skined.emplace_back(pkg);
    }

  if(skeleton!=nullptr) {
    for(size_t i=0;i<skeleton->nodes.size();++i) {
      auto& n=skeleton->nodes[i];
      if(n.name.find("ZS_POS")==0){
        Pos p;
        p.name      = n.name;
        p.node      = i;
        p.transform = n.tr;
        pos.push_back(p);
        }
      }
    }
  setupScheme(fname);
  }

ProtoMesh::ProtoMesh(const Material& mat, std::vector<Resources::Vertex> vbo, std::vector<uint32_t> ibo) {
  attach.emplace_back(mat,std::move(vbo),std::move(ibo));
  submeshId.resize(attach[0].sub.size());
  auto& att = attach[0];

  size_t count = 0;
  for(size_t r=0;r<att.sub.size();++r) {
    if(att.sub[r].material.tex==nullptr) {
      if(!att.sub[r].texName.empty())
        Tempest::Log::e("no texture: ",att.sub[r].texName);
      continue;
      }
    submeshId[count].id    = 0;
    submeshId[count].subId = r;
    count++;
    }
  submeshId.resize(count);

  nodes.emplace_back();
  nodes.back().attachId   = 0;
  nodes.back().submeshIdB = 0;
  nodes.back().submeshIdE = submeshId.size();
  nodes.back().transform.identity();

  setupScheme("");
  }

ProtoMesh::~ProtoMesh() {
  }

size_t ProtoMesh::skinedNodesCount() const {
  size_t ret=0;
  for(auto& i:skined)
    ret+=i.sub.size();
  return ret;
  }

Tempest::Matrix4x4 ProtoMesh::mapToRoot(size_t n) const {
  Tempest::Matrix4x4 m;
  m.identity();

  while(n<nodes.size()) {
    auto& nx = nodes[n];
    auto  mx = nx.transform;
    mx.mul(m);
    m = mx;
    n = nx.parentId;
    }
  return m;
  }

size_t ProtoMesh::findNode(std::string_view name, size_t def) const {
  if(skeleton==nullptr)
    return def;
  return skeleton->findNode(name,def);
  }

void ProtoMesh::setupScheme(std::string_view s) {
  auto sep = s.find("_");
  if(sep!=std::string::npos) {
    scheme = s.substr(0,sep);
    return;
    }
  sep = s.rfind(".");
  if(sep!=std::string::npos) {
    scheme = s.substr(0,sep);
    return;
    }
  scheme = s;
  }

void ProtoMesh::remap(const phoenix::morph_animation& a,
                      const std::vector<uint32_t>& vertId,
                      std::vector<int32_t>&        remap,
                      std::vector<Tempest::Vec4>&  samples,
                      size_t idOffset) {
  size_t indexSz = ((vertId.size()+3)/4)*4;
  remap.resize(indexSz);
  for(auto& i:remap)
    i = -1;
  for(size_t i=0; i<vertId.size(); ++i) {
    const uint32_t id = vertId[i];
    for(size_t r=0; r<a.vertices.size(); ++r)
      if(a.vertices[r]==id) {
        remap[i] = int(r+idOffset);
        break;
      }
  }

  samples.resize(a.samples.size());
  for(size_t i=0; i<a.samples.size(); ++i) {
    auto& s = a.samples[i];
    samples[i] = Tempest::Vec4(s.x,s.y,s.z,0);
  }
}

ProtoMesh::Morph ProtoMesh::mkAnimation(const phoenix::morph_animation& a) {
  Morph ret;
  ret.name            = a.name;
  ret.numFrames       = a.frame_count;
  ret.samplesPerFrame = a.samples.size()/a.frame_count;
  ret.layer           = a.layer;
  ret.duration        = uint64_t(a.duration>0 ? a.duration : 0);

  if(a.flags&0x2 || a.duration<=0)
    ret.tickPerFrame = size_t(1.f/a.speed); else
    ret.tickPerFrame = size_t(a.duration/float(a.frame_count));

  if(ret.tickPerFrame==0)
    ret.tickPerFrame = 1;
  return ret;
  }

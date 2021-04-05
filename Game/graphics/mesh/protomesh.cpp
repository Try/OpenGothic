#include "protomesh.h"

#include <Tempest/Log>

#include "physics/physicmeshshape.h"

ProtoMesh::Attach::~Attach() {
  }

ProtoMesh::ProtoMesh(const ZenLoad::zCModelMeshLib &library, const std::string &fname)
  :fname(fname) {
  for(auto& m:library.getAttachments()) {
    ZenLoad::PackedMesh stat;
    m.second.packMesh(stat);
    attach.emplace_back(stat);
    auto& att = attach.back();
    att.name = m.first;
    att.shape.reset(PhysicMeshShape::load(std::move(stat)));
    }

  nodes.resize(library.getNodes().size());
  for(size_t i=0;i<nodes.size();++i) {
    Node& n   = nodes[i];
    auto& src = library.getNodes()[i];
    for(size_t r=0;r<attach.size();++r)
      if(library.getAttachments()[r].first==src.name){
        n.attachId = r;
        break;
        }
    n.parentId = (src.parentIndex==uint16_t(-1) ? size_t(-1) : src.parentIndex);
    std::memcpy(reinterpret_cast<void*>(&n.transform),reinterpret_cast<const void*>(&src.transformLocal),sizeof(n.transform));
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

  for(size_t i=0;i<library.getMeshes().size();++i){
    ZenLoad::PackedSkeletalMesh pack;
    auto& mesh = library.getMeshes()[i];
    mesh.packMesh(pack);
    skined.emplace_back(pack);
    }

  for(size_t i=0;i<library.getNodes().size();++i) {
    auto& n=library.getNodes()[i];
    if(n.name.find("ZS_POS")==0){
      Pos p;
      p.name = n.name;
      p.node = i;
      std::memcpy(reinterpret_cast<void*>(&p.transform),reinterpret_cast<const void*>(&n.transformLocal),sizeof(p.transform));
      pos.push_back(p);
      }
    }
  setupScheme(fname);
  }

ProtoMesh::ProtoMesh(ZenLoad::PackedMesh&& pm, const std::string& fname)
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

  bbox[0] = Tempest::Vec3(pm.bbox[0].x,pm.bbox[0].y,pm.bbox[0].z);
  bbox[1] = Tempest::Vec3(pm.bbox[1].x,pm.bbox[1].y,pm.bbox[1].z);
  setupScheme(fname);
  }

ProtoMesh::ProtoMesh(ZenLoad::PackedMesh&& pm,
                     const std::vector<ZenLoad::zCMorphMesh::Animation>& aniList,
                     const std::string& fname)
  : ProtoMesh(std::move(pm),fname) {
  if(attach.size()!=1) {
    Tempest::Log::d("skip animations for: ",fname);
    return;
    }

  morph.resize(aniList.size());
  for(size_t i=0; i<aniList.size(); ++i) {
    morph[i] = mkAnimation(aniList[i],pm.verticesId);
    }
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

  //bbox[0] = Tempest::Vec3(pm.bbox[0].x,pm.bbox[0].y,pm.bbox[0].z);
  //bbox[1] = Tempest::Vec3(pm.bbox[1].x,pm.bbox[1].y,pm.bbox[1].z);
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

void ProtoMesh::setupScheme(const std::string &s) {
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

ProtoMesh::Animation ProtoMesh::mkAnimation(const ZenLoad::zCMorphMesh::Animation& a,
                                            const std::vector<uint32_t>& vertId) {
  std::vector<int32_t> remap(vertId.size(),-1);
  for(size_t i=0; i<vertId.size(); ++i) {
    const uint32_t id = vertId[i];
    for(size_t r=0; r<a.vertexIndex.size(); ++r)
      if(a.vertexIndex[r]==id) {
        remap[i] = int(r);
        break;
        }
    }
  remap.resize(((remap.size()+3)/4)*4);

  Animation ret;
  ret.name            = a.name;
  ret.numFrames       = a.numFrames;
  ret.samplesPerFrame = a.samples.size()/a.numFrames;
  ret.index           = Resources::ssbo(remap.data(),remap.size()*sizeof(remap[0]));

  if(a.flags&0x2 || a.duration<=0)
    ret.tickPerFrame = size_t(1.f/a.speed); else
    ret.tickPerFrame = size_t(a.duration/float(a.numFrames));

  if(ret.tickPerFrame==0)
    ret.tickPerFrame = 1;

  std::vector<Tempest::Vec4> samplesAligned(a.samples.size());
  for(size_t i=0; i<a.samples.size(); ++i) {
    auto& s = a.samples[i];
    samplesAligned[i] = Tempest::Vec4(s.x,s.y,s.z,0);
    }
  ret.samples = Resources::ssbo(samplesAligned.data(),samplesAligned.size()*sizeof(samplesAligned[0]));
  return ret;
  }

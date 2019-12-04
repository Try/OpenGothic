#include "attachbinder.h"

#include "protomesh.h"
#include "skeleton.h"

AttachBinder::AttachBinder(const Skeleton &s, const ProtoMesh &m, const char *defBone)
  :defBone(defBone==nullptr ? "" : defBone) {
  bind.resize(m.submeshId.size());
  size_t id = s.findNode(defBone);

  for(size_t i=0;i<m.submeshId.size();++i){
    auto& att = m.attach[m.submeshId[i].id];
    bind[i].boneId = s.findNode(att.name,id);
    }
  }

#include "attachbinder.h"

#include "protomesh.h"
#include "skeleton.h"

AttachBinder::AttachBinder(const Skeleton &s, const ProtoMesh &m) {
  bind.resize(m.submeshId.size());

  for(size_t i=0;i<m.submeshId.size();++i){
    auto& att = m.attach[m.submeshId[i].id];
    bind[i] = s.findNode(att.name);
    }
  }

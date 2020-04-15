#pragma once

#include <cstdlib>
#include <vector>
#include <string>

class ProtoMesh;
class Skeleton;

class AttachBinder final {
  public:
    AttachBinder()=default;
    AttachBinder(const Skeleton& s,const ProtoMesh& m);

    std::vector<size_t> bind;
  };

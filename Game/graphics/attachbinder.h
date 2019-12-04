#pragma once

#include <cstdlib>
#include <vector>
#include <string>

class ProtoMesh;
class Skeleton;

class AttachBinder final {
  public:
    AttachBinder()=default;
    AttachBinder(const Skeleton& s,const ProtoMesh& m,const char* defBone);

    struct Bind final {
      size_t boneId  =0;
      };
    std::vector<Bind> bind;
    std::string       defBone;
  };

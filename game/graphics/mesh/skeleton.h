#pragma once

#include <Tempest/Matrix4x4>

#include <phoenix/model_hierarchy.hh>

#include <vector>
#include <array>

#include "animation.h"

class Skeleton final {
  public:
    Skeleton(const phoenix::model_hierarchy& src, const Animation* anim, std::string_view name);

    struct Node final {
      size_t             parent=size_t(-1);
      Tempest::Matrix4x4 tr;
      std::string        name;
      };

    bool                            ordered=true;
    std::vector<Node>               nodes;
    std::vector<size_t>             rootNodes;
    std::vector<Tempest::Matrix4x4> tr;
    Tempest::Vec3                   rootTr={};

    size_t                          BIP01_HEAD = size_t(-1);

    Tempest::Vec3                   bboxCol[2]={};

    size_t                          findNode(std::string_view   name,size_t def=size_t(-1)) const;

    const std::string&              name() const { return fileName; }
    const Animation::Sequence*      sequence(std::string_view name) const;
    const Animation*                animation() const { return anim; }
    void                            debug() const;
    const std::string&              defaultMesh() const;

    float                           colisionHeight() const;
    float                           colisionRadius() const;

  private:
    std::string      fileName;
    const Animation* anim=nullptr;

    void mkSkeleton();
    void mkSkeleton(const Tempest::Matrix4x4& mt,size_t parent);
  };

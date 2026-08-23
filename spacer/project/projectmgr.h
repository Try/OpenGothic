#pragma once

#include <Tempest/Signal>
#include <cstddef>

#include "project/projectitem.h"

namespace zenkit
{
  class VfsNode;
};

class ProjectMgr {
  public:
    ProjectMgr();
    ~ProjectMgr();

    static ProjectMgr& inst();

    size_t       vdfCount() const;
    ProjectItem  vdf(size_t id);
    ProjectItem  root();

    Tempest::Signal<void()> onGpuAssetChanged;

  private:
    std::shared_ptr<ProjectItem::Data> mkIndex(const zenkit::VfsNode& node, size_t depth, const std::string& prefix);

    std::shared_ptr<ProjectItem::Data> rootItem;
    std::shared_ptr<ProjectItem::Data> files;
  };

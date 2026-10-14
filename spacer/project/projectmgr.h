#pragma once

#include <Tempest/Signal>
#include <cstddef>
#include <filesystem>

#include "project/projectitem.h"

namespace zenkit
{
  class VfsNode;
};

class Gothic;

class ProjectMgr {
  public:
    ProjectMgr();
    ~ProjectMgr();

    static ProjectMgr& inst();

    bool         hasVisibleProject() const;
    void         unloadAll();
    bool         addProject(const std::filesystem::path& proj);

    size_t       vdfCount() const;
    ProjectItem  vdf(size_t id);
    ProjectItem  root();

    ProjectItem  find(std::string_view name);

    Tempest::Signal<void()> onGpuAssetChanged;

  private:
    std::shared_ptr<ProjectItem::Data> mkIndex(const zenkit::VfsNode& node, size_t depth, const std::string& prefix);
    ProjectItem  find(const ProjectItem& itm, std::string_view name);

    std::shared_ptr<Gothic>            gothic;
    std::shared_ptr<ProjectItem::Data> rootItem;
  };

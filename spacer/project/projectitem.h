#pragma once

#include "utility/spinlock.h"
#include <string_view>
#include <string>
#include <vector>
#include <memory>

#include <Tempest/Texture2d>

class ProjectItem {
  public:
    ProjectItem();

    enum Type {
      T_Project,
      T_Dir,
      T_File,
      T_StaticMesh,
      };

    std::string_view      displayName() const;
    std::string_view      name() const;
    Type                  type() const;
    bool                  isVisible() const;

    std::string_view      path() const;
    size_t                depth() const;

    size_t                itemsCount() const;
    ProjectItem           item(size_t i) const;

    auto                  preview() const -> std::shared_ptr<Tempest::Texture2d>;

  private:
    struct Data {
      std::vector<std::shared_ptr<ProjectItem::Data>> files;
      std::string name;
      std::string path;
      size_t      depth = 0;

      SpinLock    sync;
      std::shared_ptr<Tempest::Texture2d> preview;
      };

    ProjectItem(std::shared_ptr<Data> data);

    void setPreview(std::shared_ptr<Tempest::Texture2d> preview);

    std::shared_ptr<Data> data;

  friend class ProjectMgr;
  friend class DataWorker;
  };

#pragma once

#include <string_view>
#include <filesystem>
#include <vector>

class ProjectItem {
  public:
    ProjectItem();

    enum Type {
      T_Project,
      T_Dir,
      T_File
      };

    std::string_view      displayName() const;
    std::string_view      name() const;
    std::string_view      path() const;
    size_t                depth() const;
    Type                  type() const;
    ProjectItem*          projectFile() const;
    bool                  isVisible() const;

    size_t                itemsCount() const;
    ProjectItem           item(size_t i) const;

  private:
    struct Data {
      std::vector<std::shared_ptr<ProjectItem::Data>> files;
      std::string name;
      std::string path;
      size_t      depth    = 0;
      };

    ProjectItem(std::shared_ptr<Data> data);

    std::shared_ptr<Data> data;

  friend class ProjectMgr;
  };

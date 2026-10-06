#pragma once

#include "utility/spinlock.h"
#include <string_view>
#include <string>
#include <vector>
#include <memory>

#include <Tempest/Texture2d>

class WorldEdit;
class ProtoMesh;

class ProjectItem {
  public:
    ProjectItem();

    bool operator == (const ProjectItem& other) const;
    bool operator != (const ProjectItem& other) const;

    enum Type {
      T_Project,
      T_Dir,
      T_File,
      T_StaticMesh,
      T_Texture,
      T_World,
      };

    enum State : uint32_t {
      S_Idle,
      S_Pending,
      S_Ready,
      S_Error,
      };

    std::string_view      displayName() const;
    std::string_view      name() const;
    Type                  type() const;
    bool                  isReady() const;
    bool                  isEmpty() const;
    bool                  isPending() const;
    bool                  isVisible() const;

    std::string_view      path() const;
    size_t                depth() const;

    size_t                itemsCount() const;
    ProjectItem           item(size_t i) const;

    auto                  preview() const -> std::shared_ptr<const Tempest::Texture2d>;
    auto                  get() const -> std::shared_ptr<WorldEdit>;

  private:
    struct Data {
      std::vector<std::shared_ptr<ProjectItem::Data>> files;
      std::string name;
      std::string path;
      size_t      depth = 0;
      State       state = S_Idle;

      SpinLock    sync;
      std::shared_ptr<const Tempest::Texture2d> preview;

      std::shared_ptr<WorldEdit> world;
      };

    ProjectItem(std::shared_ptr<Data> data);

    void setPending();
    void setError();
    void setPreview(std::shared_ptr<const Tempest::Texture2d> preview);
    void setPayload(std::shared_ptr<WorldEdit> payload);
    void setPayload(const Tempest::Texture2d* payload);
    void setPayload(const ProtoMesh* payload);

    std::shared_ptr<Data> data;

  friend class ProjectMgr;
  friend class DataWorker;
  };

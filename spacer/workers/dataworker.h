#pragma once

#include <thread>
#include <mutex>
#include <condition_variable>

#include "graphics/meshobjects.h"
#include "graphics/sceneglobals.h"
#include "graphics/visualobjects.h"

class ProjectItem;

class DataWorker {
  public:
    DataWorker();
    ~DataWorker();

    static DataWorker& inst();
    static void        load(const ProjectItem& it);

  private:
    void exec();
    void pushItem(const ProjectItem& it);
    bool popItem(ProjectItem& out);
    void commit(ProjectItem& out, std::function<void ()> func);

    auto createPreview(ProjectItem& itm, const MeshObjects::Mesh& mesh, const Tempest::Vec3* bbox) -> std::shared_ptr<Tempest::Texture2d>;

    std::thread              th;

    std::mutex               sync;
    std::condition_variable  workWait;
    std::vector<ProjectItem> items;
    bool                     isExit = false;

    struct {
      SceneGlobals     scene;
      VisualObjects    visual   = {scene, std::pair<Tempest::Vec3,Tempest::Vec3>()};
      MeshObjects      itmGroup = {visual};
      } render;
  };

#include "projectmgr.h"

#include "projectitem.h"
#include "resources.h"

ProjectMgr::ProjectMgr() {
  rootItem = mkIndex(Resources::vdfsIndex().root(), size_t(-1), "");
  }

ProjectMgr::~ProjectMgr() {
  }

ProjectMgr& ProjectMgr::inst() {
  //FIXME: need to have well defined life-time
  static ProjectMgr p;
  return p;
  }

std::shared_ptr<ProjectItem::Data> ProjectMgr::mkIndex(const zenkit::VfsNode& node, size_t depth, const std::string& prefix) {
  auto d = std::make_shared<ProjectItem::Data>();
  d->name  = node.name();
  d->depth = depth;
  if(depth!=size_t(-1))
    d->path = prefix + node.name();

  if(node.type()==zenkit::VfsNodeType::DIRECTORY) {
    auto path = d->path + "/";
    for(auto& i:node.children()) {
      d->files.emplace_back(mkIndex(i, depth+1, path));
      }
    }

  return d;
  }

size_t ProjectMgr::vdfCount() const {
  //TODO
  return 1;
  }

ProjectItem ProjectMgr::vdf(size_t id) {
  ProjectItem itm;
  itm.data = rootItem;
  return itm;
  }

ProjectItem ProjectMgr::root() {
  ProjectItem itm;
  itm.data = rootItem;
  return itm;
  }

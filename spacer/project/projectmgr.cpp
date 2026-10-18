#include "projectmgr.h"

#include "workers/dataworker.h"
#include "projectitem.h"
#include "gothic.h"
#include "utils/inifile.h"
#include "resources.h"
#include <cassert>

static ProjectMgr* instance = nullptr;

ProjectMgr::ProjectMgr() {
  instance = this;
  gothic   = std::make_shared<Gothic>();
  gothic->setupGlobalScripts();
  rootItem = mkIndex(Resources::vdfsIndex().root(), size_t(-1), "");
  }

ProjectMgr::~ProjectMgr() {
  instance = nullptr;
  }

ProjectMgr& ProjectMgr::inst() {
  return *instance;
  }

bool ProjectMgr::hasVisibleProject() const {
  return gothic!=nullptr;
  }

void ProjectMgr::unloadAll() {
  DataWorker::unloadAll();
  Resources::unloadAll();
  rootItem.reset();
  gothic.reset();
  }

bool ProjectMgr::addProject(const std::filesystem::path& proj) {
  assert(gothic==nullptr);
  //TODO: handle various read exceptions
  auto gothicPath = proj.parent_path().parent_path();
  GameModDescriptor desc = {proj.u16string(), gothicPath.u16string() + u"/Data/"};

  gothic = std::make_shared<Gothic>(std::move(desc));
  gothic->setupGlobalScripts();
  rootItem = mkIndex(Resources::vdfsIndex().root(), size_t(-1), "");
  return true;
  }

std::shared_ptr<ProjectItem::Data> ProjectMgr::mkIndex(const zenkit::VfsNode& node, size_t depth, const std::string& prefix) {
  auto d = std::make_shared<ProjectItem::Data>();
  d->name  = node.name();
  d->depth = depth;
  if(depth!=size_t(-1))
    d->path = prefix + node.name();

  if(node.type()==zenkit::VfsNodeType::DIRECTORY) {
    auto path = d->path;
    if(!path.empty())
      path += "/";
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

ProjectItem ProjectMgr::find(std::string_view name) {
  return find(root(), name);
  }

ProjectItem ProjectMgr::find(const ProjectItem& itm, std::string_view name) {
  if(itm.name()==name)
    return itm;
  for(size_t i=0; i<itm.itemsCount(); ++i) {
    auto n = find(itm.item(i), name);
    if(!n.isEmpty())
      return n;
    }
  return ProjectItem();
  }

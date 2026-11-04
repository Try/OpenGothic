#include "projectmgr.h"

#include "utils/fileext.h"
#include "workers/dataworker.h"
#include "projectitem.h"
#include "gothic.h"
#include "resources.h"
#include <cassert>
#include <Tempest/Log>

static ProjectMgr* instance = nullptr;

ProjectItem::Type fileTypeByName(std::string_view name) {
  if(FileExt::hasExt(name,"3DS"))
    return ProjectItem::T_StaticMesh;
  if(FileExt::hasExt(name,"MRM"))
    return ProjectItem::T_StaticMesh;

  if(FileExt::hasExt(name,"TEX"))
    return ProjectItem::T_Texture;
  if(FileExt::hasExt(name,"TGA"))
    return ProjectItem::T_Texture;

  if(FileExt::hasExt(name,"ZEN"))
    return ProjectItem::T_World;

  return ProjectItem::T_File;
  }

ProjectMgr::ProjectMgr() {
  instance = this;
  //gothic   = std::make_shared<Gothic>();
  //gothic->setupGlobalScripts();
  //rootItem = mkIndex(Resources::vdfsIndex().root(), size_t(-1), "");
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
  itemRoot = mkIndexDat(gothic->defaultGameDatFile());
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
    } else {
    d->type = fileTypeByName(d->name);
    }

  return d;
  }

std::shared_ptr<ProjectItem::Data> ProjectMgr::mkIndexDat(std::string_view datFile) {
  zenkit::DaedalusScript script = loadScript(datFile);
  if(script.size()==0)
    return nullptr;

  auto items = std::make_shared<ProjectItem::Data>();
  items->name  = "C_Item";
  items->depth = 0;

  // danger - script can fail!
  try {
    gothic->setupCommonScriptClasses(script);

    const auto exef =
        zenkit::DaedalusVmExecutionFlag::ALLOW_NULL_INSTANCE_ACCESS |
        zenkit::DaedalusVmExecutionFlag::IGNORE_CONST_SPECIFIER;
    auto vm = std::make_unique<zenkit::DaedalusVm>(std::move(script), exef);
    gothic->setupVmCommonApi(*vm);

    vm->enumerate_instances_by_class_name("C_Item", [this, &items, &vm](zenkit::DaedalusSymbol& s) {
      try {
        auto d = std::make_shared<ProjectItem::Data>();
        d->name  = s.name();
        d->depth = 1;
        d->path  = s.name();
        d->type  = ProjectItem::T_ScriptItem;

        auto data = std::make_shared<zenkit::IItem>();
        vm->init_instance<zenkit::IItem>(data, &s);
        d->item = data;

        items->files.push_back(d);
        }
      catch(const zenkit::DaedalusScriptError&) {
        // There was an error during initialization. Ignore it.
        }
      });
    }
  catch(const zenkit::DaedalusScriptError&) {
    Tempest::Log::d("failed script reflection at ", __func__);
    }

  return items;
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

ProjectItem ProjectMgr::cItemRoot() {
  ProjectItem itm;
  itm.data = itemRoot;
  return itm;
  }

ProjectItem ProjectMgr::find(std::string_view name) {
  return find(root(), name);
  }

ProjectItem ProjectMgr::findCItem(std::string_view name) {
  return findCItem(cItemRoot(), name);
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

ProjectItem ProjectMgr::findCItem(const ProjectItem& itm, std::string_view name) {
  if(itm.name()==name)
    return itm;
  for(size_t i=0; i<itm.itemsCount(); ++i) {
    auto n = findCItem(itm.item(i), name);
    if(!n.isEmpty())
      return n;
    }
  return ProjectItem();
  }

zenkit::DaedalusScript ProjectMgr::loadScript(std::string_view datFile) {
  if(Resources::hasFile(datFile)) {
    auto buf = Resources::getFileBuffer(datFile);
    zenkit::DaedalusScript script;
    script.load(buf.get());
    return script;
    }

  const size_t segment = datFile.find_last_of("\\/");
  if(segment!=std::string::npos && Resources::hasFile(datFile.substr(segment+1))) {
    auto buf = Resources::getFileBuffer(datFile.substr(segment+1));
    zenkit::DaedalusScript script;
    script.load(buf.get());
    return script;
    }

  return zenkit::DaedalusScript();
  }

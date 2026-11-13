#include "projectmgr.h"

#include <Tempest/TextCodec>
#include <Tempest/Log>
#include <cassert>

#include "workers/dataworker.h"
#include "utils/fileext.h"
#include "utils/fileutil.h"
#include "utils/srcparser.h"
#include "projectitem.h"
#include "gothic.h"
#include "resources.h"

using namespace Tempest;

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
  auto gothicPath = proj.parent_path().parent_path().u16string();
  auto dataPath   = FileUtil::nestedPath(gothicPath, {u"Data"},  Dir::FT_Dir);
  auto workPath   = FileUtil::nestedPath(gothicPath, {u"_work"}, Dir::FT_Dir);

  GameModDescriptor desc = {proj.u16string(), dataPath, workPath};

  //rootItem         = mkIndex(Resources::vdfsIndex().root(), size_t(-1), "");
  //itemRoot         = mkIndexItems(gothic->defaultGameDatFile());
  //lightPresetsRoot = mkIndexLightPresets(find("LIGHTPRESETS.ZEN"));
  srcDatRoot       = mkIndexScripts(desc, TextCodec::toUtf8(workPath));

  gothic = std::make_shared<Gothic>(std::move(desc));
  gothic->setupGlobalScripts();

  itemRoot         = mkIndexItems(gothic->defaultGameDatFile());
  rootItem         = mkIndex(Resources::vdfsIndex().root(), size_t(-1), "");
  lightPresetsRoot = mkIndexLightPresets(find("LIGHTPRESETS.ZEN"));

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

std::shared_ptr<ProjectItem::Data> ProjectMgr::mkIndexItems(std::string_view datFile) {
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

std::shared_ptr<ProjectItem::Data> ProjectMgr::mkIndexLightPresets(ProjectItem asset) {
  if(asset.isEmpty())
    return nullptr;

  auto items = std::make_shared<ProjectItem::Data>();
  items->name  = "LightPresets";
  items->depth = 0;

  {
    // simple light must always be available
    auto d = std::make_shared<ProjectItem::Data>();
    d->name  = "Light";
    d->depth = 1;
    d->path  = "builtin";
    d->type  = ProjectItem::T_LightPreset;
    d->light = std::make_shared<zenkit::LightPreset>();
    d->state = ProjectItem::S_Ready;
    items->files.push_back(d);

    auto& lx = *d->light;
    lx.color = zenkit::Color(255, 255, 255, 255);
    lx.range = 500;
    lx.on    = true;
  }

  try {
    std::unique_ptr<zenkit::Read> read;
    auto zen = Resources::openReader(asset.name(), read);

    zenkit::ArchiveObject obj {};
    auto count = zen->read_int();
    for(int i = 0; i < count; ++i) {
      zen->read_object_begin(obj);

      zenkit::LightPreset preset {};
      preset.load(*zen, Gothic::inst().version().game == 1 ? zenkit::GameVersion::GOTHIC_1
                                                           : zenkit::GameVersion::GOTHIC_2);
      if(!zen->read_object_end()) {
        zen->skip_object(true);
        }

      auto d = std::make_shared<ProjectItem::Data>();
      d->name  = preset.preset;
      d->depth = 1;
      d->path  = preset.preset;
      d->type  = ProjectItem::T_LightPreset;
      d->light = std::make_shared<zenkit::LightPreset>(std::move(preset));
      d->state = ProjectItem::S_Ready;
      items->files.push_back(d);
      }
    }
  catch(...) {
    Log::e("unable to load Zen-file: \"LIGHTPRESETS.ZEN\"");
    }
  return items;
  }

std::shared_ptr<ProjectItem::Data> ProjectMgr::mkIndexScripts(const GameModDescriptor& d, std::string_view root) {
  auto items = std::make_shared<ProjectItem::Data>();
  items->name  = "Source";
  items->depth = 0;

  for(auto& i:d.src) {
    auto p = std::filesystem::path(i).parent_path();
    SrcParser parser(i);
    for(auto& path:parser.files) {
      addScriptSources(items, p.string(), path);
      }
    }

  return items;
  }

void ProjectMgr::addScriptSources(std::shared_ptr<ProjectItem::Data> out,
                                  std::string_view root, std::string_view path) {
  // https://gothic-modding-community.github.io/gmc/zengin/scripts/general_info/compilation/
  auto matchWildcard = [](std::string_view file, std::string_view pattern){
    size_t p = 0, f = 0;
    size_t star  = std::string_view::npos;
    size_t retry = 0;

    while (f < file.size()) {
      if (p < pattern.size() && (pattern[p] == '?' || std::tolower(pattern[p]) == std::tolower(file[f]))) {
        ++p;
        ++f;
        }
      else if (p < pattern.size() && pattern[p] == '*') {
        star = p++;
        retry = f;
        }
      else if (star != std::string_view::npos) {
        p = star + 1;
        f = ++retry;
        }
      else {
        return false;
        }
      }

    while (p < pattern.size() && pattern[p] == '*') {
      ++p;
      }

    return p == pattern.size();
    };

  for(size_t i=0;; ++i) {
    if(i==path.size() || path[i]=='\\' || path[i]=='/') {
      if(i==path.size() && path.size()>0 && path.find('*')!=std::string::npos) {
        Dir::scan(std::string(root), [&](const std::string& p, Dir::FileType t) {
          if(t!=Dir::FT_File)
            return;
          if(!matchWildcard(p, path))
            return;

          auto d = std::make_shared<ProjectItem::Data>();
          d->name  = p;
          d->path  = std::string(root) + "/" + d->name;
          d->type  = ProjectItem::T_Script;
          out->files.push_back(d);
          });
        return;
        }

      if(i==path.size()) {
        auto d = std::make_shared<ProjectItem::Data>();
        d->name  = path;
        d->path  = std::string(root) + "/" + d->name;
        d->type  = ProjectItem::T_Script;
        out->files.push_back(d);
        return;
        }

      auto name = path.substr(0, i);
      std::shared_ptr<ProjectItem::Data> d;
      for(auto& dx:out->files) {
        //TODO: carefult about mixed cases
        if(dx->name==name) {
          d = dx;
          break;
          }
        }
      if(d==nullptr) {
        d = std::make_shared<ProjectItem::Data>();
        d->name  = name;
        d->path  = std::string(root) + "/" + d->name;
        d->type  = ProjectItem::T_Dir;
        out->files.push_back(d);
        }

      out  = d;
      root = out->path;
      path = path.substr(i+1);
      i    = 0;
      }
    }
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

ProjectItem ProjectMgr::lightsRoot() {
  ProjectItem itm;
  itm.data = lightPresetsRoot;
  return itm;
  }

ProjectItem ProjectMgr::scriptRoot() {
  ProjectItem itm;
  itm.data = srcDatRoot;
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

#include "projectitem.h"

#include "workers/dataworker.h"
#include "utils/fileext.h"

ProjectItem::ProjectItem() {
  }

ProjectItem::ProjectItem(std::shared_ptr<Data> data):data(data) {
  }

std::string_view ProjectItem::displayName() const {
  return data ? std::string_view(data->name) : "";
  }

std::string_view ProjectItem::name() const {
  return data ? std::string_view(data->name) : "";
  }

std::string_view ProjectItem::path() const {
  return data ? std::string_view(data->path) : "";
  }

size_t ProjectItem::depth() const {
  return data ? data->depth : 0;
  }

bool ProjectItem::isVisible() const {
  return true;
  }

ProjectItem::Type ProjectItem::type() const {
  if(data==nullptr)
    return T_File;

  if(data->files.size()>0)
    return T_Dir;

  if(FileExt::hasExt(data->name,"3DS"))
    return T_StaticMesh;
  if(FileExt::hasExt(data->name,"MRM"))
    return T_StaticMesh;

  return T_File;
  }

size_t ProjectItem::itemsCount() const {
  return data ? data->files.size() : 0;
  }

ProjectItem ProjectItem::item(size_t i) const {
  return ProjectItem(data->files[i]);
  }

auto ProjectItem::preview() const -> std::shared_ptr<Tempest::Texture2d> {
  if(data==nullptr)
    return nullptr;
  std::lock_guard<SpinLock> guard(data->sync);
  if(data->preview==nullptr)
    DataWorker::load(*this);
  return data->preview;
  }

void ProjectItem::setPreview(std::shared_ptr<Tempest::Texture2d> preview) {
  if(data==nullptr)
    return;
  std::lock_guard<SpinLock> guard(data->sync);
  data->preview = preview;
  }

#include "projectitem.h"

#include "workers/dataworker.h"
#include "utils/fileext.h"

ProjectItem::ProjectItem() {
  }

ProjectItem::ProjectItem(std::shared_ptr<Data> data):data(data) {
  }

bool ProjectItem::operator ==(const ProjectItem& other) const {
  return data==other.data;
  }

bool ProjectItem::operator !=(const ProjectItem& other) const {
  return data!=other.data;
  }

std::string_view ProjectItem::displayName() const {
  return data ? std::string_view(data->name) : "";
  }

std::string_view ProjectItem::name() const {
  return data ? std::string_view(data->name) : "";
  }

bool ProjectItem::isReady() const {
  if(type()==T_World)
    return get()!=nullptr; //WIP
  return true;
  }

bool ProjectItem::isEmpty() const {
  return data==nullptr;
  }

bool ProjectItem::isPending() const {
  std::lock_guard<SpinLock> guard(data->sync);
  return data!=nullptr && data->state==S_Pending;
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

  if(FileExt::hasExt(data->name,"TEX"))
    return T_Texture;
  if(FileExt::hasExt(data->name,"TGA"))
    return T_Texture;

  if(FileExt::hasExt(data->name,"ZEN"))
    return T_World;

  return T_File;
  }

size_t ProjectItem::itemsCount() const {
  return data ? data->files.size() : 0;
  }

ProjectItem ProjectItem::item(size_t i) const {
  return ProjectItem(data->files[i]);
  }

auto ProjectItem::preview() const -> std::shared_ptr<const Tempest::Texture2d> {
  if(data==nullptr)
    return nullptr;
  {
    std::lock_guard<SpinLock> guard(data->sync);
    if(data->preview!=nullptr)
      return data->preview;
  }
  DataWorker::load(*this);
  return nullptr;
  }

auto ProjectItem::get() const -> std::shared_ptr<WorldEdit> {
  if(data==nullptr)
    return nullptr;
  {
    std::lock_guard<SpinLock> guard(data->sync);
    if(data->world!=nullptr)
      return data->world;
  }
  DataWorker::load(*this);
  return nullptr;
  }

void ProjectItem::setPending() {
  std::lock_guard<SpinLock> guard(data->sync);
  data->state = S_Pending;
  }

void ProjectItem::setError() {
  std::lock_guard<SpinLock> guard(data->sync);
  data->state = S_Error;
  }

void ProjectItem::setPreview(std::shared_ptr<const Tempest::Texture2d> preview) {
  if(data==nullptr)
    return;
  std::lock_guard<SpinLock> guard(data->sync);
  data->preview = preview;
  }

void ProjectItem::setPayload(std::shared_ptr<WorldEdit> payload) {
  if(data==nullptr)
    return;
  std::lock_guard<SpinLock> guard(data->sync);
  data->world = payload;
  data->state = S_Ready;
  }

void ProjectItem::setPayload(const Tempest::Texture2d*) {
  if(data==nullptr)
    return;
  std::lock_guard<SpinLock> guard(data->sync);
  // data->world = payload;
  data->state = S_Ready;
  }

void ProjectItem::setPayload(const ProtoMesh* payload) {
  if(data==nullptr)
    return;
  std::lock_guard<SpinLock> guard(data->sync);
  // data->world = payload;
  data->state = S_Ready;
  }

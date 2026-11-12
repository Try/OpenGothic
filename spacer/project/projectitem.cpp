#include "projectitem.h"

#include "workers/dataworker.h"

ProjectItem::ProjectItem() {
  }

ProjectItem::ProjectItem(std::shared_ptr<Data> data):data(data) {
  }

ProjectItem::~ProjectItem() {
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
  if(type()==T_World) {
    std::lock_guard<SpinLock> guard(data->sync);
    return data->state!=S_Idle || data->state!=S_Pending;
    }
  return true;
  }

bool ProjectItem::isEmpty() const {
  return data==nullptr;
  }

bool ProjectItem::isPending() const {
  std::lock_guard<SpinLock> guard(data->sync);
  return data!=nullptr && data->state==S_Pending;
  }

bool ProjectItem::isError() const {
  std::lock_guard<SpinLock> guard(data->sync);
  return data!=nullptr && data->state==S_Error;
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
  return data->type;
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

auto ProjectItem::implGet(const WorldEdit*) const -> std::shared_ptr<WorldEdit> {
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

auto ProjectItem::implGet(const zenkit::IItem*) const -> std::shared_ptr<zenkit::IItem> {
  if(data==nullptr)
    return nullptr;
  std::lock_guard<SpinLock> guard(data->sync);
  return data->item;
  }

auto ProjectItem::implGet(const std::string*) const -> std::shared_ptr<std::string> {
  if(data==nullptr)
    return nullptr;
  std::lock_guard<SpinLock> guard(data->sync);
  return data->text;
  }

auto ProjectItem::implGet(const zenkit::LightPreset*) const -> std::shared_ptr<zenkit::LightPreset> {
  if(data==nullptr)
    return nullptr;
  std::lock_guard<SpinLock> guard(data->sync);
  return data->light;
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

void ProjectItem::setPayload(std::shared_ptr<std::string> payload) {
  if(data==nullptr)
    return;
  std::lock_guard<SpinLock> guard(data->sync);
  data->text  = payload;
  data->state = S_Ready;
  }

void ProjectItem::setPayload(std::shared_ptr<WorldEdit> payload) {
  if(data==nullptr)
    return;
  std::lock_guard<SpinLock> guard(data->sync);
  data->world = payload;
  data->state = S_Ready;
  }

void ProjectItem::setPayload(std::shared_ptr<zenkit::LightPreset> payload) {
  if(data==nullptr)
    return;
  std::lock_guard<SpinLock> guard(data->sync);
  data->light = payload;
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

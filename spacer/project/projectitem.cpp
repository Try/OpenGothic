#include "projectitem.h"


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

ProjectItem* ProjectItem::projectFile() const {
  return nullptr;
  }

bool ProjectItem::isVisible() const {
  return true;
  }

ProjectItem::Type ProjectItem::type() const {
  if(data && data->files.size()>0)
    return T_Dir;
  return T_File;
  }

size_t ProjectItem::itemsCount() const {
  return data ? data->files.size() : 0;
  }

ProjectItem ProjectItem::item(size_t i) const {
  return ProjectItem(data->files[i]);
  }

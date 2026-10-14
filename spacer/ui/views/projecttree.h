#pragma once

#include <Tempest/Widget>

class ProjectItem;
class Shelf;

class ProjectTree : public Tempest::Widget {
  public:
    ProjectTree();

    void clear();
    void invalidate();

    Tempest::Signal<void(const ProjectItem&)> onFile;

  private:
    Shelf* shelf = nullptr;
  };


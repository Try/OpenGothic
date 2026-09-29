#pragma once

#include <Tempest/Widget>

class ProjectItem;

class ProjectTree : public Tempest::Widget {
  public:
    ProjectTree();

    Tempest::Signal<void(const ProjectItem&)> onFile;
  };


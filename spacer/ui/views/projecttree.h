#pragma once

#include <Tempest/Widget>

class ProjectTree : public Tempest::Widget {
  public:
    ProjectTree();

    Tempest::Signal<void(size_t)> onFile;
  };


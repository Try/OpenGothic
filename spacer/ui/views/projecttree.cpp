#include "projecttree.h"

#include <Tempest/ListView>
#include <Tempest/Menu>

#include "ui/views/shelf.h"

using namespace Tempest;

ProjectTree::ProjectTree() {
  setSizePolicy(Preferred,Preferred);
  setLayout(Vertical);

  shelf = &addWidget(new Shelf());
  shelf->onItemSelected.bind(&onFile,&Signal<void(const ProjectItem&)>::operator());
  }

void ProjectTree::clear() {
  shelf->clear();
  }

void ProjectTree::invalidate() {
  shelf->invalidate();
  }

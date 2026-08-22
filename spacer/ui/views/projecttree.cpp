#include "projecttree.h"

#include <Tempest/ListView>
#include <Tempest/Menu>

//#include "ui/views/shelf.h"

using namespace Tempest;

ProjectTree::ProjectTree() {
  setSizePolicy(Preferred,Preferred);

  //auto& list = addWidget(new Shelf());
  auto& list = addWidget(new Panel());
  setLayout(Vertical);
  //list.onItemSelected.bind(&onFile,&Signal<void(size_t)>::operator());
  }

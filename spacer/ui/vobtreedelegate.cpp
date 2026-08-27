#include "vobtreedelegate.h"

#include "controls/vobtreeitemview.h"
#include "objects/worldedit.h"

using namespace Tempest;

VobTreeDelegate::VobTreeDelegate(WorldEdit& world)
  :world(world) {
  mkIndex();
  }

size_t VobTreeDelegate::size() const {
  return index.size();
  }

Tempest::Widget* VobTreeDelegate::createView(size_t position) {
  size_t id = index[position].item;
  auto   it = index[position].vob->get();
  auto   b  = new VobTreeItemView(*this, id);

  b->setAsOpen(true);
  b->setText(it!=nullptr ? (*it).vob_name : "LEVEL");
  b->setDepth(index[position].depth);
  b->onClick.bind(this,&VobTreeDelegate::emitClick);

  return b;
  }

void VobTreeDelegate::removeView(Widget* w, size_t) {
  delete w;
  }

void VobTreeDelegate::emitClick(Widget* w, size_t id) {
  auto& it = *index[id].vob;
  if(it.size()>0) {
    toogleFolder(it);
    return;
    }
  onItemSelected(id);
  onItemViewSelected(id,w);
  }

void VobTreeDelegate::mkIndex() {
  index.clear();
  mkIndex(world.root(), index, 0);
  invalidateView();
  }

void VobTreeDelegate::mkIndex(const WorldEdit::Vob& v, std::vector<Item>& index, size_t depth) {
  Item it;
  it.item  = index.size();
  it.depth = depth;
  it.vob   = &v;
  index.push_back(it);

  if(closedDir.find(&v)!=closedDir.end()) {
    return;
    }

  for(size_t i=0; i<v.size(); ++i) {
    mkIndex(v[i], index, depth+1);
    }
  }

void VobTreeDelegate::refreshFileTree() {
  mkIndex();
  }

void VobTreeDelegate::toogleFolder(const WorldEdit::Vob& itm) {
  auto it = closedDir.find(&itm);
  if(it==closedDir.end()) {
    closedDir.insert(&itm);
    } else {
    closedDir.erase(&itm);
    }
  mkIndex();
  }
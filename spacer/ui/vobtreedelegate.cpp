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

  b->setAsOpen(isOpen(index[position].vob));
  b->setText(it!=nullptr ? (*it).vob_name : "LEVEL");
  b->setTextAlt(index[position].textAlt());
  b->setDepth(index[position].depth);
  b->setAsGroup(index[position].vob->size()>0);
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

  if(!isOpen(&v)) {
    return;
    }

  for(size_t i=0; i<v.size(); ++i) {
    mkIndex(v[i], index, depth+1);
    }
  }

bool VobTreeDelegate::isOpen(const WorldEdit::Vob* v) const {
  return closedDir.find(v)==closedDir.end();
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

std::string_view VobTreeDelegate::Item::textAlt() const {
  if(vob->get()==nullptr)
    return "null";
  auto& v = *vob->get();
  if(v.type==zenkit::VirtualObjectType::zCVobLight)
    return "zCVobLight";
  return v.visual_name;
  }

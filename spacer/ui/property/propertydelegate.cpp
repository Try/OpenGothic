#include "propertydelegate.h"

#include "ui/controls/parameterwidget.h"
#include "ui/controls/widgetheader.h"

#include <zenkit/vobs/Light.hh>

using namespace Tempest;

class CmdSetProperty final : public Command::Action<WorldEdit> {
  public:
    CmdSetProperty(WorldEdit::Vob& vob, const VobReflection::Field& field, const Variant& value)
      : vob(vob), field(field), value(value) {}

  private:
    void redo(WorldEdit& subj) override {
      if(field.exchange(*vob.orig, value))
        vob.initView(subj);
      }

    void undo(WorldEdit& subj) override {
      redo(subj);
      }

    bool merge(const Action& prev) override {
      auto* cmd = dynamic_cast<const CmdSetProperty*>(&prev);
      return cmd!=nullptr && &vob==&cmd->vob && &field==&cmd->field;
      }

    WorldEdit::Vob&             vob;
    const VobReflection::Field& field;
    Variant                     value;
  };

struct PropertyDelegate::TypeHeader : WidgetHeader {
  TypeHeader(zenkit::VirtualObjectType c):cat(c){}

  void emitClick() override {
    onChanged(cat);
    }

  Tempest::Signal<void(zenkit::VirtualObjectType)> onChanged;
  zenkit::VirtualObjectType                        cat = {};
  };

PropertyDelegate::PropertyDelegate() {
  }

void PropertyDelegate::setVob(WorldEdit::Vob* inVob) {
  vob = inVob;
  update();
  }

void PropertyDelegate::update() {
  index.clear();
  if(vob!=nullptr && vob->get()!=nullptr) {
    auto type = vob->get()->type;
    if(type==VobReflection::Type::UNKNOWN)
      type = VobReflection::Type::zCVob;
    if(const auto* cls = VobReflection::find(type))
      setupUI(*cls);
    }
  invalidateView();
  }

size_t PropertyDelegate::size() const {
  return index.size();
  }

void PropertyDelegate::onProperty(size_t id, const Variant& v, bool commit) {
  const auto* field = index[id].field;
  if(field==nullptr || !field->exchange || vob==nullptr || vob->get()==nullptr)
    return;
  std::unique_ptr<Command::Action<WorldEdit>> command = std::make_unique<CmdSetProperty>(*vob, *field, v);
  onChanged(command, commit);
  }

void PropertyDelegate::onCategory(zenkit::VirtualObjectType cat) {
  if(isClosed(cat))
    closed.erase(cat); else
    closed.insert(cat);
  update();
  }

Widget* PropertyDelegate::createView(size_t i) {
  const auto& row = index[i];
  if(row.field==nullptr) {
    auto ret = new TypeHeader(row.cls->type);
    ret->setSizePolicy(Preferred,Fixed);
    ret->setText(row.cls->name);
    ret->setClosed(isClosed(row.cls->type));
    ret->onChanged.bind(this, &PropertyDelegate::onCategory);
    return ret;
    }
  auto var = row.field->get ? row.field->get(*vob->get()) : Variant();
  auto ed  = ParameterWidget::createEditor(row.field->slot, var, i);
  ed->onChanged.bind(this, &PropertyDelegate::onProperty);
  return update(ed, i);
  }

Widget* PropertyDelegate::update(Widget* widget, size_t i) {
  const auto* field = index[i].field;
  if(field == nullptr)
    return widget;

  const auto* light = dynamic_cast<const zenkit::VLight*>(vob->get());
  auto* ctrl = dynamic_cast<ParameterWidget*>(widget);
  if(ctrl!=nullptr && light!=nullptr) {
    if(field->slot.type == &Property::Type::ColorList)
      ctrl->setAnimationTiming(light->color_animation_fps, light->color_animation_smooth);
    else if(field->slot.type == &Property::Type::RangeScale)
      ctrl->setAnimationTiming(light->range_animation_fps, light->range_animation_smooth);
    }
  return widget;
  }

void PropertyDelegate::setupUI(const VobReflection::Class& cls) {
  if(const auto* base = cls.baseClass())
    setupUI(*base);
  index.push_back({&cls});
  if(!isClosed(cls.type))
    for(const auto& field : cls.properties())
      index.push_back({&cls, &field});
  }

bool PropertyDelegate::isClosed(zenkit::VirtualObjectType type) const {
  return closed.find(type)!=closed.end();
  }

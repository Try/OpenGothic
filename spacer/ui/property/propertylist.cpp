#include "propertylist.h"

#include <Tempest/Label>

#include "ui/controls/parameterwidget.h"

using namespace Tempest;

PropertyList::Prop::Prop(std::string_view title)
  :title(title) {
  }

PropertyList::Prop::Prop(std::string_view title, const Property::Type& t)
  :title(title), type(T_Typed) {
  ptype = &t;
  }


struct PropertyList::Delegate : ListDelegate {
  Delegate(PropertyList& owner, const std::initializer_list<Prop>& prop):owner(owner),prop(prop) {}
  Delegate(PropertyList& owner, const std::vector<Prop>&           prop):owner(owner),prop(prop) {}

  size_t size() const override { return prop.size(); }

  Widget* createView(size_t i) override {
    Property::Slot slt;
    slt.name = prop[i].title;

    switch(prop[i].type) {
      case T_Label:
        return createLabel(prop[i]);
      default: {
        auto prm = createParameter(prop[i]);
        prm->onChanged.bind(this, &Delegate::emitChange);
        return prm;
        }
      }
    return new Widget();
    }

  Widget* createLabel(const Prop& p){
    Label* b = new Label();
    b->setText(p.title);
    return b;
    }

  ParameterWidget* createParameter(const Prop& p) {
    Property::Slot slt;
    slt.name = p.title;

    switch(p.type) {
      case T_Label:
        return nullptr;
      case T_Typed: {
        slt.type = p.ptype;
        return ParameterWidget::createEditor(slt, Variant(1), 0);
        }
      }
    return nullptr;
    };

  void emitChange(size_t id,const Variant& v,bool commit) {
    owner.onChanged();
    }

  PropertyList&     owner;
  std::vector<Prop> prop;
  };

PropertyList::PropertyList(const std::initializer_list<Prop>& prop) {
  setLayout(Vertical);
  addWidget(&list);

  list.setDelegate(new Delegate(*this,prop));
  }

PropertyList::PropertyList(const std::vector<PropertyList::Prop>& prop) {
  setLayout(Vertical);
  addWidget(&list);

  list.setDelegate(new Delegate(*this,prop));
  }


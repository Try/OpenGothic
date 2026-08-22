#include "parameterwidget.h"

#include "enumwidget.h"
#include "floatwidget.h"
#include "intwidget.h"
#include "vecwidget.h"
#include "colorwidget.h"

#include "ui/uihelper.h"

using namespace Tempest;

template<class Editor>
struct ParameterWidget::Base : ParameterWidget {
  template<class ... Args>
  Base(size_t pos, Args...a):ParameterWidget(), pos(pos) {
    this->setSizePolicy(Preferred,Fixed);
    this->setLayout(Vertical);
    this->setMargins(Margin(0));
    edit = &addWidget(new Editor(a...));
    adjustSize();
    setupCallbacks(edit);
    }

  void setupCallbacks(Widget*) {}
  void setupCallbacks(VecWidget* w) {
    w->onResize.bind(this,&Base::adjustSize);
    }

  void adjustSize() {
    auto sz = UiHelper::wrapContent(*this, Vertical);
    setSizeHint(sz,margins());
    }

  Variant argv() const override {
    return implArgv(edit);
    }

  static Variant implArgv(const Widget*) {
    throw std::logic_error("unreachable");
    }

  static Variant implArgv(const VecWidget* e) {
    return Variant(e->value());
    }

  void setArgv(const Variant& v) override {
    implSetArgv(v,edit);
    }

  static void implSetArgv(const Variant&, Widget*) {
    throw std::logic_error("unreachable");
    }

  static void implSetArgv(const Variant& val, VecWidget* e) {
    if(auto i = val.get<int>())
      e->setValue(Vec4(float(*i),0,0,0));
    else if(auto f = val.get<float>())
      e->setValue(Vec4(*f,0,0,0));
    else if(auto v2 = val.get<Tempest::Vec2>())
      e->setValue(*v2);
    else if(auto v3 = val.get<Tempest::Vec3>())
      e->setValue(*v3);
    else if(auto v4 = val.get<Tempest::Vec4>())
      e->setValue(*v4);
    }

  static float toNumber(const Variant& val) {
    if(auto i = val.get<bool>())
      return (*i) ? 1.f : 0.f;
    if(auto i = val.get<int>())
      return float(*i);
    if(auto f = val.get<float>())
      return *f;
    return 0;
    }

  Editor* edit = nullptr;
  size_t  pos = 0;
  };

struct ParameterWidget::EditEnum    : Base<EnumWidget> {
  EditEnum(size_t pos, std::string_view name):Base(pos,name) {
    this->setMargins(Margin(4,0,0,0));
    edit->onItemSelected.bind(this, &EditEnum::commit);
    }

  void commit(size_t id) {
    Variant v;
    v.set(float(id));
    onChanged(pos,v,true);
    }

  Variant argv() const override {
    return Variant(float(edit->currentIndex()));
    }

  void setArgv(const Variant& v) override {
    edit->setCurrentIndex(size_t(toNumber(v)));
    }
  };

struct ParameterWidget::EditColorWidget : Base<ColorWidget> {
  EditColorWidget(size_t pos, std::string_view name):Base(pos,name) {
    edit->onValueModifyed.bind(this, &EditColorWidget::commit);
    edit->onResize  .bind(this, &EditColorWidget::adjustSize);
    }

  void commit(Vec4,bool) {
    Variant v = argv();
    onChanged(pos,v,true);
    }

  Variant argv() const override {
    Variant v;
    v.set(edit->value());
    return v;
    }

  void setArgv(const Variant& v) override {
    if(auto v3 = v.get<Vec3>()) {
      edit->setValue(*v3);
      }
    }

  void adjustSize() {
    auto sz = UiHelper::wrapContent(*this,Vertical);
    setSizeHint(sz,margins());
    }
  };

struct ParameterWidget::EditInt1    : Base<IntWidget> {
  EditInt1(size_t pos, std::string_view name):Base(pos,name) {
    edit->onValueModifyed.bind(this, &EditInt1::commit);
    }

  void commit(int32_t a,bool commit) {
    Variant v;
    v.set(float(a));
    onChanged(pos,v,commit);
    }

  Variant argv() const override {
    return Variant(float(edit->value()));
    }

  void setArgv(const Variant& v) override {
    edit->setValue(int(toNumber(v)));
    }
  };

struct ParameterWidget::EditFloat1  : Base<FloatWidget> {
  EditFloat1(size_t pos, std::string_view name):Base(pos,name) {
    edit->setUndoRedoEnabled(false);
    edit->onValueModifyed.bind(this, &EditFloat1::commit);
    }

  void commit(float a,bool commit) {
    Variant v;
    v.set(float(a));
    onChanged(pos,v,commit);
    }

  Variant argv() const override {
    return Variant(edit->value());
    }

  void setArgv(const Variant& v) override {
    edit->setValue(toNumber(v));
    }
  };

struct ParameterWidget::EditFloat2  : Base<VecWidget> {
  EditFloat2(size_t pos, std::string_view name):Base(pos,name,2) {
    edit->onValueModifyed.bind(this, &EditFloat2::commit);
    }

  void commit(Vec4 a, bool commit) {
    Variant v;
    v.set(Vec2(a.x,a.y));
    onChanged(pos,v,commit);
    }
  };

struct ParameterWidget::EditFloat3  : Base<VecWidget> {
  EditFloat3(size_t pos, std::string_view name):Base(pos,name,3) {
    edit->onValueModifyed.bind(this, &EditFloat3::commit);
    }

  void commit(Vec4 a, bool commit) {
    Variant v;
    v.set(Vec3(a.x,a.y,a.z));
    onChanged(pos,v,commit);
    }
  };

struct ParameterWidget::EditFloat4  : Base<VecWidget> {
  EditFloat4(size_t pos, std::string_view name):Base(pos,name,4) {
    edit->onValueModifyed.bind(this, &EditFloat4::commit);
    }

  void commit(Vec4 a, bool commit) {
    Variant v;
    v.set(a);
    onChanged(pos,v,commit);
    }
  };


ParameterWidget* ParameterWidget::createEditor(const Property::Slot& s, const Variant& v, size_t id) {
  auto w = implCreateEditor(*s.type,s.name,s.enumValues,{},s.min,s.max,id);
  w->setArgv(v);
  return w;
  }

ParameterWidget* ParameterWidget::implCreateEditor(const Property::Type& type, std::string_view name,
                                                   const std::vector<std::string>& enumValues,
                                                   const std::vector<Tempest::Vec3>& clId,
                                                   const Vec4& min, const Vec4& max,
                                                   size_t pos) {
  if(&type==&Property::Type::Int1) {
    auto ret = new EditInt1(pos,name);
    return ret;
    }
  if(&type==&Property::Type::Enum) {
    auto ret = new EditEnum(pos,name);
    ret->edit->setItems(enumValues);
    return ret;
    }
  if(&type==&Property::Type::Color) {
    // auto ret = new EditTexture(pos,name);
    auto ret = new EditColorWidget(pos,name);
    return ret;
    }
  if(&type==&Property::Type::Bool1) {
    auto ret = new EditEnum(pos,name);
    ret->edit->setItems({"False","True"});
    return ret;
    }

  const auto ctype = type.coreType;
  switch(ctype) {
    case Property::CoreType::Texture:
      return nullptr; //new EditTexture(pos,name); //TODO: edit texture
    case Property::CoreType::Vec1:
      return implVecEditor<EditFloat1,float>(type,name,min,max,pos);
    case Property::CoreType::Vec2:
      return implVecEditor<EditFloat2,Tempest::Vec2>(type,name,min,max,pos);
    case Property::CoreType::Vec3:
      return implVecEditor<EditFloat3,Tempest::Vec3>(type,name,min,max,pos);
    case Property::CoreType::Vec4:
      return implVecEditor<EditFloat4,Tempest::Vec4>(type,name,min,max,pos);
    default:
      return new ParameterWidget();
    }
  return new ParameterWidget();
  }

template<class Edit, class T>
ParameterWidget* ParameterWidget::implVecEditor(const Property::Type& type, std::string_view name,
                                                const Tempest::Vec4& min, const Tempest::Vec4& max, size_t pos) {
  auto ret = new Edit(pos,name);
  if(type.itype==Property::InputType::Int || type.itype==Property::InputType::Round)
    ret->edit->enableFloats(false);
  implSetSliderMinMax(ret,min,max);
  return ret;
  }

void ParameterWidget::implSetSliderMinMax(EditFloat1* e, const Vec4& min, const Vec4& max) {
  e->edit->setSliderMinMax(min.x,max.x);
  }

template<class Edit>
void ParameterWidget::implSetSliderMinMax(Edit* e, const Tempest::Vec4& min, const Tempest::Vec4& max) {
  e->edit->setSliderMinMax(min,max);
  }

Variant ParameterWidget::argv() const {
  return Variant();
  }

void ParameterWidget::setArgv(const Variant&) {
  }

void ParameterWidget::setColorIds(const std::vector<Tempest::Vec3>&) {
  }

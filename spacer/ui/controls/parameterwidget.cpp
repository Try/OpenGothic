#include "parameterwidget.h"

#include <zenkit/Misc.hh>

#include <Tempest/CheckBox>
#include <Tempest/Log>

#include "enumwidget.h"
#include "floatwidget.h"
#include "intwidget.h"
#include "ui/controls/checkwidget.h"
#include "ui/controls/meshwidget.h"
#include "ui/controls/sliderwidget.h"
#include "vecwidget.h"
#include "colorwidget.h"
#include "colorlistwidget.h"
#include "rangescalewidget.h"
#include "stringwidget.h"

#include "ui/uihelper.h"
#include "assets.h"

#include <zenkit/vobs/VirtualObject.hh>

using namespace Tempest;

template<class Editor>
struct ParameterWidget::Base : ParameterWidget {
  template<class ... Args>
  Base(size_t pos, Args...a):ParameterWidget(), pos(pos) {
    auto& ic = Assets::inst().ic.up;
    auto& sp = ic.sprite(w(),h(),Icon::ST_Normal);

    auto& m = style().metrics();
    this->setSizePolicy(Preferred,Fixed);
    this->setSizeHint(Size(m.buttonSize,m.buttonSize));
    this->setLayout(Vertical);
    this->setMargins(Margin(sp.w()+8,0,0,0));
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

  static std::string toString(const Variant& val) {
    if(auto i = val.get<std::string_view>())
      return std::string(*i);
    if(auto i = val.get<std::string>())
      return std::string(*i);
    return "";
    }

  Editor* edit = nullptr;
  size_t  pos = 0;
  };

struct ParameterWidget::EditBool : Base<CheckWidget> {
  EditBool(size_t pos, std::string_view name):Base(pos,name) {
    edit->onValueChanged.bind(this, &EditBool::commit);
    }

  void commit(size_t id) {
    Variant v;
    v.set(int(id));
    onChanged(pos,v,true);
    }

  Variant argv() const override {
    return Variant(int(edit->value() ? 1 : 0));
    }

  void setArgv(const Variant& v) override {
    edit->setValue(size_t(toNumber(v))!=0);
    }
  };

struct ParameterWidget::EditEnum : Base<EnumWidget> {
  EditEnum(size_t pos, std::string_view name):Base(pos,name) {
    edit->onItemSelected.bind(this, &EditEnum::commit);
    }

  void commit(size_t id) {
    Variant v;
    v.set(int(id));
    onChanged(pos,v,true);
    }

  Variant argv() const override {
    return Variant(int(edit->currentIndex()));
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

  void commit(Vec4, bool commit) {
    Variant v = argv();
    onChanged(pos,v,commit);
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
    if(auto cl = v.get<zenkit::Color>()) {
      edit->setValue(*cl);
      }
    }

  void adjustSize() {
    auto sz = UiHelper::wrapContent(*this,Vertical);
    setSizeHint(sz,margins());
    }
  };

template<class Editor, class Value>
struct ParameterWidget::EditAnimationList : Base<Editor> {
  EditAnimationList(size_t pos, std::string_view name):Base<Editor>(pos,name) {
    this->edit->onValueModifyed.bind(this, &EditAnimationList::commit);
    this->edit->onResize.bind(this, &Base<Editor>::adjustSize);
    }

  Variant argv() const override {
    return Variant(this->edit->value());
    }

  void setArgv(const Variant& value) override {
    if(const auto* list = value.get<std::vector<Value>>())
      this->edit->setValue(*list);
    }

  void setAnimationTiming(float fps, bool smooth) override {
    this->edit->setTiming(fps, smooth);
    }

  void commit(bool commit) {
    this->onChanged(this->pos, argv(), commit);
    }
  };

struct ParameterWidget::EditString : Base<StringWidget> {
  EditString(size_t pos, std::string_view name):Base(pos,name) {
    edit->onValueModifyed.bind(this, &EditString::commit);
    }

  void commit(std::string_view a, bool commit) {
    Variant v;
    v.set(std::string(a));
    onChanged(pos,v,commit);
    }

  Variant argv() const override {
    return Variant(std::string(edit->value()));
    }

  void setArgv(const Variant& v) override {
    edit->setValue(toString(v));
    }
  };

struct ParameterWidget::EditMesh : Base<MeshWidget> {
  EditMesh(size_t pos, std::string_view name):Base(pos, name) {
    edit->onValueModifyed.bind(this, &EditMesh::commit);
    }

  void commit(const zenkit::Visual& a, bool commit) {
    Variant v;
    v.set(std::make_shared<zenkit::Visual>(a));
    onChanged(pos,v,commit);
    }

  Variant argv() const override {
    return Variant(std::make_shared<zenkit::Visual>(edit->value()));
    }

  void setArgv(const Variant& v) override {
    if(auto vis = v.get<std::shared_ptr<zenkit::Visual>>())
      edit->setValue(*vis); else
      Log::e("");
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

struct ParameterWidget::EditFloat1  : Base<SliderWidget> {
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
    auto ret = new EditBool(pos,name);
    return ret;
    }
  if(&type==&Property::Type::String) {
    auto ret = new EditString(pos, name);
    return ret;
    }
  if(&type==&Property::Type::Mesh) {
    auto ret = new EditMesh(pos, name);
    return ret;
    }

  const auto ctype = type.coreType;
  switch(ctype) {
    case Property::CoreType::ColorList:
      return new EditAnimationList<ColorListWidget,zenkit::Color>(pos, name);
    case Property::CoreType::RangeScale:
      return new EditAnimationList<RangeScaleWidget,float>(pos, name);
    case Property::CoreType::Texture:
      return new ParameterWidget(); //TODO: edit texture
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

void ParameterWidget::setAnimationTiming(float, bool) {
  }

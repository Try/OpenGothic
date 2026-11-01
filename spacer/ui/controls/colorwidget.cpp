#include "colorwidget.h"

#include "ui/controls/numberedit.h"
#include "ui/uihelper.h"
#include "utility/colormath.h"
#include "vecwidget.h"

#include "resources.h"

#include <Tempest/Painter>

using namespace Tempest;

struct ColorWidget::ClrCycle : Widget {
  void resizeEvent(SizeEvent& e) override {
    recreateImg();
    }

  void recreateImg() {
    if(size().isEmpty())
      return;

    auto& device = Resources::device();
    update();

    auto px = Pixmap(w(),h(),TextureFormat::RGBA8);
    for(size_t y=0; y<h(); ++y)
      for(size_t x=0; x<w(); ++x) {
        auto  p = reinterpret_cast<uint8_t*>(px.data()) + (x+y*w())*4;
        float u = (float(x)/float(w()-1))*2.f - 1.f;
        float v = (float(y)/float(h()-1))*2.f - 1.f;
        if(u*u + v*v > 1.f) {
          p[0] = 0;
          p[1] = 0;
          p[2] = 0;
          p[3] = 0;
          } else {
          float h = (std::atan2(u,v)/M_PI)*0.5+0.5;
          float s = std::sqrt(u*u + v*v);
          double r=0,g=0,b=0;
          ColorMath::hsv2rgb(r,g,b, h,s,1);
          p[0] = r*255;
          p[1] = g*255;
          p[2] = b*255;
          p[3] = 255;
          }
        }

    Resources::recycle(std::move(view));
    view = device.texture(px);
    }

  void paintEvent(PaintEvent& e) override {
    Painter p(e);
    p.setBrush(view);
    p.drawRect(0,0,w(),h());

    float a  = 2.0*M_PI*(hsv.x-0.5);
    float c  = std::cos(a), s = std::sin(a);
    auto  at = Vec2(w()*s,h()*c)*0.5f*hsv.y + Vec2(w(),h())*0.5;
    p.setBrush(Color(0,0,0,1));
    p.drawRect(at.x-1,at.y-1, 3,3);
    p.setBrush(Color(1,1,1,1));
    p.drawRect(at.x,at.y, 1,1);
    }

  void mouseDownEvent(MouseEvent& e) override {
    onSlide(e);
    }

  void mouseDragEvent(MouseEvent& e) override {
    onSlide(e);
    }

  void mouseUpEvent(MouseEvent& e) override {
    onSlide(e);
    }

  void onSlide(MouseEvent& e) {
    float u = (float(e.x)/float(w()-1))*2.f - 1.f;
    float v = (float(e.y)/float(h()-1))*2.f - 1.f;

    float h = (std::atan2(u,v)/M_PI)*0.5+0.5;
    float s = std::sqrt(u*u + v*v);
    double r=0,g=0,b=0;
    Vec4 rgb;
    if(hsv.z<=0.f)
      hsv.z = 1.0;
    ColorMath::hsv2rgb(rgb.x,rgb.y,rgb.z, h,s,hsv.z);

    onValueModifyed(rgb*255.f, e.type()==Event::MouseUp);
    }

  void setRGB(const Vec3& clr) {
    ColorMath::rgb2hsv(clr.x, clr.y, clr.z,
                       hsv.x, hsv.y, hsv.z);
    update();
    }

  Tempest::Signal<void(Tempest::Vec4 v,bool commit)> onValueModifyed;

  Tempest::Texture2d view;
  Tempest::Vec3      hsv;
  };

struct ColorWidget::NumPad : Widget {
  NumPad() {
    red   = &addWidget(new NumberEdit());
    green = &addWidget(new NumberEdit());
    blue  = &addWidget(new NumberEdit());

    for(auto i:{red, green, blue}) {
      // i->setSliderMinMax(0, 255);
      i->enableFloats(false);
      i->onValueModifyed.bind(this, &NumPad::onModifyed);
      }

    auto sz = UiHelper::wrapContent(*this, Vertical);
    setSizeHint(sz, margins());
    setSizePolicy(Preferred,Fixed);
    setLayout(Vertical);
    }

  Vec3 value() const {
    return Vec3(red->value(), green->value(), blue->value());
    }

  void setValue(Vec3 v) {
    red  ->setValue(v.x);
    green->setValue(v.y);
    blue ->setValue(v.z);
    }

  void setValue(Vec4 v) {
    red  ->setValue(v.x);
    green->setValue(v.y);
    blue ->setValue(v.z);
    }

  void onModifyed(double, bool commited){
    auto v = value();
    onValueModifyed(Vec4(v.x, v.y, v.z, 1), commited);
    }

  Tempest::Signal<void(Tempest::Vec4 v,bool commit)> onValueModifyed;

  NumberEdit* red   = nullptr;
  NumberEdit* green = nullptr;
  NumberEdit* blue  = nullptr;
  };

ColorWidget::ColorWidget(std::string_view name) {
  auto& lft = addWidget(new Widget());
  auto& rgh = addWidget(new Widget());

  auto lbl = &lft.addWidget(new Label());
  lft.addWidget(new Widget());
  clr  = &rgh.addWidget(new ClrCycle());
  edit = &rgh.addWidget(new NumPad());

  clr->setSizePolicy(Fixed);

  lft.setLayout(Vertical);
  rgh.setLayout(Horizontal);
  rgh.setMinimumSize(UiHelper::wrapContent(rgh));

  auto sz = UiHelper::wrapContent(*this, Horizontal);
  setSizeHint(sz, margins());
  setSizePolicy(Preferred,Fixed);
  setLayout(Horizontal);

  lbl->setText(name);
  edit->onValueModifyed.bind(this,&ColorWidget::modifyProxy);
  clr ->onValueModifyed.bind(this,&ColorWidget::modifyProxy);
  //edit->onResize.bind(this,&ColorWidget::adjustSize);

  adjustSize();
  }

Vec3 ColorWidget::value() const {
  auto v = edit->value();
  return Vec3(v.x,v.y,v.z);
  }

void ColorWidget::setValue(const Tempest::Vec3& v) {
  edit->setValue(v);
  clr ->setRGB(Vec3(v.x,v.y,v.z)/255.f);
  }

void ColorWidget::setValue(const zenkit::Color& v) {
  setValue(Vec3(v.r, v.g, v.b));
  }

void ColorWidget::adjustSize() {
  auto sz = UiHelper::wrapContent(*edit,Vertical);
  setSizeHint(sz,margins());
  clr->setMinimumSize(sz.h,sz.h);
  onResize();
  }

void ColorWidget::modifyProxy(Vec4 v, bool commit) {
  v.x = std::clamp(v.x, 0.f, 255.f);
  v.y = std::clamp(v.y, 0.f, 255.f);
  v.z = std::clamp(v.z, 0.f, 255.f);

  edit->setValue(v);
  clr ->setRGB(Vec3(v.x,v.y,v.z)/255.f);
  onValueModifyed(v,commit);
  }

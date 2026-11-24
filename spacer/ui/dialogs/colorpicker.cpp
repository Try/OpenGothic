#include "colorpicker.h"

#include <Tempest/Painter>
#include <Tempest/Label>

#include <algorithm>
#include <cmath>

#include "assets.h"
#include "resources.h"
#include "ui/controls/numberedit.h"
#include "utility/colormath.h"

using namespace Tempest;

struct ColorPicker::Wheel : Widget {
  ~Wheel() override {
    Resources::recycle(std::move(view));
    }

  void resizeEvent(SizeEvent&) override {
    recreateImg();
    }

  void recreateImg() {
    if(w()<2 || h()<2)
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

    float a  = 2.0*M_PI*(hue-0.5);
    float c  = std::cos(a), s = std::sin(a);
    auto  at = Vec2((w()-3)*s,(h()-3)*c)*0.5f*saturation + Vec2(w(),h())*0.5;
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
    if(w()<2 || h()<2)
      return;
    const float u = float(e.x)/float(w()-1)*2.f - 1.f;
    const float v = float(e.y)/float(h()-1)*2.f - 1.f;
    hue = (std::atan2(u,v)/M_PI)*0.5+0.5;
    saturation = std::min(1.f, std::hypot(u,v));
    update();
    onChange();
    }

  Signal<void()> onChange;

  Texture2d view;
  float hue        = 0;
  float saturation = 0;
  };

struct ColorPicker::ValueBar : Widget {
  void paintEvent(PaintEvent& e) override {
    Painter p(e);
    for(int y=0; y<h(); ++y) {
      const float gray = 1.f - float(y)/std::max(1,h()-1);
      p.setBrush(Color(gray,gray,gray,1));
      p.drawRect(2,y,std::max(0,w()-4),1);
      }
    const int y = std::clamp(int(std::round((1.f-value)*std::max(0,h()-1))),0,std::max(0,h()-3));
    p.setBrush(Color(0,0,0,1));
    p.drawRect(0,y,w(),3);
    p.setBrush(Color(1,1,1,1));
    p.drawRect(0,y+1,w(),1);
    }

  void mouseDownEvent(MouseEvent& e) override { slide(e); }
  void mouseDragEvent(MouseEvent& e) override { slide(e); }
  void mouseUpEvent(MouseEvent& e) override { slide(e); }

  void slide(MouseEvent& e) {
    value = 1.f - std::clamp(float(e.y)/std::max(1,h()-1),0.f,1.f);
    update();
    onChange();
    }

  Signal<void()> onChange;
  float          value = 0;
  };

struct ColorPicker::ChannelEdit : NumberEdit {
  void keyDownEvent(KeyEvent& e) override {
    NumberEdit::keyDownEvent(e);
    e.accept();
    }
  };

struct ColorPicker::PickerPanel : Widget {
  void paintEvent(PaintEvent& e) override {
    Painter p(e);
    p.setBrush(Assets::inst().colors.workspaceD);
    p.drawRect(0,0,w(),h());
    p.setBrush(Assets::inst().colors.menu);
    p.drawRect(1,1,w()-2,h()-2);
    }

  void mouseMoveEvent(MouseEvent& e) override { e.accept(); }
  void mouseDownEvent(MouseEvent& e) override { e.accept(); }
  };

ColorPicker::ColorPicker(Widget& anchor, Vec3 color, bool preferLeft)
  : anchor(anchor), rgb(color), preferLeft(preferLeft) {
  setStyle(&anchor.style());

  panel = &addWidget(new PickerPanel());
  wheel = &panel->addWidget(new Wheel());
  valueBar = &panel->addWidget(new ValueBar());
  wheel->onChange.bind(this,&ColorPicker::updateRgb);
  valueBar->onChange.bind(this,&ColorPicker::updateRgb);

  auto& row = panel->addWidget(new Widget());
  row.setLayout(Horizontal);
  static constexpr const char* names[] = {"R","G","B"};
  for(size_t i=0; i<3; ++i) {
    auto& channel = row.addWidget(new Widget());
    channel.setLayout(Horizontal);
    auto& label = channel.addWidget(new Label());
    label.setText(names[i]);
    label.setSizePolicy(Fixed,Preferred);
    channels[i] = &channel.addWidget(new ChannelEdit());
    channels[i]->setMinimumSize(32,0);
    channels[i]->setUndoRedoEnabled(false);
    channels[i]->onValueModifyed.bind(this,&ColorPicker::editRgb);
    }
  channels[0]->setValue(rgb.x);
  channels[1]->setValue(rgb.y);
  channels[2]->setValue(rgb.z);
  syncHsv();
  }

void ColorPicker::dismiss() {
  detached = true;
  open     = false;
  }

void ColorPicker::resizeEvent(SizeEvent&) {
  constexpr int pad         = 8;
  constexpr int barWidth    = 19;
  const int     width       = std::min(std::max(218, int(anchor.w()) + 2 * pad), int(w()));
  const int     rowHeight   = channels[0]->sizeHint().h;
  const int     maxDiameter = std::clamp(width - barWidth - 3 * pad, 0, 220);
  const int     diameter    = std::clamp(int(h()) - rowHeight - 3 * pad, 0, maxDiameter);
  const int     height      = std::min(int(h()), diameter + rowHeight + 3 * pad);
  const auto    at          = anchor.mapToRoot(Point());
  const bool    openLeft    = preferLeft && at.x >= width + pad;
  const int     leftEdge    = openLeft ? at.x - width : at.x + anchor.w() - width;
  const int     x           = std::clamp(leftEdge, 0, int(w()) - width);
  const int     below       = at.y + anchor.h();
  const int     top         = openLeft ? at.y : (below + height <= h() ? below : at.y - height);
  const int     y           = std::clamp(top, 0, int(h()) - height);
  panel->setGeometry(x, y, width, height);

  const int left = std::max(0, (width - diameter - barWidth - pad) / 2);
  wheel->setGeometry(left, pad, diameter, diameter);
  valueBar->setGeometry(left + diameter + pad, pad, barWidth, diameter);
  panel->widget(2).setGeometry(pad, diameter + 2 * pad, std::max(0, width - 2 * pad), rowHeight);
  }

void ColorPicker::mouseDownEvent(MouseEvent& e) {
  if(!panel->rect().contains(e.pos()))
    open = false;
  e.accept();
  }

void ColorPicker::keyDownEvent(KeyEvent& e) {
  e.accept();
  }

void ColorPicker::updateRgb() {
  Vec3 next = {};
  ColorMath::hsv2rgb(next.x,next.y,next.z,wheel->hue,wheel->saturation,valueBar->value);
  next = Vec3(std::round(next.x*255.f),std::round(next.y*255.f),std::round(next.z*255.f));
  if(next.x==rgb.x && next.y==rgb.y && next.z==rgb.z)
    return;
  rgb = next;
  channels[0]->setValue(rgb.x);
  channels[1]->setValue(rgb.y);
  channels[2]->setValue(rgb.z);
  onChange(rgb);
  }

void ColorPicker::editRgb(double, bool commit) {
  if(commit)
    return;
  float values[3] = {};
  for(size_t i=0; i<3; ++i) {
    const double v = channels[i]->value();
    values[i] = std::clamp(v,0.0,255.0);
    if(values[i]!=v)
      channels[i]->setValue(values[i]);
    }
  const Vec3 next(values[0],values[1],values[2]);
  if(next.x==rgb.x && next.y==rgb.y && next.z==rgb.z)
    return;
  rgb = next;
  syncHsv();
  onChange(rgb);
  }

void ColorPicker::syncHsv() {
  float hue = 0, saturation = 0, value = 0;
  ColorMath::rgb2hsv(rgb.x/255.f,rgb.y/255.f,rgb.z/255.f,hue,saturation,value);
  if(value>0.f)
    wheel->saturation = saturation;
  if(saturation>0.f)
    wheel->hue = hue;
  valueBar->value = value;
  wheel->update();
  valueBar->update();
  }

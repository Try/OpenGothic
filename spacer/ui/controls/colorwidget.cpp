#include "colorwidget.h"

#include <Tempest/Application>
#include <Tempest/Button>
#include <Tempest/Label>
#include <Tempest/Painter>

#include <zenkit/Misc.hh>

#include <algorithm>
#include <string>

#include "assets.h"
#include "ui/uistyle.h"
#include "ui/dialogs/colorpicker.h"

using namespace Tempest;

struct ColorWidget::Swatch : Button {
  Swatch(const ColorWidget& row, std::string_view text) : row(row), caption(text) {
    setButtonType(T_FlatButton);
    }

  void paintEvent(PaintEvent& e) override {
    constexpr int inset   = 3;
    constexpr int padding = 4;

    Button::paintEvent(e);
    Painter p(e);
    auto&         icons   = Assets::inst().ic;
    const auto    sp      = (row.picker ? icons.up : icons.down).sprite(w(), h(), Icon::ST_Normal);
    const int     x       = std::max(0, w() - sp.w() - padding);
    const int     width   = std::max(0, x - padding - inset);
    const int     split   = row.picker ? width / 2 : width;
    const auto draw = [&](int left, int length, Vec3 rgb) {
      constexpr float channelMax = 255.f;
      p.setBrush(Color(rgb.x/channelMax,rgb.y/channelMax,rgb.z/channelMax,1.f));
      p.drawRect(left,inset,length,std::max(0,h()-2*inset));
      };
    draw(inset,split,row.rgb);
    if(row.picker)
      draw(inset+split,width-split,row.original);

    if(caption.empty()) {
      p.setBrush(sp);
      p.drawRect(x, (h() - sp.h()) / 2, sp.w(), sp.h());
      } else {
      const auto& fnt       = font();
      const auto  text      = fnt.textSize(caption);
      const int   slotWidth = w() - x + padding;
      const int   textX     = x - padding + std::max(0, (slotWidth - text.w) / 2);
      const int   baseline  = (h() - text.h) / 2 + int(fnt.metrics().ascent);
      p.setFont(fnt);
      p.setBrush(Color(1, 1, 1, 1));
      p.drawText(textX, baseline, caption);
      }
    }

  const ColorWidget& row;
  std::string        caption;
  };

ColorWidget::ColorWidget(std::string_view name, std::string_view swatchText) {
  if(!name.empty())
    addWidget(new Label()).setText(name);
  swatch = &addWidget(new Swatch(*this,swatchText));
  swatch->onClick.bind(this,&ColorWidget::openPicker);
  setLayout(Horizontal);
  setSizeHint(0,UiStyle::propertyRowHeight);
  setSizePolicy(Preferred,Fixed);
  }

ColorWidget::~ColorWidget() {
  if(picker!=nullptr)
    picker->dismiss();
  }

Vec3 ColorWidget::value() const {
  return rgb;
  }

void ColorWidget::setValue(const Vec3& v) {
  rgb = v;
  swatch->update();
  }

void ColorWidget::setValue(const zenkit::Color& v) {
  setValue(Vec3(v.r,v.g,v.b));
  }

void ColorWidget::openPicker() {
  original = rgb;
  modified = false;
  {
    ColorPicker overlay(*swatch,rgb,!swatch->caption.empty());
    picker = &overlay;
    overlay.onChange.bind(this,&ColorWidget::previewColor);
    swatch->update();

    SystemApi::addOverlay(&overlay);
    while(overlay.isOpen() && Application::isRunning())
      Application::processEvents();

    if(overlay.dismissed())
      return;

    picker = nullptr;
    }

  swatch->update();
  if(modified)
    onValueModifyed(Vec4(rgb.x,rgb.y,rgb.z,1),true);
  }

void ColorWidget::previewColor(Vec3 v) {
  modified = true;
  setValue(v);
  onValueModifyed(Vec4(v.x,v.y,v.z,1),false);
  }

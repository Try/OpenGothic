#include "vecwidget.h"

#include <Tempest/Painter>
#include <Tempest/Label>
#include <Tempest/Button>

#include "widgetheader.h"
#include "floatwidget.h"
#include "numberedit.h"
#include "assets/assets.h"

using namespace Tempest;

struct VecWidget::Header : WidgetHeader {
  Header() {}
  };

VecWidget::VecWidget(std::string_view name, uint8_t cnt)
  :compCnt(cnt) {
  setLayout(Vertical);
  setSizePolicy(Preferred,Fixed);

  hdr = &addWidget(new Header());
  hdr->setClosed(false);
  hdr->setText(name);

  std::string_view names[] = {"x","y","z","w"};

  for(uint8_t i=0;i<compCnt;++i) {
    edit[i] = &addWidget(new FloatWidget(names[i]));
    //edit[i]->enableFloats(true);
    //edit[i]->setSliderMinMax(0,256);
    edit[i]->onValueModifyed.bind(this,&VecWidget::onModifyed);
    edit[i]->setUndoRedoEnabled(false);
    }

  hdr->onClick.bind(this,&VecWidget::onToogleMinimize);
  onToogleMinimize();
  invalidatePriview();
  }

void VecWidget::setValue(const Vec2& v) {
  setValue(Vec4(v.x,v.y,0,0));
  }

void VecWidget::setValue(const Vec3& v) {
  setValue(Vec4(v.x,v.y,v.z,0));
  }

void VecWidget::setValue(const Vec4& v) {
  val = v;
  float vx[4] = {val.x,val.y,val.z,val.w};

  for(uint8_t i=0;i<4;++i) {
    if(edit[i]!=nullptr)
      edit[i]->setValue(vx[i]);
    }
  invalidatePriview();
  }

void VecWidget::setComponentCount(uint8_t cnt) {
  compCnt = cnt;
  for(uint8_t i=0; i<4; ++i)
    if(edit[i]!=nullptr && i>=compCnt)
      edit[i]->setVisible(false);
  setCompact(hdr->isClose());
  invalidatePriview();
  }

void VecWidget::setNames(std::initializer_list<std::string_view> n) {
  for(uint8_t i=0;i<4;++i) {
    if(edit[i]==nullptr)
      continue;
    edit[i]->setTitle(*(n.begin()+i));
    }
  }

void VecWidget::setSliderMinMax(float min, float max) {
  for(uint8_t i=0;i<4;++i) {
    if(edit[i]!=nullptr) {
      edit[i]->setSliderMinMax(min,max);
      }
    }
  invalidatePriview();
  }

void VecWidget::setSliderMinMax(const Vec4& min, const Vec4& max) {
  float a[4] = {min.x,min.y,min.z,min.w};
  float b[4] = {max.x,max.y,max.z,max.w};

  for(uint8_t i=0;i<4;++i) {
    if(edit[i]!=nullptr) {
      edit[i]->setSliderMinMax(std::min(a[i],b[i]),std::max(a[i],b[i]));
      }
    }
  invalidatePriview();
  }

void VecWidget::enableFloats(bool e) {
  for(uint8_t i=0;i<4;++i) {
    if(edit[i]!=nullptr)
      edit[i]->enableFloats(e);
    }
  invalidatePriview();
  }

void VecWidget::setCompact(bool comp) {
  hdr->setClosed(comp);

  for(uint8_t i=0; i<compCnt; ++i)
    if(edit[i]!=nullptr)
      edit[i]->setVisible(!comp);

  int h = 0, cnt=0;
  for(size_t i=0; i<widgetsCount(); ++i) {
    auto& w = widget(i);
    if(!w.isVisible())
      continue;
    h += w.sizeHint().h;
    if(cnt!=0)
      h+=spacing();
    cnt++;
    }
  setSizeHint(Size(0,h), margins());
  onResize();
  }

Vec4 VecWidget::value() const {
  return val;
  }

void VecWidget::onToogleMinimize() {
  setCompact(hdr->isClose());
  }

void VecWidget::onModifyed(float /*v*/, bool commit) {
  float vx[4] = {val.x,val.y,val.z,val.w};

  for(uint8_t i=0;i<4;++i) {
    if(edit[i]!=nullptr)
      vx[i] = float(edit[i]->value());
    }
  val = Vec4(vx[0],vx[1],vx[2],vx[3]);
  invalidatePriview();
  onValueModifyed(val,commit);
  }

void VecWidget::invalidatePriview() {
  bool showColor = false;
  if(compCnt>=3) {
    showColor = true;
    for(uint8_t i=0;i<compCnt;++i) {
      if(edit[i]==nullptr)
        continue;
      auto rgn = edit[i]->sliderRange();
      if(rgn.x!=0 || rgn.y!=1 || !edit[i]->hasFloats())
        showColor=false;
      }
    }

  hdr->setPriview(val);
  hdr->showPriview(showColor);
  }

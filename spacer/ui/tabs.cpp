#include "tabs.h"

#include <Tempest/Button>
#include <Tempest/Layout>
#include <Tempest/Painter>

#include "ui/editorarea.h"

#include "ui/uihelper.h"
#include "assets/assets.h"

using namespace Tempest;

class Tabs::Tab : public Button {
  public:
    static const int closeBtnSize = 16;
    static const int extraSize    = closeBtnSize+8+8+8;

    Tab(Tabs& e,size_t id):tabs(e),id(id){
      close = &addWidget(UiHelper::toolBtn(Assets::inst().ic.close));
      close->onClick.bind(this,&Tab::onClose);
      setFont(Assets::inst().fntSmall);
      }

    void resizeEvent(SizeEvent& e) override {
      Button::resizeEvent(e);

      const int sz = closeBtnSize;
      close->setGeometry(w()-8-sz,(h()-sz)/2,16,16);
      }

    void emitClick() override {
      tabs.onClicked(id);
      }

    void paintEvent(PaintEvent &e) override {
      Tempest::Painter p(e);
      if(tabs.selection()==id) {
        p.setBrush(Assets::inst().colors.workspace);
        p.drawRect(0,0,w(),h());
        }
      else if(state().moveOver) {
        auto cl = Assets::inst().colors.workspace;
        cl[3] = 0.75f;
        p.setBrush(cl);
        p.drawRect(0,0,w(),h());
        }
      else {
        auto cl = Assets::inst().colors.workspace;
        cl[3] = 0.5f;
        p.setBrush(cl);
        p.drawRect(0,0,w(),h());
        }

      if(tabs.selection()==id) {
        p.setBrush(Assets::inst().colors.highlight);
        p.drawRect(0,0,w(),3);
        }
      auto& t  = text();
      auto  sz = p.font().textSize(t.c_str());

      p.setBrush(Color(1,1,1,1));
      p.setFont(font());
      p.drawText(8, (h()-sz.h)/2+sz.h, t.c_str());
      }

    void onClose() {
      tabs.onClose(id);
      }

    void setState(bool ch) {
      if(ch)
        close->setIcon(Assets::inst().ic.close_save); else
        close->setIcon(Assets::inst().ic.close);
      }

    Tabs&   tabs;
    Button* close = nullptr;
    size_t  id    = 0;
  };

class Tabs::Lay : public Layout {
  void applyLayout() override {
    int x = 0, h = owner()->h();

    for(size_t i=0;i<owner()->widgetsCount();++i){
      auto& w = owner()->widget(i);
      int width = w.sizeHint().w+Tab::extraSize;
      if(width<128)
        width=128;
      w.setGeometry(x,0,width,h);
      x+=width;
      }
    }
  };

Tabs::Tabs(EditorArea& editor)
  :editor(editor) {
  setSizeHint(0,27);
  setSizePolicy(Preferred,Fixed);
  setLayout(new Lay());
  invalidate();
  }

void Tabs::invalidate() {
  // setVisible(editor.editorsCount()>1);
  setVisible(editor.editorsCount()>0);

  while(widgetsCount()<editor.editorsCount())
    addWidget(new Tab(*this,widgetsCount()));

  while(editor.editorsCount()<widgetsCount())
    delete takeWidget(&widget(widgetsCount()-1));

  for(size_t i=0; i<widgetsCount(); ++i) {
    Tab& t = reinterpret_cast<Tab&>(widget(i));
    t.setText (editor.editorTitle(i));
    t.setState(editor.hasUnsavedChanges(i));
    }
  applyLayout();
  }

void Tabs::setSelection(size_t id) {
  sel = id;
  update();
  }

void Tabs::mouseDownEvent(Tempest::MouseEvent& e) {
  e.accept();
  }

void Tabs::paintEvent(PaintEvent& e) {
  Painter p(e);
  p.setBrush(Assets::inst().colors.workspaceD);
  p.drawRect(0,0,w(),h());
  }

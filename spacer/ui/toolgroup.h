#pragma once

#include <Tempest/Panel>
#include <Tempest/Widget>

#include "ui/editors/baseeditor.h"

#include "dragdrop.h"

class ToolGroup : public Tempest::Panel {
  public:
    ToolGroup();

    template<class T>
    T& add(T* w) {
      auto& wx = central->addWidget(w);
      mkTabs();
      setSelection(adjustSelection(central->widgetsCount()-1));
      invalidate();
      return wx;
      }

    void   invalidate();
    void   saveUiLayout(BaseEditor::ToolType parent, size_t group);
    void   moveContent(ToolGroup& src);

  private:
    using  Widget::addWidget;
    void   mkTabs();
    void   setSelection(size_t i);

    size_t adjustSelection(size_t sel);

    struct Tab;
    struct Header;

    Header*  header  = nullptr;
    Widget*  central = nullptr;
    DragDrop dd;
    size_t   sel      = 0;
    size_t   selOrder = 0;
  };


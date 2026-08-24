#include "toolwindow.h"

#include <Tempest/Button>
#include <Tempest/Label>
#include <Tempest/Painter>

using namespace Tempest;

ToolWindow::ToolWindow(Tool t)
  :tId(t) {
  setMargins(Margin(0,0,0,0));
  setLayout(Vertical);
  setName(tId);
  }

void ToolWindow::setName(const char* n) {
  tname = n;
  }

void ToolWindow::setName(ToolWindow::Tool preset) {
  switch(preset) {
    case T_ProjectTree:
      return setName("Project");
    case T_VobTree:
      return setName("Objects");
    case T_VobProp:
      return setName("Properties");
    case T_Count:
      break;
    }
  }

void ToolWindow::invalidate() {
  setVisible(hasContent());
  }

bool ToolWindow::hasContent() {
  bool v = false;
  for(size_t i=0;i<widgetsCount(); ++i)
    v |= widget(i).isVisible();
  return v;
  }

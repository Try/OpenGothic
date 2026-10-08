#include "stacklayout.h"

#include <Tempest/Widget>

StackLayout::StackLayout(Tempest::Widget& fullScreen) : fullScreen(fullScreen) {
  }

void StackLayout::applyLayout() {
  auto& w = *owner();
  size_t count=w.widgetsCount();

  for(size_t i=0;i<count;++i){
    auto& wx=w.widget(i);
    if(&wx==&fullScreen)
      wx.setGeometry(0,0,w.w(),w.h()); else
      wx.setGeometry(w.clientRect());
    }
  }

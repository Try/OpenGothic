#include "stacklayout.h"

#include <Tempest/Widget>

StackLayout::StackLayout() {  
  }

void StackLayout::applyLayout() {
  auto& w = *owner();
  size_t count=w.widgetsCount();

  for(size_t i=0;i<count;++i){
    auto& wx=w.widget(i);
    wx.setGeometry(0,0,w.w(),w.h());
    }
  }

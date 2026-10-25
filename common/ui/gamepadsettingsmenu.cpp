#include "gamepadsettingsmenu.h"

#include <Tempest/Painter>

#include "resources.h"
#include "utils/gthfont.h"

#include <algorithm>
#include <utility>

using namespace Tempest;

GamepadSettingsMenu::GamepadSettingsMenu(GamepadSettings settings, bool russian, float uiScale)
  :input(std::move(settings)), russian(russian), uiScale(uiScale) {
  setFocusPolicy(ClickFocus);
  setFocus(true);
  setCursorShape(CursorShape::Arrow);
  }

Tempest::Size GamepadSettingsMenu::canvasSize() const {
  // UiOverlay follows viewport resizes, but the modal keeps its opening size.
  // Keep all controls inside the currently visible part of that modal.
  const auto* parent = owner();
  return {std::max(1,std::min(w(),parent ? parent->w()-x() : w())),
          std::max(1,std::min(h(),parent ? parent->h()-y() : h()))};
  }

GamepadSettingsMenu::Layout GamepadSettingsMenu::layout() const {
  const auto canvas = canvasSize();
  const float scale = std::max(0.25f,std::min({uiScale,
                                            float(canvas.w)/900.f,float(canvas.h)/300.f}));
  const int padding = std::max(4,int(14.f*scale));
  const int rowHeight = Resources::font(scale).pixelSize()+std::max(3,int(8.f*scale));
  const int width = std::max(1,std::min(canvas.w,int(880.f*scale)));
  const int height = std::max(1,std::min(canvas.h,(GamepadSettingsInput::Rows+3)*rowHeight+2*padding));
  return {(canvas.w-width)/2,(canvas.h-height)/2,width,height,padding,rowHeight,
          std::max(1,(height-2*padding)/rowHeight-2),scale};
  }

void GamepadSettingsMenu::reveal(const Layout& l) {
  if(input.selected<firstRow)
    firstRow = input.selected;
  if(input.selected>=firstRow+l.visibleRows)
    firstRow = input.selected-l.visibleRows+1;
  firstRow = std::clamp(firstRow,0,std::max(0,GamepadSettingsInput::Rows-l.visibleRows));
  }

void GamepadSettingsMenu::onGamepad(KeyCodec::Action action) {
  if(!isOpen() || input.finished)
    return;
  input.process(action);
  if(input.finished) {
    close();
    return;
    }
  update();
  }

void GamepadSettingsMenu::keyDownEvent(KeyEvent& event) {
  event.accept();
  if(!isOpen() || input.finished)
    return;
  switch(event.key) {
    case Event::K_Up:     onGamepad(KeyCodec::Forward);       break;
    case Event::K_Down:   onGamepad(KeyCodec::Back);          break;
    case Event::K_Left:   onGamepad(KeyCodec::Left);          break;
    case Event::K_Right:  onGamepad(KeyCodec::Right);         break;
    case Event::K_Return: onGamepad(KeyCodec::ActionGeneric); break;
    case Event::K_ESCAPE: onGamepad(KeyCodec::Escape);        break;
    default: break;
    }
  }

void GamepadSettingsMenu::keyRepeatEvent(KeyEvent& event) {
  event.accept();
  if(!isOpen() || input.finished)
    return;
  if(event.key==Event::K_Up || event.key==Event::K_Down ||
     event.key==Event::K_Left || event.key==Event::K_Right)
    keyDownEvent(event);
  }

void GamepadSettingsMenu::keyUpEvent(KeyEvent& event) {
  event.accept();
  if(!isOpen() || input.finished)
    return;
  }

void GamepadSettingsMenu::mouseDownEvent(MouseEvent& event) {
  event.accept();
  if(!isOpen() || input.finished || event.mouseID!=0)
    return;
  if(event.button==Event::ButtonRight) {
    onGamepad(KeyCodec::Escape);
    return;
    }
  if(event.button!=Event::ButtonLeft)
    return;
  const auto l = layout();
  reveal(l);
  const int top = l.y+l.padding+l.rowHeight;
  if(event.x<l.x || event.x>=l.x+l.width || event.y<top ||
     event.y>=top+l.visibleRows*l.rowHeight)
    return;
  const int row = firstRow+(event.y-top)/l.rowHeight;
  if(row>=GamepadSettingsInput::Rows)
    return;
  input.select(row);
  if(row>=GamepadSettingsInput::Apply)
    onGamepad(KeyCodec::ActionGeneric);
  else
    onGamepad(event.x<l.x+l.width/2 ? KeyCodec::Left : KeyCodec::Right);
  }

void GamepadSettingsMenu::mouseWheelEvent(MouseEvent& event) {
  event.accept();
  if(!isOpen() || input.finished || event.delta==0)
    return;
  onGamepad(event.delta>0 ? KeyCodec::Forward : KeyCodec::Back);
  }

std::string GamepadSettingsMenu::rowText(int row) const {
  static const char* en[] = {"Movement dead zone", "Camera dead zone", "Camera X sensitivity",
                            "Camera Y sensitivity", "Invert camera Y", "Button hints"};
  static const char* ru[] = {"\xcc\xe5\xf0\xf2\xe2\xe0\xff \xe7\xee\xed\xe0 \xe4\xe2\xe8\xe6\xe5\xed\xe8\xff", "\xcc\xe5\xf0\xf2\xe2\xe0\xff \xe7\xee\xed\xe0 \xea\xe0\xec\xe5\xf0\xfb", "\xd7\xf3\xe2\xf1\xf2\xe2\xe8\xf2\xe5\xeb\xfc\xed\xee\xf1\xf2\xfc \xea\xe0\xec\xe5\xf0\xfb X",
                            "\xd7\xf3\xe2\xf1\xf2\xe2\xe8\xf2\xe5\xeb\xfc\xed\xee\xf1\xf2\xfc \xea\xe0\xec\xe5\xf0\xfb Y", "\xc8\xed\xe2\xe5\xf0\xf1\xe8\xff \xea\xe0\xec\xe5\xf0\xfb Y", "\xcf\xee\xe4\xf1\xea\xe0\xe7\xea\xe8 \xea\xed\xee\xef\xee\xea"};
  if(row<GamepadSettingsInput::FirstBinding) {
    std::string text = russian ? ru[row] : en[row];
    text += ":  < ";
    if(row<4) {
      const int values[] = {input.draft.moveDeadZone,input.draft.lookDeadZone,
                            input.draft.sensitivityX,input.draft.sensitivityY};
      text += std::to_string(values[row])+"%";
      }
    else {
      const bool enabled = row==4 ? input.draft.invertY : input.draft.hints;
      text += enabled ? (russian ? "\xc4\xe0" : "On") : (russian ? "\xcd\xe5\xf2" : "Off");
      }
    return text+" >";
    }
  if(row<GamepadSettingsInput::Apply) {
    const auto button = GamepadSettings::remappableButtons[size_t(row-GamepadSettingsInput::FirstBinding)];
    return std::string(GamepadSettings::buttonName(button))+":  < "+
           std::string(GamepadSettings::actionName(input.draft.bindings[size_t(button)]))+" >";
    }
  if(row==GamepadSettingsInput::Apply)
    return russian ? "\xcf\xf0\xe8\xec\xe5\xed\xe8\xf2\xfc" : "Apply";
  if(row==GamepadSettingsInput::Reset)
    return russian ? "\xd1\xe1\xf0\xee\xf1\xe8\xf2\xfc \xed\xe0\xf1\xf2\xf0\xee\xe9\xea\xe8" : "Reset defaults";
  return russian ? "\xce\xf2\xec\xe5\xed\xe0" : "Cancel";
  }

void GamepadSettingsMenu::paintEvent(PaintEvent& event) {
  Painter p(event);
  const auto l = layout();
  reveal(l);
  p.setScissor(l.x,l.y,l.width,l.height);
  p.setBrush(Color(0.025f,0.025f,0.025f,1.f));
  p.drawRect(l.x,l.y,l.width,l.height);
  auto font = Resources::font(l.scale);
  const int textX = l.x+l.padding;
  int y = l.y+l.padding;
  const auto title = russian ? "\xcd\xe0\xf1\xf2\xf0\xee\xe9\xea\xe8 \xea\xee\xed\xf2\xf0\xee\xeb\xeb\xe5\xf0\xe0" : "Controller settings";
  font.drawText(p,textX,y+font.pixelSize(),l.width-2*l.padding,l.rowHeight,title,AlignLeft);
  y += l.rowHeight;
  for(int row=firstRow; row<std::min(GamepadSettingsInput::Rows,firstRow+l.visibleRows); ++row) {
    if(row==input.selected) {
      p.setBrush(Color(0.15f,0.18f,0.22f,1.f));
      p.drawRect(l.x,y,l.width,l.rowHeight);
      }
    const auto text = (row==input.selected ? "> " : "  ")+rowText(row);
    font.drawText(p,textX,y+font.pixelSize(),l.width-2*l.padding,l.rowHeight,text,AlignLeft);
    y += l.rowHeight;
    }
  const auto footer = russian ? "\xd1\xf2\xf0\xe5\xeb\xea\xe8: \xe2\xfb\xe1\xee\xf0/\xe8\xe7\xec\xe5\xed\xe5\xed\xe8\xe5  Cross/Enter: OK  Circle/Esc: \xee\xf2\xec\xe5\xed\xe0" :
                               "Arrows: select/change  Cross/Enter: OK  Circle/Esc: cancel";
  font.drawText(p,textX,l.y+l.height-l.padding-l.rowHeight+font.pixelSize(),
                l.width-2*l.padding,l.rowHeight,footer,AlignLeft);
  }

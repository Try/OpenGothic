#include "gamemenu.h"

#include <Tempest/Painter>

#include "mainwindow.h"
#include "gothic.h"
#include "resources.h"

#include <world/world.h>

using namespace Tempest;

static const float scriptDiv=8192.0f;

GameMenu::GameMenu(MainWindow &owner, Gothic &gothic, const char* menuSection)
  :gothic(gothic), owner(owner) {
  timer.timeout.bind(this,&GameMenu::onTimer);
  timer.start(100);

  textBuf.reserve(64);
  vm = gothic.createVm("_work/Data/Scripts/_compiled/MENU.DAT");

  Daedalus::DATFile& dat=vm->getDATFile();

  hMenu = vm->getGameState().createMenu();
  vm->initializeInstance(ZMemory::toBigHandle(hMenu),
                         dat.getSymbolIndexByName(menuSection),
                         Daedalus::IC_Menu);

  Daedalus::GEngineClasses::C_Menu& menu = vm->getGameState().getMenu(hMenu);

  back = Resources::loadTexture(menu.backPic);
  //back = Resources::loadTexture("menu_gothicshadow.tga");
  //back = Resources::loadTexture("menu_gothic.tga");

  initItems();
  if(menu.flags & Daedalus::GEngineClasses::C_Menu::MENU_SHOW_INFO) {
    float infoX = 1000.0f/scriptDiv;
    float infoY = 7500.0f/scriptDiv;

    // There could be script-defined values
    if(dat.hasSymbolName("MENU_INFO_X") && dat.hasSymbolName("MENU_INFO_X")) {
      Daedalus::PARSymbol& symX = dat.getSymbolByName("MENU_INFO_X");
      Daedalus::PARSymbol& symY = dat.getSymbolByName("MENU_INFO_Y");

      infoX = symX.getInt()/scriptDiv;
      infoY = symY.getInt()/scriptDiv;
      }
    setPosition(int(infoX*w()),int(infoY*h()));
    }

  setSelection(0);
  }

GameMenu::~GameMenu() {
  vm->getGameState().removeMenu(hMenu);
  }

void GameMenu::initItems() {
  Daedalus::GEngineClasses::C_Menu& menu = vm->getGameState().getMenu(hMenu);

  for(int i=0;i<Daedalus::GEngineClasses::MenuConstants::MAX_ITEMS;++i){
    if(menu.items[i].empty())
      continue;

    //LogInfo() << "Initializing item: " << menu.items[i];

    hItems[i].handle = vm->getGameState().createMenuItem();
    vm->initializeInstance(ZMemory::toBigHandle(hItems[i].handle),
                           vm->getDATFile().getSymbolIndexByName(menu.items[i]),
                           Daedalus::IC_MenuItem);
    auto& item=hItems[i].get(*vm);
    hItems[i].img = Resources::loadTexture(item.backPic);
    }
  }

void GameMenu::paintEvent(PaintEvent &e) {
  Painter p(e);
  p.setBrush(back);

  p.drawRect(0,0,w(),h(),
             0,0,back.w(),back.h());

  for(auto& hItem:hItems){
    if(!hItem.handle.isValid())
      continue;
    Daedalus::GEngineClasses::C_Menu_Item& item = hItem.get(*vm);
    getText(item,textBuf);

    Color clText = clNormal;
    if(item.fontName=="font_old_10_white.tga"){
      p.setFont(Resources::font());
      clText = clNormal;
      }
    else if(item.fontName=="font_old_10_white_hi.tga"){
      p.setFont(Resources::font());
      clText = clWhite;
      }
    else if(item.fontName=="font_old_20_white.tga"){
      p.setFont(Resources::menuFont());
      } else {
      p.setFont(Resources::menuFont());
      }

    int x = int(w()*item.posx/scriptDiv);
    int y = int(h()*item.posy/scriptDiv);

    if(!hItem.img.isEmpty()) {
      int32_t dimx = 8192;
      int32_t dimy = 750;

      if(item.dimx!=-1) dimx = item.dimx;
      if(item.dimy!=-1) dimy = item.dimy;

      const int szX = int(w()*dimx/scriptDiv);
      const int szY = int(h()*dimy/scriptDiv);
      p.setBrush(hItem.img);
      p.drawRect((w()-szX)/2,y,szX,szY,
                 0,0,hItem.img.w(),hItem.img.h());
      }

    if(&hItem==selectedItem())
      p.setBrush(clSelected); else
      p.setBrush(clText);

    if(item.flags & Daedalus::GEngineClasses::C_Menu_Item::IT_TXT_CENTER){
      Size sz = p.font().textSize(textBuf.data());
      x = (w()-sz.w)/2;
      //y += sz.h/2;
      }

    p.drawText(x,y+int(p.font().pixelSize()/2),textBuf.data());
    }

  if(auto sel=selectedItem()){
    p.setFont(Resources::font());
    Daedalus::GEngineClasses::C_Menu_Item& item = sel->get(*vm);
    if(item.text->size()>1) {
      const char* txt = item.text[1].c_str();
      int tw = p.font().textSize(txt).w;

      p.setBrush(clSelected);
      p.drawText((w()-tw)/2,h()-12,txt);
      }
    }
  }

void GameMenu::resizeEvent(SizeEvent &) {
  onTimer();
  }

void GameMenu::mouseDownEvent(MouseEvent &event){
  event.accept();
  }

void GameMenu::mouseUpEvent(MouseEvent&) {
  using namespace Daedalus::GEngineClasses::MenuConstants;

  if(auto sel=selectedItem()){
    Daedalus::GEngineClasses::C_Menu_Item& item = sel->get(*vm);

    for(auto action:item.onSelAction)
      switch(action) {
        case SEL_ACTION_BACK:
          owner.popMenu();
          return;
          break;
        case SEL_ACTION_CLOSE:
          //TODO
          break;
        }

    for(auto& str:item.onSelAction_S)
      if(!str.empty())
        exec(str);
    }
  }

void GameMenu::mouseWheelEvent(MouseEvent &event) {
  int dx = (std::abs(event.delta))/120;
  if(event.delta<0)
    setSelection(int(curItem)+dx,1);
  else if(event.delta>0)
    setSelection(int(curItem)-dx,-1);
  update();
  }

void GameMenu::onTimer() {
  update();

  const float fx = 640.0f;
  const float fy = 480.0f;

  Daedalus::GEngineClasses::C_Menu& menu = vm->getGameState().getMenu(hMenu);

  if(menu.flags & Daedalus::GEngineClasses::C_Menu::MENU_DONTSCALE_DIM)
    resize(int(menu.dimx/scriptDiv*fx),int(menu.dimy/scriptDiv*fy));

  if(menu.flags & Daedalus::GEngineClasses::C_Menu::MENU_ALIGN_CENTER) {
    setPosition((owner.w()-w())/2, (owner.h()-h())/2);
    }
  else if(menu.flags & Daedalus::GEngineClasses::C_Menu::MENU_DONTSCALE_DIM)
    setPosition(int(menu.posx/scriptDiv*fx), int(menu.posy/scriptDiv*fy));
  }

GameMenu::Item *GameMenu::selectedItem() {
  if(curItem<Daedalus::GEngineClasses::MenuConstants::MAX_ITEMS)
    if(hItems[curItem].handle.isValid())
      return &hItems[curItem];
  return nullptr;
  }

void GameMenu::setSelection(int desired,int seek) {
  uint32_t cur=uint32_t(desired);
  for(int i=0;i<Daedalus::GEngineClasses::MenuConstants::MAX_ITEMS;++i,cur+=uint32_t(seek)) {
    cur%=Daedalus::GEngineClasses::MenuConstants::MAX_ITEMS;

    if(hItems[cur].handle.isValid()) {
      auto& it=hItems[cur].get(*vm);
      if(it.flags& Daedalus::GEngineClasses::C_Menu_Item::EFlags::IT_SELECTABLE){
        curItem=cur;
        return;
        }
      }
    }
  }

Daedalus::GEngineClasses::C_Menu_Item &GameMenu::Item::get(Daedalus::DaedalusVM &vm) {
  return vm.getGameState().getMenuItem(handle);
  }

void GameMenu::getText(Daedalus::GEngineClasses::C_Menu_Item& it, std::vector<char> &out) {
  if(out.size()==0)
    out.resize(1);
  out[0]='\0';

  const std::string& src = it.text[0];
  if(it.type==Daedalus::GEngineClasses::C_Menu_Item::MENU_ITEM_TEXT) {
    out.resize(src.size()+1);
    std::memcpy(&out[0],&src[0],src.size());
    out[src.size()] = '\0';
    return;
    }

  if(it.type==Daedalus::GEngineClasses::C_Menu_Item::MENU_ITEM_CHOICEBOX) {
    size_t pos = src.find("#");
    if(pos==std::string::npos)
      pos = src.find("|");
    if(pos==std::string::npos)
      pos = src.size();
    out.resize(pos+1);
    std::memcpy(&out[0],&src[0],pos);
    out[pos] = '\0';
    return;
    }
  }

void GameMenu::exec(const std::string &action) {
  if(action=="NEW_GAME"){
    World w(gothic,gothic.defaultWorld());
    gothic.setWorld(std::move(w));
    }

  static const char* menuList[]={
    "MENU_OPTIONS",
    "MENU_SAVEGAME_LOAD",
    "MENU_SAVEGAME_SAVE",
    "MENU_STATUS",
    "MENU_LEAVE_GAME",
    "MENU_OPT_GAME",
    "MENU_OPT_GRAPHICS",
    "MENU_OPT_VIDEO",
    "MENU_OPT_AUDIO",
    "MENU_OPT_CONTROLS",
    "MENU_OPT_CONTROLS_EXTKEYS",
    "MENU_OPT_EXT"
    };

  for(auto subMenu:menuList)
    if(action==subMenu) {
      owner.pushMenu(new GameMenu(owner,gothic,subMenu));
      return;
      }
  }

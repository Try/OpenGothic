#include "mainmenu.h"

#include <Tempest/Painter>

#include "gothic.h"
#include "resources.h"

using namespace Tempest;

static const float scriptDiv=8192.0f;

MainMenu::MainMenu(Gothic &gothic)
  :gothic(gothic) {
  timer.timeout.bind(this,&MainMenu::onTimer);
  timer.start(100);

  vm = gothic.createVm("_work/data/Scripts/_compiled/MENU.DAT");

  Daedalus::DATFile& dat=vm->getDATFile();

  hMenu = vm->getGameState().createMenu();
  vm->initializeInstance(ZMemory::toBigHandle(hMenu),
                         dat.getSymbolIndexByName("MENU_MAIN"),
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

MainMenu::~MainMenu() {
  vm->getGameState().removeMenu(hMenu);
  }

void MainMenu::initItems() {
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

void MainMenu::paintEvent(PaintEvent &e) {
  Painter p(e);
  p.setBrush(back);

  p.drawRect(0,0,w(),h(),
             0,0,back.w(),back.h());

  p.setFont(Resources::menuFont());

  for(auto& hItem:hItems){
    if(!hItem.handle.isValid())
      continue;
    Daedalus::GEngineClasses::C_Menu_Item& item = hItem.get(*vm);

    int x = int(w()*item.posx/scriptDiv);
    int y = int(h()*item.posy/scriptDiv);

    if(item.flags & Daedalus::GEngineClasses::C_Menu_Item::IT_TXT_CENTER){
      int tw = item.text->size()>0 ? p.font().textSize(item.text[0].c_str()).w : 0;
      x = (w()-tw-hItem.img.w())/2;
      }
    if(&hItem==selectedItem())
      p.setBrush(clSelected); else
      p.setBrush(clNormal);

    if(item.text->size()>0)
      p.drawText(x,y+int(p.font().pixelSize()/2),item.text[0].c_str());

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
    }

  if(auto sel=selectedItem()){
    p.setFont(Resources::font());
    Daedalus::GEngineClasses::C_Menu_Item& item = sel->get(*vm);
    if(item.text->size()>1) {
      const char* txt = item.text[1].c_str();
      int tw = p.font().textSize(txt).w;

      p.setBrush(clSelected);
      p.drawText((w()-tw)/2,h()-20,txt);
      }
    }
  }

void MainMenu::resizeEvent(SizeEvent &) {
  onTimer();
  }

void MainMenu::mouseDownEvent(MouseEvent &event){
  event.accept();
  }

void MainMenu::mouseUpEvent(MouseEvent &event) {

  }

void MainMenu::mouseWheelEvent(MouseEvent &event) {
  int dx = (std::abs(event.delta))/120;
  if(event.delta<0)
    setSelection(int(curItem)+dx,1);
  else if(event.delta>0)
    setSelection(int(curItem)-dx,-1);
  update();
  }

void MainMenu::onTimer() {
  update();

  const float fx = 640.0f;
  const float fy = 480.0f;

  Daedalus::GEngineClasses::C_Menu& menu = vm->getGameState().getMenu(hMenu);

  if(menu.flags & Daedalus::GEngineClasses::C_Menu::MENU_DONTSCALE_DIM)
    resize(int(menu.dimx/scriptDiv*fx),int(menu.dimy/scriptDiv*fy));

  if(menu.flags & Daedalus::GEngineClasses::C_Menu::MENU_ALIGN_CENTER) {
    setPosition((owner()->w()-w())/2, (owner()->h()-h())/2);
    }
  else if(menu.flags & Daedalus::GEngineClasses::C_Menu::MENU_DONTSCALE_DIM)
    setPosition(int(menu.posx/scriptDiv*fx), int(menu.posy/scriptDiv*fy));
  }

MainMenu::Item *MainMenu::selectedItem() {
  if(curItem<Daedalus::GEngineClasses::MenuConstants::MAX_ITEMS)
    if(hItems[curItem].handle.isValid())
      return &hItems[curItem];
  return nullptr;
  }

void MainMenu::setSelection(int desired,int seek) {
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

Daedalus::GEngineClasses::C_Menu_Item &MainMenu::Item::get(Daedalus::DaedalusVM &vm) {
  return vm.getGameState().getMenuItem(handle);
  }

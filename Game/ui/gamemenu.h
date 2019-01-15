#pragma once

#include <Tempest/Widget>
#include <Tempest/Texture2d>
#include <Tempest/Timer>
#include <Tempest/Color>

#include <daedalus/DaedalusVM.h>
#include <memory>

class Gothic;
class MenuRoot;

class GameMenu : public Tempest::Widget {
  public:
    GameMenu(MenuRoot& owner, Gothic& gothic, const char *menuSection);
    ~GameMenu() override;

    void onMove(int dy);
    void onSelect();
    void onTick();

  protected:
    void paintEvent(Tempest::PaintEvent& event) override;
    void resizeEvent(Tempest::SizeEvent& event) override;

  private:
    Gothic&                               gothic;
    MenuRoot&                             owner;
    std::unique_ptr<Daedalus::DaedalusVM> vm;

    Daedalus::GameState::MenuHandle       hMenu;
    Tempest::Texture2d*                   back=nullptr;

    Tempest::Color                        clNormal  ={1.f,0.87f,0.67f,1.f};
    Tempest::Color                        clDisabled={1.f,0.87f,0.67f,0.6f};
    Tempest::Color                        clSelected={1.f,1.f,1.f,1.f};
    Tempest::Color                        clWhite   ={1.f,1.f,1.f,1.f};
    std::vector<char>                     textBuf;

    Tempest::Timer                        timer;

    struct Item {
      Daedalus::GameState::MenuItemHandle handle={};
      Tempest::Texture2d*                 img=nullptr;

      Daedalus::GEngineClasses::C_Menu_Item& get(Daedalus::DaedalusVM& vm);
      };
    Item                                  hItems[Daedalus::GEngineClasses::MenuConstants::MAX_ITEMS];
    uint32_t                              curItem=0;

    Item*                                 selectedItem();
    void                                  setSelection(int cur, int seek=1);
    void                                  initItems();
    void                                  getText(const Daedalus::GEngineClasses::C_Menu_Item&,std::vector<char>& out);
    bool                                  isEnabled(const Daedalus::GEngineClasses::C_Menu_Item& item);

    void                                  exec(const Daedalus::GEngineClasses::C_Menu_Item& item);
    bool exec(const std::string& action);
  };

#pragma once

#include <Tempest/Widget>
#include <Tempest/Texture2d>
#include <Tempest/Timer>
#include <Tempest/Color>

#include <daedalus/DaedalusVM.h>
#include <memory>

class Gothic;

class MainMenu : public Tempest::Widget {
  public:
    MainMenu(Gothic& gothic);
    ~MainMenu() override;

  protected:
    void paintEvent(Tempest::PaintEvent& event) override;
    void resizeEvent(Tempest::SizeEvent& event) override;

    void mouseDownEvent (Tempest::MouseEvent& event) override;
    void mouseUpEvent   (Tempest::MouseEvent& event) override;
    void mouseWheelEvent(Tempest::MouseEvent& event) override;
    //void keyDownEvent();

    void onTimer();

  private:
    Gothic&                               gothic;
    std::unique_ptr<Daedalus::DaedalusVM> vm;

    Daedalus::GameState::MenuHandle       hMenu;
    Tempest::Texture2d                    back;

    Tempest::Color                        clNormal  ={1.f,0.87f,0.67f,1.f};
    Tempest::Color                        clSelected={1.f,1.f,1.f,1.f};

    Tempest::Timer                        timer;

    struct Item {
      Daedalus::GameState::MenuItemHandle handle={};
      Tempest::Texture2d                  img;

      Daedalus::GEngineClasses::C_Menu_Item& get(Daedalus::DaedalusVM& vm);
      };
    Item                                  hItems[Daedalus::GEngineClasses::MenuConstants::MAX_ITEMS];
    uint32_t                              curItem=0;

    Item*                                 selectedItem();
    void                                  setSelection(int cur, int seek=1);
    void                                  initItems();
  };

#pragma once

#include <Tempest/Font>
#include <Tempest/Texture2d>
#include <Tempest/Timer>
#include <Tempest/Widget>

class DialogMenu : public Tempest::Widget {
  public:
    DialogMenu();

    void tick(uint64_t dt);
    void aiOutput(const char* msg);
    void aiClose();

    void printScreen(const char* msg,int x,int y,int time,const Tempest::Font& font);

  protected:
    void paintEvent(Tempest::PaintEvent& e);

  private:
    const Tempest::Texture2d* tex=nullptr;
    enum Flags:uint8_t {
      NoFlags =0,
      DlgClose
      };

    struct Entry {
      std::string txt;
      uint32_t    time=0;
      Flags       flag=NoFlags;
      };

    struct PScreen {
      std::string   txt;
      Tempest::Font font;
      uint32_t      time=0;
      int           x=-1;
      int           y=-1;
      };

    void onEntry(const Entry& e);

    std::vector<Entry>   txt;
    std::vector<PScreen> pscreen;
    uint64_t             remDlg=0;
  };

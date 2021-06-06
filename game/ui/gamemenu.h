#pragma once

#include <Tempest/Widget>
#include <Tempest/Texture2d>
#include <Tempest/Timer>
#include <Tempest/Color>

#include <daedalus/DaedalusVM.h>
#include <memory>

#include "game/savegameheader.h"
#include "game/questlog.h"
#include "utils/keycodec.h"

class Gothic;
class MenuRoot;
class Npc;
class GthFont;

class GameMenu : public Tempest::Widget {
  public:
    GameMenu(MenuRoot& owner, KeyCodec& keyCodec, Daedalus::DaedalusVM& vm, const char *menuSection, KeyCodec::Action keyClose);
    ~GameMenu() override;

    void setPlayer(const Npc& pl);

    void onMove (int dy);
    void onSlide(int dx);
    void onSelect();
    void onTick();


    KeyCodec::Action keyClose() const { return kClose; }

  protected:
    void paintEvent (Tempest::PaintEvent& event) override;
    void resizeEvent(Tempest::SizeEvent&  event) override;

  private:
    struct KeyEditDialog;
    struct SavNameDialog;
    struct Item {
      std::string                           name;
      Daedalus::GEngineClasses::C_Menu_Item handle={};
      const Tempest::Texture2d*             img=nullptr;
      SaveGameHeader                        savHdr;
      bool                                  visible=true;
      int                                   value=0;
      };

    MenuRoot&                             owner;
    KeyCodec&                             keyCodec;
    Daedalus::DaedalusVM&                 vm;
    Tempest::Timer                        timer;

    Daedalus::GEngineClasses::C_Menu      menu={};
    const Tempest::Texture2d*             back=nullptr;
    const Tempest::Texture2d*             slider=nullptr;
    Tempest::Texture2d                    savThumb;
    std::vector<char>                     textBuf;

    Item                                  hItems[Daedalus::GEngineClasses::MenuConstants::MAX_ITEMS];
    Item*                                 ctrlInput = nullptr;
    uint32_t                              curItem=0;
    bool                                  exitFlag=false;
    bool                                  closeFlag=false;

    KeyCodec::Action                      kClose = KeyCodec::Escape;

    void                                  drawItem(Tempest::Painter& p, Item& it);
    void                                  drawSlider(Tempest::Painter& p, Item& it, int x, int y, int w, int h);
    void                                  drawQuestList(Tempest::Painter& p, int x, int y, int w, int h, const GthFont& fnt, const QuestLog& log, QuestLog::Status st, bool isNote);

    Item*                                 selectedItem();
    Item*                                 selectedNextItem(Item* cur);
    void                                  setSelection(int cur, int seek=1);
    void                                  initItems();
    void                                  getText(const Item &it, std::vector<char>& out);
    const GthFont&                        getTextFont(const Item &it);
    bool                                  isEnabled(const Daedalus::GEngineClasses::C_Menu_Item& item);

    void                                  exec         (Item &item, int slideDx);
    void                                  execSingle   (Item &it,   int slideDx);
    void                                  execChgOption(Item &item, int slideDx);
    void                                  execSaveGame (Item &item);
    void                                  execLoadGame (Item &item);
    void                                  execCommands (Item &it, const Daedalus::ZString str);

    bool                                  implUpdateSavThumb(Item& sel);
    size_t                                saveSlotId(const Item& sel);

    const char*                           strEnum(const char* en, int id, std::vector<char> &out);
    size_t                                strEnumSize(const char* en);

    void                                  updateValues();
    void                                  updateItem    (Item &item);
    void                                  updateSavTitle(Item& sel);
    void                                  updateSavThumb(Item& sel);
    void                                  updateVideo();
    void                                  setDefaultKeys(const char* preset);

    bool                                  isInGameAndAlive() const;

    void                                  set(const char* item,const Tempest::Texture2d* value);
    void                                  set(const char* item,const uint32_t value);
    void                                  set(const char* item,const int32_t  value);
    void                                  set(const char* item,const int32_t  value,const char* post);
    void                                  set(const char* item,const int32_t  value,const int32_t max);
    void                                  set(const char* item,const char*    value);
  };

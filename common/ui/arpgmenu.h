#pragma once

#include <Tempest/Painter>
#include <Tempest/Rect>

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

class Npc;
class Item;
class World;
class InventoryRenderer;
class InventoryMenu;
class GthFont;

// gothic-diablo: the ARPG window: equipment, character, skill trees.
// The hero himself is not drawn here: while it is open the camera stands in front of him (Camera::setArpgPortrait),
// so he is seen in the middle of the screen between the two pages, with his idle animation and his real equipment.
// Drawn as an open book of the game (BOOK_BROWN_L/R pages, the book fonts, LETTERS sheets, INV_SLOT frames)
// with the game's item models (InventoryRenderer) - also as the icons of the skill tree nodes.
class ArpgMenu final {
  public:
    ArpgMenu();
    ~ArpgMenu();

    enum Page : uint8_t {
      P_Closed,
      P_Equip,
      P_Character,
      P_Skills,
      P_Trade,     // trade, a chest, looting: opened and closed with Gothic's menu (InventoryMenu keeps the state)
      };

    bool   isOpen()  const { return page!=P_Closed; }
    Page   current() const { return page; }
    void   open(Page p);
    void   close();
    void   toggle(Page p);   // the page that is shown again: closes

    // uiLayer: pages, slots, texts, item models (into 'items'); overlay: what goes over the models (counts)
    void   paint       (Tempest::Painter& p, int w, int h, float scale, InventoryRenderer& items, Npc& pl);
    void   paintOverlay(Tempest::Painter& p, float scale, Npc& pl);

    // left button: click = like Gothic's inventory (equip / use / take off), held and moved = drag the item:
    // onto an equipment slot (equip), onto a key 3-0 (rune / scroll), onto the backpack (take off), out of the book
    // (drop on the ground); right button: drop one (Shift: all), on worn things: take off
    bool   mouseDown (const Tempest::Point& pos, bool right, bool shift, Npc& pl);
    void   mouseUp   (const Tempest::Point& pos, bool right, Npc& pl);
    void   mouseMove (const Tempest::Point& pos);
    void   mouseWheel(int delta);
    bool   spellKey  (uint8_t slot, Npc& pl);   // 3..10: the rune / scroll under the cursor goes to that key
    void   setInventoryMenu(InventoryMenu* m) { invMenu = m; }

    // UTF-8 text for Gothic's fonts (they draw the bytes of the game's code page; Polish game = Windows-1250)
    static std::string gtxt(std::string_view utf8);

  private:
    enum HitKind : uint8_t {
      H_Tab,
      H_Cat,
      H_Pack,
      H_Equip,
      H_Tree,
      H_Node,
      H_Key,      // runes and scrolls under the keys 3-0: id = Gothic's slot (3..10), count = the key number
      H_Other,    // trade / chest / looting: a thing on the other's page
      H_CatL,     // trade / chest / looting: kind of things on the other's page
      H_TakeAll,  // chest / looting: take everything
      };
    struct Hit {
      Tempest::Rect r;
      HitKind       kind     = H_Tab;
      int           id       = 0;
      size_t        cls      = 0;
      bool          equipped = false;
      int32_t       count    = 0;
      int32_t       price    = 0;   // trade: what it costs / what the hero gets
      };
    struct Fonts;

    Page                 page   = P_Closed;
    int                  cat    = 0;   // backpack: kind of items shown
    int                  tree   = 0;   // skill tree shown
    int                  scroll = 0;   // backpack: rows scrolled
    int                  maxScroll = 0;
    int                  catL      = 0; // trade / chest / looting: the other's page
    int                  scrollL   = 0;
    int                  maxScrollL = 0;
    InventoryMenu*       invMenu   = nullptr;
    float                k      = 1;   // size of the window: 1 = 1080 lines
    Tempest::Point       mouse;
    std::vector<Hit>     hits;
    size_t               hoverCls = 0;
    Tempest::Rect        pageL, pageR;   // the pages (outside them: the ground)

    // drag and drop
    struct Drag {
      bool           pending  = false; // left button down on an item, not moved yet
      bool           on       = false; // being dragged
      Tempest::Point from;
      Tempest::Rect  src;
      HitKind        kind     = H_Pack;
      int            id       = 0;
      size_t         cls      = 0;
      bool           equipped = false;
      bool           shift    = false;
      int32_t        count    = 0;
      };
    Drag                 drag;
    void   click   (const Hit& h, Npc& pl);
    void   dropOn  (const Tempest::Point& pos, Npc& pl);
    void   paintDrag(Tempest::Painter& p, const Fonts& f, InventoryRenderer& items, Npc& pl);
    std::string_view dragHint(const Hit* target, const Item& it) const;

    // models of the game's items as icons of the skill nodes (made once, dropped on close or with a new world)
    const World*                                         iconWorld = nullptr;
    std::vector<std::pair<std::string,std::unique_ptr<Item>>> icons;
    const Item*          iconItem(const char* instance);

    const Hit*     hitAt(const Tempest::Point& pos) const;
    Tempest::Rect  drawPage (Tempest::Painter& p, const Tempest::Rect& r, bool left, const char* tex = nullptr) const;
    void   drawSlot  (Tempest::Painter& p, const Tempest::Rect& r, bool equipped, bool hover) const;
    void   drawBar   (Tempest::Painter& p, const Tempest::Rect& r, const char* fill, float v) const;
    void   drawRule  (Tempest::Painter& p, int x, int y, int w) const;
    int    drawSection(Tempest::Painter& p, const Fonts& f, const Tempest::Rect& c, int y, std::string_view name) const;
    void   drawTabs  (Tempest::Painter& p, int w, const Fonts& f);
    void   drawHint  (Tempest::Painter& p, int w, int h, const Fonts& f) const;
    void   paintEquip(Tempest::Painter& p, const Fonts& f, const Tempest::Rect& left, const Tempest::Rect& right, InventoryRenderer& items, Npc& pl);
    void   paintStats(Tempest::Painter& p, const Fonts& f, const Tempest::Rect& left, const Tempest::Rect& right, Npc& pl);
    void   paintTrees(Tempest::Painter& p, const Fonts& f, const Tempest::Rect& left, const Tempest::Rect& right, InventoryRenderer& items);
    void   paintInfo (Tempest::Painter& p, const Fonts& f, const Tempest::Rect& r, InventoryRenderer& items, const Item* it,
                      std::string_view extra = {}, std::string_view help = {}) const;
    void   paintTrade(Tempest::Painter& p, const Fonts& f, const Tempest::Rect& left, const Tempest::Rect& right, InventoryRenderer& items, Npc& pl);
    void   tradeMove (bool fromOther, size_t cls, int32_t count, bool equipped, Npc& pl);
    void   tradeDrop (const Tempest::Point& pos, Npc& pl);
  };

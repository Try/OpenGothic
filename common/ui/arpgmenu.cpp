#include "game/arpgkeys.h"
#include "arpgmenu.h"

#include <Tempest/Brush>
#include <Tempest/Color>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <map>

#include "graphics/inventoryrenderer.h"
#include "ui/inventorymenu.h"
#include "world/objects/interactive.h"
#include "world/objects/npc.h"
#include "world/objects/item.h"
#include "world/world.h"
#include "game/gamescript.h"
#include "game/gamesession.h"
#include "utils/gthfont.h"
#include "resources.h"
#include "gothic.h"

using namespace Tempest;

// ---------------------------------------------------------------------------------------------------------------
// text

static bool polishGame() {
  // Gothic's fonts draw the bytes of the game's code page; the Polish game is Windows-1250 ("Wartość:" in the scripts)
  static int cached = -1;
  if(cached>=0)
    return cached==1;
  auto w = Gothic::inst().world();
  if(w==nullptr)
    return true;
  bool pl = false;
  if(auto sym = w->script().findSymbol("NAME_VALUE")) {
    auto& s = sym->get_string();
    pl = (s.rfind("Warto",0)==0) || s.find('\x9c')!=std::string::npos;
    }
  cached = pl ? 1 : 0;
  return pl;
  }

std::string ArpgMenu::gtxt(std::string_view s) {
  struct Map { uint32_t cp; unsigned char cp1250; char ascii; };
  static const Map map[] = {
    {0x105,0xB9,'a'},{0x104,0xA5,'A'},{0x107,0xE6,'c'},{0x106,0xC6,'C'},{0x119,0xEA,'e'},{0x118,0xCA,'E'},
    {0x142,0xB3,'l'},{0x141,0xA3,'L'},{0x144,0xF1,'n'},{0x143,0xD1,'N'},{0x0F3,0xF3,'o'},{0x0D3,0xD3,'O'},
    {0x15B,0x9C,'s'},{0x15A,0x8C,'S'},{0x17A,0x9F,'z'},{0x179,0x8F,'Z'},{0x17C,0xBF,'z'},{0x17B,0xAF,'Z'},
    };
  const bool  pl = polishGame();
  std::string out;
  out.reserve(s.size());
  for(size_t i=0; i<s.size();) {
    const auto c = uint8_t(s[i]);
    uint32_t   cp  = c;
    size_t     len = 1;
    if((c&0xE0)==0xC0 && i+1<s.size()) {
      cp  = (uint32_t(c&0x1F)<<6) | uint32_t(uint8_t(s[i+1])&0x3F);
      len = 2;
      }
    else if((c&0xF0)==0xE0 && i+2<s.size()) {
      cp  = (uint32_t(c&0x0F)<<12) | (uint32_t(uint8_t(s[i+1])&0x3F)<<6) | uint32_t(uint8_t(s[i+2])&0x3F);
      len = 3;
      }
    i += len;
    if(cp<0x80) {
      out.push_back(char(cp));
      continue;
      }
    char rep = '?';
    for(auto& m:map)
      if(m.cp==cp) {
        rep = (pl || cp==0xF3 || cp==0xD3) ? char(m.cp1250) : m.ascii;
        break;
        }
    if(cp==0x2013 || cp==0x2014 || cp==0xB7)
      rep = '-';
    else if(cp==0xD7)
      rep = 'x';
    out.push_back(rep);
    }
  return out;
  }


// ---------------------------------------------------------------------------------------------------------------
// look: an open book of the game. Dark ink on the pages, red ink for values and what is chosen.

static const char* const BookL = "BOOK_BROWN_L.TGA";   // also in the game: BOOK_RED_*, BOOK_WOOD_*, BOOK_MAGE_*
static const char* const BookR = "BOOK_BROWN_R.TGA";

static const Color InkC    (0.20f, 0.13f, 0.07f, 1.f);
static const Color RedC    (0.62f, 0.10f, 0.05f, 1.f);
static const Color LineC   (0.26f, 0.17f, 0.08f, 0.80f);
static const Color RuleC   (0.26f, 0.17f, 0.08f, 0.35f);
static const Color FaintC  (0.26f, 0.17f, 0.08f, 0.14f);
static const Color LineRedC(0.55f, 0.09f, 0.05f, 0.85f);

// a copy of a game font in another colour (the old white font is light: multiplied, it gives coloured ink)
static const GthFont& tinted(const GthFont& base, int id, const Color& c) {
  static std::map<std::pair<const GthFont*,int>,GthFont> cache;
  const auto key = std::make_pair(&base, id);
  auto it = cache.find(key);
  if(it==cache.end()) {
    GthFont f = base;
    f.setColor(c);
    it = cache.emplace(key, std::move(f)).first;
    }
  return it->second;
  }

struct ArpgMenu::Fonts {
  explicit Fonts(float k) {
    using FT = Resources::FontType;
    body  = &Resources::font("font_10_book.tga", FT::Normal, 1.30f*k);
    small = &Resources::font("font_10_book.tga", FT::Normal, 1.05f*k);
    title = &Resources::font("font_20_book.tga", FT::Normal, 0.95f*k);
    sub   = &Resources::font("font_20_book.tga", FT::Normal, 0.75f*k);
    auto& old  = Resources::font("font_old_10_white.tga", FT::Normal, 1.25f*k);
    auto& oldS = Resources::font("font_old_10_white.tga", FT::Normal, 1.05f*k);
    ink   = &tinted(old,  0, InkC);
    red   = &tinted(old,  1, RedC);
    redS  = &tinted(oldS, 1, RedC);
    light = &old;
    }
  const GthFont* body  = nullptr; // the book's ink
  const GthFont* small = nullptr;
  const GthFont* title = nullptr;
  const GthFont* sub   = nullptr;
  const GthFont* ink   = nullptr; // labels
  const GthFont* red   = nullptr; // values, what is chosen
  const GthFont* redS  = nullptr;
  const GthFont* light = nullptr; // over dark things: bars, item counts
  };

// y = top of the line
static void text(Painter& p, const GthFont& f, int x, int y, std::string_view s) {
  f.drawText(p, x, y+f.pixelSize(), s);
  }

static void textRight(Painter& p, const GthFont& f, int right, int y, std::string_view s) {
  f.drawText(p, right-f.textSize(s).w, y+f.pixelSize(), s);
  }

static void textCenter(Painter& p, const GthFont& f, int cx, int y, std::string_view s) {
  f.drawText(p, cx-f.textSize(s).w/2, y+f.pixelSize(), s);
  }

// wrapped text, returns its height
static int wrap(Painter& p, const GthFont& f, int x, int y, int w, std::string_view s) {
  if(s.empty())
    return 0;
  const int lines = std::max(1, int(f.lineCount(w, s)));
  f.drawText(p, x, y+f.pixelSize(), w, 0, s, AlignLeft);
  return lines*f.pixelSize();
  }

// one or two centered lines (split at the space nearest the middle)
static void nameUnder(Painter& p, const GthFont& f, int cx, int y, int maxW, const std::string& s) {
  if(f.textSize(s).w<=maxW) {
    textCenter(p, f, cx, y, s);
    return;
    }
  size_t best = std::string::npos;
  const int mid = int(s.size()/2);
  for(size_t i=0; i<s.size(); ++i)
    if(s[i]==' ' && (best==std::string::npos || std::abs(int(i)-mid)<std::abs(int(best)-mid)))
      best = i;
  if(best==std::string::npos) {
    textCenter(p, f, cx, y, s);
    return;
    }
  textCenter(p, f, cx, y,                s.substr(0, best));
  textCenter(p, f, cx, y+f.pixelSize(),  s.substr(best+1));
  }

static void fill(Painter& p, int x, int y, int w, int h, const Color& c) {
  if(w<=0 || h<=0)
    return;
  p.setBrush(Brush(c, Painter::Alpha));
  p.drawRect(x, y, w, h);
  }

static void frame(Painter& p, const Rect& r, const Color& c, int t) {
  fill(p, r.x,         r.y,         r.w, t,       c);
  fill(p, r.x,         r.y+r.h-t,   r.w, t,       c);
  fill(p, r.x,         r.y+t,       t,   r.h-2*t, c);
  fill(p, r.x+r.w-t,   r.y+t,       t,   r.h-2*t, c);
  }

// a sheet of the game's letters (burnt edges)
static void letter(Painter& p, const Rect& r, float alpha) {
  if(auto t = Resources::loadTexture("LETTERS.TGA")) {
    p.setBrush(Brush(*t, Color(1.f, 1.f, 1.f, alpha)));
    p.drawRect(r.x, r.y, r.w, r.h, 0, 0, t->w(), t->h());
    }
  }


// ---------------------------------------------------------------------------------------------------------------

ArpgMenu::ArpgMenu()  = default;
ArpgMenu::~ArpgMenu() = default;

static ArpgProgress* progress() {
  auto gs = Gothic::inst().gameSession();
  return gs!=nullptr ? &gs->arpgProgress() : nullptr;
  }

// Diathic: percents from equipped weapons / rings (e.g. +3% of a knife) do not count for the limit of Biegłość
static int32_t equipBonus(const Npc& pl, int talent) {
  auto w = Gothic::inst().world();
  return w!=nullptr ? w->script().arpgEquipFightBonus(pl, talent) : 0;
  }

static ArpgProgress::Hero heroOf(const Npc& pl) {
  ArpgProgress::Hero h;
  h.level = pl.level();
  for(int t=1; t<=4; ++t)
    h.pct[t] = pl.hitChance(Talent(t)) - equipBonus(pl, t);
  return h;
  }

void ArpgMenu::open(Page p) {
  page   = p;
  scroll = 0;
  if(!undoSet) {
    if(auto pr = progress())
      undoBase = pr->ranks();
    undoSet = true;
    }
  if(p==P_Trade) {
    catL    = 0;
    scrollL = 0;
    cat     = 0;
    }
  }

void ArpgMenu::close() {
  page      = P_Closed;
  hoverCls  = 0;
  undoSet   = false;   // the points spent are kept
  undoBase.clear();
  lockedNodes.clear();
  drag      = Drag();
  hits.clear();
  icons.clear();
  iconWorld = nullptr;
  }

void ArpgMenu::toggle(Page p) {
  if(page==p)
    close(); else
    open(p);
  }

const Item* ArpgMenu::iconItem(const char* instance) {
  auto w = Gothic::inst().world();
  if(w==nullptr || instance==nullptr)
    return nullptr;
  if(w!=iconWorld) {
    icons.clear();
    iconWorld = w;
    }
  for(auto& i:icons)
    if(i.first==instance)
      return i.second.get();
  std::unique_ptr<Item> itm;
  const size_t id = w->script().findSymbolIndex(instance);
  if(id!=size_t(-1))
    itm = std::make_unique<Item>(*w, id, Item::T_Inventory);
  icons.emplace_back(instance, std::move(itm));
  return icons.back().second.get();
  }

const ArpgMenu::Hit* ArpgMenu::hitAt(const Point& pos) const {
  for(auto& h:hits)
    if(h.r.contains(pos))
      return &h;
  return nullptr;
  }

void ArpgMenu::mouseWheel(int delta) {
  if(page==P_Trade && pageL.contains(mouse)) {
    scrollL = std::clamp(scrollL + (delta>0 ? -1 : 1), 0, maxScrollL);
    return;
    }
  if(page!=P_Equip && page!=P_Trade)
    return;
  scroll = std::clamp(scroll + (delta>0 ? -1 : 1), 0, maxScroll);
  }

// which equipment slot (0..6, see Inventory::equippedIn) a thing goes to; -1: none
static int slotOf(const Item& it) {
  const auto cat = ItmFlags(it.mainFlag());
  const auto flg = ItmFlags(it.handle().flags);
  if(cat & ITM_CAT_NF)    return 0;
  if(cat & ITM_CAT_FF)    return 1;
  if(cat & ITM_CAT_ARMOR) return 2;
  if(flg & ITM_AMULET)    return 3;
  if(flg & ITM_RING)      return 4;
  if(flg & ITM_BELT)      return 6;
  return -1;
  }

static const Item* findItem(Npc& pl, size_t cls) {
  for(auto it = pl.inventory().iterator(Inventory::T_Inventory); it.isValid(); ++it)
    if(it->clsId()==cls)
      return &(*it);
  return nullptr;
  }

void ArpgMenu::mouseMove(const Point& pos) {
  mouse = pos;
  if(drag.pending && !drag.on) {
    const Point d = pos-drag.from;
    if(std::abs(d.x)>6 || std::abs(d.y)>6)
      drag.on = true;
    }
  }

bool ArpgMenu::mouseDown(const Point& pos, bool right, bool shift, Npc& pl) {
  mouse = pos;
  auto h = hitAt(pos);
  if(h==nullptr)
    return false;
  switch(h->kind) {
    case H_Tab:
      open(Page(h->id));
      return true;
    case H_Cat:
      cat    = h->id;
      scroll = 0;
      return true;
    case H_Tree:
      tree = h->id;
      return true;
    case H_Node: {
      // left button: a point into the node; right: one point back (only one added since the book was opened)
      auto pr = progress();
      if(h->count==0 || pr==nullptr)
        return true; // the root of the tree
      const ArpgSkills::NodeId id{size_t(tree), h->count==2 ? -1 : h->id/16, h->id%16};
      const bool ok = right ? pr->remove(id, undoBase) : pr->add(id, heroOf(pl));
      if(ok) {
        // Biegłość: Gothic's percent of the weapon goes with the rank (hit chance, moves of a warrior / master)
        if(const int tal = ArpgSkills::talentOf(id); tal>0)
          pl.world().script().arpgRaiseFightTalent(pl, tal, right ? -ArpgSkills::PctPerRank : ArpgSkills::PctPerRank);
        Gothic::inst().emitGlobalSound(Gothic::inst().loadSoundFx("MENU_SELECT"));
        }
      return true;
      }
    case H_CatL:
      catL    = h->id;
      scrollL = 0;
      return true;
    case H_TakeAll: {
      // everything from the chest / the body
      if(invMenu==nullptr)
        return true;
      std::vector<std::pair<size_t,size_t>> all;
      if(auto oi = invMenu->arpgOtherInventory())
        for(auto it = oi->iterator(invMenu->arpgOtherIter()); it.isValid(); ++it)
          all.emplace_back(it->clsId(), it.count());
      for(auto& i:all)
        invMenu->arpgMove(true, i.first, i.second);
      return true;
      }
    case H_Other:
    case H_Pack:
    case H_Equip:
    case H_Key:
      if(h->cls==0)
        return true;
      if(page==P_Trade && right) {
        tradeMove(h->kind==H_Other, h->cls, h->count, h->equipped, pl); // the whole stack
        return true;
        }
      if(page==P_Trade) {
        drag          = Drag();
        drag.pending  = true;
        drag.from     = pos;
        drag.src      = h->r;
        drag.kind     = h->kind;
        drag.cls      = h->cls;
        drag.equipped = h->equipped;
        drag.shift    = shift;
        drag.count    = h->count;
        return true;
        }
      if(right) {
        // right button: worn things come off, the rest falls to the ground (one, Shift: all)
        if(h->equipped)
          pl.unequipItem(h->cls); else
          pl.dropItem(h->cls, shift ? size_t(std::max<int32_t>(1, h->count)) : 1);
        return true;
        }
      drag          = Drag();
      drag.pending  = true;
      drag.from     = pos;
      drag.src      = h->r;
      drag.kind     = h->kind;
      drag.id       = h->id;
      drag.cls      = h->cls;
      drag.equipped = h->equipped;
      return true;
    }
  return false;
  }

void ArpgMenu::mouseUp(const Point& pos, bool right, Npc& pl) {
  mouse = pos;
  if(right || !drag.pending)
    return;
  const Drag d = drag;
  drag = Drag();
  if(page==P_Trade) {
    if(d.on) {
      drag = d;
      tradeDrop(pos, pl);
      drag = Drag();
      return;
      }
    // a click: one piece (Shift: the stack); gold and loot from chests and bodies go whole
    const bool other = (d.kind==H_Other);
    int32_t    cnt   = d.shift ? d.count : 1;
    if(other && invMenu!=nullptr && invMenu->isOpen()!=InventoryMenu::State::Trade)
      cnt = d.count;
    tradeMove(other, d.cls, cnt, d.equipped, pl);
    return;
    }
  if(d.on) {
    drag = d;          // dropOn reads it
    dropOn(pos, pl);
    drag = Drag();
    return;
    }
  // no move: a click, like in Gothic's inventory
  Hit h;
  h.kind     = d.kind;
  h.id       = d.id;
  h.cls      = d.cls;
  h.equipped = d.equipped;
  click(h, pl);
  }

void ArpgMenu::click(const Hit& h, Npc& pl) {
  if(h.cls==0)
    return;
  if(h.equipped)
    pl.unequipItem(h.cls); else
    pl.useItem(h.cls);      // equip / drink / read
  }

std::string_view ArpgMenu::dragHint(const Hit* t, const Item& it) const {
  if(t==nullptr) {
    if(pageL.contains(mouse) || pageR.contains(mouse))
      return {};
    return "wyrzu\xe6 na ziemi\xea";                    // drop on the ground
    }
  switch(t->kind) {
    case H_Equip: {
      const int s = slotOf(it);
      if(s<0 || !(s==t->id || (s==4 && t->id==5)))
        return "nie pasuje";
      return drag.equipped ? std::string_view() : std::string_view("za\xb3\xf3\xbf");   // put on
      }
    case H_Key:
      return ((ItmFlags(it.mainFlag()) & ITM_CAT_RUNE)!=0) ? std::string_view("pod klawisz") : std::string_view("tylko runy i zwoje");
    case H_Pack:
      return drag.equipped ? std::string_view("zdejmij") : std::string_view();
    default:
      return {};
    }
  }

void ArpgMenu::dropOn(const Point& pos, Npc& pl) {
  const Item* it = findItem(pl, drag.cls);
  if(it==nullptr)
    return;
  const Hit* t = hitAt(pos);
  if(t==nullptr) {
    if(pageL.contains(pos) || pageR.contains(pos)) {
      // somewhere on the right page (the backpack): worn things come off
      if(drag.equipped && pageR.contains(pos))
        pl.unequipItem(drag.cls);
      return;
      }
    // out of the book: on the ground, the whole stack (worn things first come off)
    const size_t cls   = drag.cls;
    const size_t count = it->count();
    if(drag.equipped)
      pl.unequipItem(cls);
    pl.dropItem(cls, std::max<size_t>(1, count));
    return;
    }
  switch(t->kind) {
    case H_Equip: {
      const int s = slotOf(*it);
      if(s>=0 && (s==t->id || (s==4 && t->id==5)) && !drag.equipped)
        pl.useItem(drag.cls);
      return;
      }
    case H_Key:
      if((ItmFlags(it->mainFlag()) & ITM_CAT_RUNE)!=0)
        pl.useItem(drag.cls, uint8_t(t->id), false);
      return;
    case H_Pack:
      if(drag.equipped)
        pl.unequipItem(drag.cls);
      return;
    default:
      return;
    }
  }

void ArpgMenu::paintDrag(Painter& p, const Fonts& f, InventoryRenderer& items, Npc& pl) {
  if(!drag.on)
    return;
  const Item* it = findItem(pl, drag.cls);
  if(it==nullptr) {
    drag = Drag();
    return;
    }
  // the place it came from goes darker
  fill(p, drag.src.x, drag.src.y, drag.src.w, drag.src.h, Color(0.f, 0.f, 0.f, 0.35f));

  // the target: green when it fits, red when not
  const Hit*       t = hitAt(mouse);
  std::string_view hint;
  std::string      tradeHint;
  if(page==P_Trade) {
    // trade, chest, looting: the hint says what happens on the other page
    const auto st    = invMenu!=nullptr ? invMenu->isOpen() : InventoryMenu::State::Closed;
    const bool other = (drag.kind==H_Other);
    if(other && pageR.contains(mouse))
      tradeHint = gtxt(st==InventoryMenu::State::Trade ? "kup" : "weź");
    else if(!other && pageL.contains(mouse) && st!=InventoryMenu::State::Ransack)
      tradeHint = drag.equipped ? gtxt("zdejmij najpierw") : gtxt(st==InventoryMenu::State::Trade ? "sprzedaj" : "odłóż");
    hint = tradeHint;
    t    = nullptr;
    } else {
    hint = dragHint(t, *it);
    }
  if(t!=nullptr && (t->kind==H_Equip || t->kind==H_Key || t->kind==H_Pack) && !hint.empty()) {
    const bool ok = (hint!="nie pasuje" && hint!="tylko runy i zwoje");
    const int  lw = std::max(2, int(3*k));
    frame(p, Rect(t->r.x-lw, t->r.y-lw, t->r.w+2*lw, t->r.h+2*lw),
          ok ? Color(0.15f, 0.55f, 0.15f, 0.95f) : Color(0.70f, 0.08f, 0.05f, 0.95f), lw);
    }

  // the thing itself at the cursor
  const int sz = int(72*k);
  items.drawItem(mouse.x-sz/2, mouse.y-sz/2, sz, sz, *it);
  if(!hint.empty()) {
    const int tw = f.light->textSize(hint).w;
    const Rect r(mouse.x-tw/2-int(8*k), mouse.y+sz/2+int(2*k), tw+int(16*k), f.light->pixelSize()+int(8*k));
    fill(p, r.x, r.y, r.w, r.h, Color(0.f, 0.f, 0.f, 0.6f));
    textCenter(p, *f.light, mouse.x, r.y+int(4*k), hint);
    }
  }

bool ArpgMenu::spellKey(uint8_t slot, Npc& pl) {
  if(page!=P_Equip || hoverCls==0)
    return false;
  for(auto it = pl.inventory().iterator(Inventory::T_Inventory); it.isValid(); ++it) {
    if(it->clsId()!=hoverCls)
      continue;
    if((ItmFlags(it->mainFlag()) & ITM_CAT_RUNE)==0)
      return false;
    pl.useItem(hoverCls, slot, false);
    return true;
    }
  return false;
  }

// ---------------------------------------------------------------------------------------------------------------
// drawing helpers: Gothic's own textures

Rect ArpgMenu::drawPage(Painter& p, const Rect& r, bool left, const char* tex) const {
  // the pages of the game's books: the left one is bound on its left side, the right one on its right side
  if(auto t = Resources::loadTexture(tex!=nullptr ? tex : (left ? BookL : BookR))) {
    const int tw = t->w(), th = t->h();
    const int u0 = left ? tw*192/512 : 0;
    const int u1 = left ? tw         : tw*320/512;
    p.setBrush(*t);
    p.drawRect(r.x, r.y, r.w, r.h, u0, 0, u1, th);
    } else {
    fill(p, r.x, r.y, r.w, r.h, Color(0.85f, 0.80f, 0.70f, 1.f));
    }
  // the paper without the cover
  const int x = r.x + int(float(r.w)*(left ? 0.215f : 0.045f));
  return Rect(x, r.y+int(float(r.h)*0.05f), int(float(r.w)*0.74f), int(float(r.h)*0.90f));
  }

void ArpgMenu::drawSlot(Painter& p, const Rect& r, bool equipped, bool hover) const {
  // the framed slots of Gothic's inventory: metal corners, the equipped one with a red border
  if(auto t = Resources::loadTexture(equipped ? "INV_SLOT_EQUIPPED_FOCUS.TGA" : "INV_SLOT_FOCUS.TGA")) {
    p.setBrush(*t);
    p.drawRect(r.x, r.y, r.w, r.h, 0, 0, t->w(), t->h());
    }
  if(hover) {
    // small: a lighter inside and a thin red line close around
    const int d  = std::max(2, r.w/16);
    const int lw = std::max(2, int(2.f*k));
    p.setBrush(Brush(Color(0.12f, 0.10f, 0.07f, 1.f), Painter::Add));
    p.drawRect(r.x+d, r.y+d, r.w-2*d, r.h-2*d);
    frame(p, Rect(r.x-lw, r.y-lw, r.w+2*lw, r.h+2*lw), LineRedC, lw);
    }
  }

void ArpgMenu::drawBar(Painter& p, const Rect& r, const char* fill, float v) const {
  if(auto t = Resources::loadTexture("BAR_BACK.TGA")) {
    p.setBrush(*t);
    p.drawRect(r.x, r.y, r.w, r.h, 0, 0, t->w(), t->h());
    }
  if(auto t = Resources::loadTexture(fill)) {
    const int bx = r.w*9/256, by = r.h*7/32; // the frame of BAR_BACK
    const int iw = int(float(r.w-2*bx)*std::clamp(v, 0.f, 1.f));
    p.setBrush(*t);
    p.drawRect(r.x+bx, r.y+by, iw, r.h-2*by, 0, 0, int(float(t->w())*std::clamp(v,0.f,1.f)), t->h());
    }
  }

void ArpgMenu::drawRule(Painter& p, int x, int y, int w) const {
  fill(p, x, y, w, std::max(1, int(2.f*k)), RuleC);
  }

int ArpgMenu::drawSection(Painter& p, const Fonts& f, const Rect& c, int y, std::string_view name) const {
  y += int(10*k);
  text(p, *f.sub, c.x, y, name);
  y += f.sub->pixelSize()+int(2*k);
  drawRule(p, c.x, y, c.w);
  return y+int(8*k);
  }

void ArpgMenu::drawTabs(Painter& p, int w, const Fonts& f) {
  // Diathic: the keys from ArpgKeys ("Sterowanie" in the main menu)
  const std::string names[3] = {
    "Ekwipunek ("   + ArpgKeys::keyName(ArpgKeys::key(ArpgKeys::Equipment)) + ")",
    "Postać ("      + ArpgKeys::keyName(ArpgKeys::key(ArpgKeys::Character)) + ")",
    "Umiejętności (" + ArpgKeys::keyName(ArpgKeys::key(ArpgKeys::Skills))   + ")"};
  const int gap = int(10*k);
  const int tw  = int(220*k);
  const int th  = int(46*k);
  const int y   = int(14*k);
  const int x0  = (w-(3*tw+2*gap))/2;
  for(int i=0; i<3; ++i) {
    const Rect r(x0+i*(tw+gap), y, tw, th);
    const bool on = (page==P_Equip+i);
    const bool hv = r.contains(mouse);
    letter(p, r, (on || hv) ? 1.f : 0.78f);
    auto& fn = on ? *f.red : *f.ink;
    textCenter(p, fn, r.x+r.w/2, r.y+(r.h-fn.pixelSize())/2, gtxt(names[i]));
    Hit h; h.r = r; h.kind = H_Tab; h.id = P_Equip+i;
    hits.push_back(h);
    }
  }

void ArpgMenu::drawHint(Painter& p, int w, int h, const Fonts& f) const {
  const std::string s = (page==P_Trade) ?
    gtxt("LPM - jedna sztuka      Shift+LPM / PPM - cały stos      przeciągnij na drugą stronę      Esc - koniec") :
    gtxt("Q / E - obrót kamery        Esc - zamknij");
  const int tw = f.small->textSize(s).w + int(70*k);
  const int th = int(40*k);
  const Rect r((w-tw)/2, h-int(12*k)-th, tw, th);
  letter(p, r, 0.92f);
  textCenter(p, *f.small, w/2, r.y+(r.h-f.small->pixelSize())/2, s);
  }

// ---------------------------------------------------------------------------------------------------------------

void ArpgMenu::paint(Painter& p, int w, int h, float /*scale*/, InventoryRenderer& items, Npc& pl) {
  if(page==P_Closed)
    return;
  hits.clear();
  hoverCls = 0;

  // the window follows the height of the screen (designed for 1080 lines)
  k = std::max(0.5f, float(h)/1080.f);
  const Fonts f(k);

  const int M   = int(16*k);
  const int top = int(14*k)+int(46*k)+int(10*k);
  const int ph  = h-top-M;
  Rect left, right;
  if(page==P_Skills) {
    // one open book in the middle: the tree needs room
    const int pw = std::min(int(float(ph)*0.68f), (w-2*M)/2);
    left  = Rect(w/2-pw, top, pw, ph);
    right = Rect(w/2,    top, pw, ph);
    } else {
    // two pages at the sides, the hero between them
    const int pw = std::min(int(float(ph)*0.66f), int(float(w)*0.34f));
    left  = Rect(M,      top, pw, ph);
    right = Rect(w-M-pw, top, pw, ph);
    }
  pageL = left;
  pageR = right;
  // trade, chest, looting: the other's page is a red book (not ours), ours the brown one
  const Rect cl = drawPage(p, left,  true, page==P_Trade ? "BOOK_RED_L.TGA" : nullptr);
  const Rect cr = drawPage(p, right, false);
  if(page!=P_Trade)
    drawTabs(p, w, f);

  switch(page) {
    case P_Closed:    break;
    case P_Equip:     paintEquip(p, f, cl, cr, items, pl); break;
    case P_Character: paintStats(p, f, cl, cr, pl);        break;
    case P_Skills:    paintTrees(p, f, cl, cr, items, pl); break;
    case P_Trade:     paintTrade(p, f, cl, cr, items, pl); break;
    }

  if(page!=P_Skills)
    drawHint(p, w, h, f);
  if(page==P_Equip || page==P_Trade)
    paintDrag(p, f, items, pl);
  }

static int itemCategory(const Item& it) {
  // 0 everything, 1 weapons, 2 armor, 3 magic, 4 potions, 5 food, 6 documents, 7 other
  const auto f = ItmFlags(it.mainFlag());
  if(f & (ITM_CAT_NF|ITM_CAT_FF|ITM_CAT_MUN))
    return 1;
  if(f & ITM_CAT_ARMOR)
    return 2;
  if(f & (ITM_CAT_RUNE|ITM_CAT_MAGIC))
    return 3;
  if(f & ITM_CAT_POTION)
    return 4;
  if(f & ITM_CAT_FOOD)
    return 5;
  if(f & ITM_CAT_DOCS)
    return 6;
  return 7;
  }

void ArpgMenu::paintEquip(Painter& p, const Fonts& f, const Rect& cl, const Rect& cr, InventoryRenderer& items, Npc& pl) {
  auto&       inv  = pl.inventory();
  auto        wrld = Gothic::inst().world();
  const Item* info = nullptr;

  // ---- left page: what the hero wears ----
  int y = cl.y;
  text(p, *f.title, cl.x, y, gtxt("Bezimienny"));
  y += f.title->pixelSize()+int(2*k);
  {
    char lvl[64] = {};
    std::snprintf(lvl, sizeof(lvl), "Poziom %d", int(pl.level()));
    std::string line = gtxt(lvl);
    if(wrld!=nullptr)
      if(auto g = wrld->script().findSymbol("TXT_GUILDS"))
        line = std::string(g->get_string(uint16_t(pl.guild()))) + "  -  " + line;
    text(p, *f.body, cl.x, y, line);
    y += f.body->pixelSize()+int(8*k);
  }
  drawRule(p, cl.x, y, cl.w);
  y += int(12*k);

  static const struct { uint8_t slot; const char* name; } slots[] = {
    {0,"Broń biała"}, {1,"Broń dystansowa"}, {2,"Pancerz"}, {3,"Amulet"}, {4,"Pierścień"}, {5,"Pierścień"}, {6,"Pas"},
    };
  const int gapX = int(8*k);
  const int colW = (cl.w-gapX)/2;
  const int ss   = std::min(int(80*k), colW*2/5);
  const int rowH = ss+int(10*k);
  for(size_t i=0; i<std::size(slots); ++i) {
    const int   cx = cl.x + int(i%2)*(colW+gapX);
    const int   cy = y + int(i/2)*rowH;
    const Rect  r(cx, cy, ss, ss);
    const Item* it = inv.equippedIn(slots[i].slot);
    const bool  hv = r.contains(mouse) && it!=nullptr;
    drawSlot(p, r, it!=nullptr, hv);
    if(it!=nullptr)
      items.drawItem(r.x+ss/8, r.y+ss/8, ss-ss/4, ss-ss/4, *it);
    const int tx = cx+ss+int(8*k);
    const int tw = colW-ss-int(8*k);
    text(p, *f.redS, tx, cy+int(2*k), gtxt(slots[i].name));
    const int ny = cy+int(4*k)+f.redS->pixelSize();
    if(it!=nullptr)
      f.body->drawText(p, tx, ny+f.body->pixelSize(), tw, 3*f.body->pixelSize(), it->description(), AlignLeft); else
      text(p, *f.small, tx, ny, "-");
    {
      Hit h; h.r = r; h.kind = H_Equip; h.id = slots[i].slot;
      if(it!=nullptr) {
        h.cls      = it->clsId();
        h.equipped = true;
        }
      hits.push_back(h);
      if(hv && it!=nullptr) {
        hoverCls = it->clsId();
        info     = it;
        }
    }
    }
  y += int((std::size(slots)+1)/2)*rowH+int(2*k);

  // life and mana (Gothic's bars)
  drawRule(p, cl.x, y, cl.w);
  y += int(12*k);
  {
    const int lab = int(96*k), bh = int(24*k);
    char txt[64] = {};
    text(p, *f.ink, cl.x, y+(bh-f.ink->pixelSize())/2, gtxt("Zdrowie"));
    drawBar(p, Rect(cl.x+lab, y, cl.w-lab, bh), "BAR_HEALTH.TGA",
            float(pl.attribute(ATR_HITPOINTS))/float(std::max(1,pl.attribute(ATR_HITPOINTSMAX))));
    std::snprintf(txt, sizeof(txt), "%d / %d", int(pl.attribute(ATR_HITPOINTS)), int(pl.attribute(ATR_HITPOINTSMAX)));
    textCenter(p, *f.light, cl.x+lab+(cl.w-lab)/2, y+(bh-f.light->pixelSize())/2, txt);
    y += bh+int(8*k);
    text(p, *f.ink, cl.x, y+(bh-f.ink->pixelSize())/2, gtxt("Mana"));
    drawBar(p, Rect(cl.x+lab, y, cl.w-lab, bh), "BAR_MANA.TGA",
            float(pl.attribute(ATR_MANA))/float(std::max(1,pl.attribute(ATR_MANAMAX))));
    std::snprintf(txt, sizeof(txt), "%d / %d", int(pl.attribute(ATR_MANA)), int(pl.attribute(ATR_MANAMAX)));
    textCenter(p, *f.light, cl.x+lab+(cl.w-lab)/2, y+(bh-f.light->pixelSize())/2, txt);
    y += bh+int(12*k);
  }

  // runes and scrolls under the keys 3-0 (1 and 2 drink potions)
  drawRule(p, cl.x, y, cl.w);
  y += int(12*k);
  {
    text(p, *f.ink, cl.x, y, gtxt("Klawisze 3–0"));
    y += f.ink->pixelSize()+int(2*k);
    y += wrap(p, *f.small, cl.x, y, cl.w, gtxt("Najedź na runę albo zwój w plecaku i naciśnij klawisz.")) + int(8*k);
    const int gap = int(6*k);
    const int qs  = std::min(int(58*k), (cl.w-7*gap)/8);
    for(uint8_t i=0; i<8; ++i) {
      const Rect  r(cl.x+i*(qs+gap), y, qs, qs);
      const Item* it = inv.currentSpell(i);
      const bool  hv = r.contains(mouse) && it!=nullptr;
      drawSlot(p, r, it!=nullptr, hv);
      if(it!=nullptr) {
        items.drawItem(r.x+qs/8, r.y+qs/8, qs-qs/4, qs-qs/4, *it);
        if(hv) {
          hoverCls = it->clsId();
          info     = it;
          }
        }
      Hit h; h.r = r; h.kind = H_Key; h.id = i+3; h.cls = 0; h.count = (i+3)%10; // Gothic's slot, the key number
      if(it!=nullptr) {
        h.cls      = it->clsId();
        h.equipped = true;
        }
      hits.push_back(h);
      }
    y += qs;
  }

  // protection, in short (the whole list: character page)
  y = drawSection(p, f, cl, y+int(4*k), gtxt("Ochrona"));
  {
    static const struct { Protection prot; const char* name; } prot[] = {
      {PROT_EDGE,"Przed bronią"}, {PROT_POINT,"Przed strzałami"}, {PROT_FIRE,"Przed ogniem"}, {PROT_MAGIC,"Przed magią"},
      };
    const int colW2 = (cl.w-int(24*k))/2;
    for(size_t i=0; i<std::size(prot); ++i) {
      const int x  = cl.x+int(i%2)*(colW2+int(24*k));
      const int ry = y+int(i/2)*(f.body->pixelSize()+int(8*k));
      char v[16] = {};
      std::snprintf(v, sizeof(v), "%d", int(pl.protection(prot[i].prot)));
      text(p, *f.body, x, ry, gtxt(prot[i].name));
      textRight(p, *f.red, x+colW2, ry+(f.body->pixelSize()-f.red->pixelSize())/2, v);
      }
  }

  // ---- right page: the backpack ----
  y = cr.y;
  text(p, *f.title, cr.x, y, gtxt("Plecak"));
  {
    int32_t gold = 0;
    if(wrld!=nullptr)
      if(auto g = wrld->script().goldId())
        gold = int32_t(pl.itemCount(g->index()));
    const std::string v  = std::to_string(gold);
    const int         gy = y+(f.title->pixelSize()-f.red->pixelSize())/2;
    textRight(p, *f.red, cr.x+cr.w, gy, v);
    textRight(p, *f.ink, cr.x+cr.w-f.red->textSize(v).w-int(8*k), gy, gtxt("Złoto:"));
  }
  y += f.title->pixelSize()+int(6*k);
  drawRule(p, cr.x, y, cr.w);
  y += int(8*k);

  static const char* cats[8] = {"Wszystko","Broń","Pancerze","Magia","Mikstury","Jedzenie","Dokumenty","Inne"};
  {
    const int cw = cr.w/4;
    const int ch = f.body->pixelSize()+int(12*k);
    for(int i=0; i<8; ++i) {
      const Rect r(cr.x+(i%4)*cw, y+(i/4)*ch, cw, ch);
      const bool on = (cat==i), hv = r.contains(mouse);
      auto& fn = on ? *f.red : (hv ? *f.ink : *f.body);
      const std::string s = gtxt(cats[i]);
      textCenter(p, fn, r.x+r.w/2, r.y+(r.h-fn.pixelSize())/2, s);
      if(on) {
        const int tw = fn.textSize(s).w;
        fill(p, r.x+(r.w-tw)/2, r.y+r.h-int(4*k), tw, std::max(1,int(2*k)), LineRedC);
        }
      Hit h; h.r = r; h.kind = H_Cat; h.id = i;
      hits.push_back(h);
      }
    y += 2*ch+int(10*k);
  }

  const int infoMin = int(210*k);
  const int cols  = 6;
  const int gap   = int(5*k);
  const int cs    = (cr.w-(cols-1)*gap)/cols;
  const int ind   = f.small->pixelSize()+int(4*k);
  const int rows  = std::max(1, (cr.y+cr.h-infoMin-int(10*k)-ind-y+gap)/(cs+gap));

  struct Cell { const Item* it; size_t cls; int32_t count; bool eq; };
  std::vector<Cell> list;
  const size_t goldId = [&]() -> size_t {
    if(wrld!=nullptr)
      if(auto g = wrld->script().goldId())
        return g->index();
    return size_t(-1);
    }();
  for(auto it = inv.iterator(Inventory::T_Inventory); it.isValid(); ++it) {
    const Item& itm = *it;
    if(itm.clsId()==goldId)
      continue;
    if(cat!=0 && itemCategory(itm)!=cat)
      continue;
    list.push_back({&itm, itm.clsId(), int32_t(it.count()), it.isEquipped()});
    }
  const int totalRows = int((list.size()+cols-1)/cols);
  maxScroll = std::max(0, totalRows-rows);
  scroll    = std::clamp(scroll, 0, maxScroll);
  for(int i=0; i<rows*cols; ++i) {
    const Rect   r(cr.x+(i%cols)*(cs+gap), y+(i/cols)*(cs+gap), cs, cs);
    const size_t id = size_t(i+scroll*cols);
    if(id>=list.size()) {
      drawSlot(p, r, false, false);
      continue;
      }
    auto&      c  = list[id];
    const bool hv = r.contains(mouse);
    drawSlot(p, r, c.eq, hv);
    items.drawItem(r.x+cs/8, r.y+cs/8, cs-cs/4, cs-cs/4, *c.it);
    Hit h; h.r = r; h.kind = H_Pack; h.cls = c.cls; h.equipped = c.eq; h.count = c.count;
    hits.push_back(h);
    if(hv) {
      hoverCls = c.cls;
      info     = c.it;
      }
    }
  if(maxScroll>0) {
    char txt[64] = {};
    std::snprintf(txt, sizeof(txt), "%s %d / %d", gtxt("kółko myszy - strona").c_str(), scroll+1, maxScroll+1);
    textRight(p, *f.small, cr.x+cr.w, y+rows*(cs+gap), txt);
    }

  const int infoY = y+rows*(cs+gap)+ind+int(6*k);
  paintInfo(p, f, Rect(cr.x-int(14*k), infoY, cr.w+int(28*k), cr.y+cr.h-infoY), items, info);
  }

void ArpgMenu::paintInfo(Painter& p, const Fonts& f, const Rect& r, InventoryRenderer& items, const Item* it,
                         std::string_view extra, std::string_view help) const {
  // the description on a sheet of the game's letters
  letter(p, r, 1.f);
  const int pad = int(28*k);  // the burnt edge
  if(it==nullptr) {
    const std::string s = !help.empty() ? std::string(help) :
      gtxt("Najedź na przedmiot.\nLPM: załóż, użyj, zdejmij; przeciągnij: przenieś albo wyrzuć.\nPPM: wyrzuć jeden (Shift: wszystkie), na założonym: zdejmij.");
    const int lines = std::max(1, int(f.small->lineCount(r.w-2*pad, s)));
    f.small->drawText(p, r.x+pad, r.y+(r.h-lines*f.small->pixelSize())/2+f.small->pixelSize(), r.w-2*pad, 0, s, AlignHCenter);
    return;
    }
  const int mw = r.h-2*pad;          // model on the right
  const int tw = r.w-2*pad-mw-int(8*k);
  int y = r.y+pad-int(4*k);
  auto& fName = f.sub->textSize(it->description()).w<=tw ? *f.sub : *f.body;
  text(p, fName, r.x+pad, y, it->description());
  y += fName.pixelSize()+int(6*k);
  const int bottom = r.y+r.h-pad+int(6*k) - (extra.empty() ? 0 : f.redS->pixelSize()+int(4*k));
  for(size_t i=0; i<Item::MAX_UI_ROWS; ++i) {
    auto    txt = it->uiText(i);
    int32_t val = it->uiValue(i);
    if(txt.empty())
      continue;
    if(y+f.small->pixelSize()>bottom)
      break;
    text(p, *f.small, r.x+pad, y, txt);
    if(val!=0) {
      char v[32] = {};
      std::snprintf(v, sizeof(v), "%d", int(val));
      textRight(p, *f.redS, r.x+pad+tw, y, v);
      }
    y += f.small->pixelSize()+int(2*k);
    }
  if(!extra.empty())
    text(p, *f.redS, r.x+pad, std::min(y+int(2*k), bottom+int(2*k)), extra);
  items.drawItem(r.x+r.w-pad-mw, r.y+pad, mw, mw, *it);
  }

void ArpgMenu::paintStats(Painter& p, const Fonts& f, const Rect& cl, const Rect& cr, Npc& pl) {
  auto      w    = Gothic::inst().world();
  const int line = f.body->pixelSize()+int(9*k);
  char      buf[128] = {};

  auto row = [&](const Rect& c, int& y, std::string_view key, std::string_view val) {
    text(p, *f.body, c.x, y, key);
    textRight(p, *f.red, c.x+c.w, y+(f.body->pixelSize()-f.red->pixelSize())/2, val);
    fill(p, c.x, y+f.body->pixelSize()+int(3*k), c.w, std::max(1,int(k)), FaintC);
    y += line;
    };
  auto section = [&](const Rect& c, int& y, std::string_view name) {
    y = drawSection(p, f, c, y, name);
    };

  // ---- left page: the hero ----
  int y = cl.y;
  text(p, *f.title, cl.x, y, gtxt("Bezimienny"));
  y += f.title->pixelSize()+int(2*k);
  if(w!=nullptr)
    if(auto g = w->script().findSymbol("TXT_GUILDS")) {
      text(p, *f.body, cl.x, y, g->get_string(uint16_t(pl.guild())));
      y += f.body->pixelSize()+int(2*k);
      }

  section(cl, y, gtxt("Postęp"));
  std::snprintf(buf, sizeof(buf), "%d", int(pl.level()));
  row(cl, y, gtxt("Poziom"), buf);
  std::snprintf(buf, sizeof(buf), "%d / %d", int(pl.experience()), int(pl.experienceNext()));
  row(cl, y, gtxt("Doświadczenie"), buf);
  {
    const int   bh = int(20*k);
    const float v  = pl.experienceNext()>0 ? float(pl.experience())/float(pl.experienceNext()) : 0.f;
    drawBar(p, Rect(cl.x, y-int(2*k), cl.w, bh), "BAR_MISC.TGA", v);
    y += bh+int(8*k);
  }
  std::snprintf(buf, sizeof(buf), "%d", int(pl.learningPoints()));
  row(cl, y, gtxt("Punkty nauki"), buf);
  {
    auto pr = progress();
    std::snprintf(buf, sizeof(buf), "%d", pr!=nullptr ? int(std::max(0, pr->freePoints(pl.level()))) : 0);
    row(cl, y, gtxt("Punkty umiejętności"), buf);
  }
  y += wrap(p, *f.small, cl.x, y-int(4*k), cl.w, gtxt("Do drzewek ("+ArpgKeys::keyName(ArpgKeys::key(ArpgKeys::Skills))+"): 1 za poziom; każdy nauczyciel zamienia 5 punktów nauki na 1; kamienne tablice walki dają punkty do Biegłości."));

  section(cl, y, gtxt("Atrybuty"));
  std::snprintf(buf, sizeof(buf), "%d", int(pl.attribute(ATR_STRENGTH)));
  row(cl, y, gtxt("Siła"), buf);
  std::snprintf(buf, sizeof(buf), "%d", int(pl.attribute(ATR_DEXTERITY)));
  row(cl, y, gtxt("Zręczność"), buf);
  std::snprintf(buf, sizeof(buf), "%d / %d", int(pl.attribute(ATR_HITPOINTS)), int(pl.attribute(ATR_HITPOINTSMAX)));
  row(cl, y, gtxt("Zdrowie"), buf);
  std::snprintf(buf, sizeof(buf), "%d / %d", int(pl.attribute(ATR_MANA)), int(pl.attribute(ATR_MANAMAX)));
  row(cl, y, gtxt("Mana"), buf);

  section(cl, y, gtxt("Ochrona"));
  std::snprintf(buf, sizeof(buf), "%d", int(pl.protection(PROT_EDGE)));
  row(cl, y, gtxt("Przed bronią"), buf);
  std::snprintf(buf, sizeof(buf), "%d", int(pl.protection(PROT_POINT)));
  row(cl, y, gtxt("Przed strzałami"), buf);
  std::snprintf(buf, sizeof(buf), "%d", int(pl.protection(PROT_FIRE)));
  row(cl, y, gtxt("Przed ogniem"), buf);
  std::snprintf(buf, sizeof(buf), "%d", int(pl.protection(PROT_MAGIC)));
  row(cl, y, gtxt("Przed magią"), buf);

  // ---- right page: Gothic's talents (names from the scripts) and the ARPG fight ----
  // Diathic: everything the skill trees give (only what the hero has), small rows under "Walka"
  ArpgPerks pk;
  if(auto pr = progress())
    pk = pr->perks();
  std::vector<std::pair<std::string,std::string>> perkRows;
  {
    auto num = [](float v, int prec) {
      char b[32] = {};
      std::snprintf(b, sizeof(b), "%.*f", prec, double(v));
      std::string r = b;
      if(r.find('.')!=std::string::npos) {
        while(!r.empty() && r.back()=='0')
          r.pop_back();
        if(!r.empty() && r.back()=='.')
          r.pop_back();
        }
      for(auto& c:r)
        if(c=='.')
          c = ',';
      return r;
      };
    auto pc  = [&](float v) { return "+"+num(v*100.f, 0)+"%"; };
    auto add = [&](const char* k, std::string v) { perkRows.emplace_back(k, std::move(v)); };
    if(pk.meleeDmg>0)      add("Obrażenia ciosu", pc(pk.meleeDmg));
    if(pk.critAdd>0)       add("Szansa na pełne obrażenia", "+"+std::to_string(pk.critAdd)+"%");
    if(pk.bleed>0)         add("Krwawienie (3 s)", num(pk.bleed*100.f,0)+"% ciosu / s");
    if(pk.comboWindow>0)   add("Dłuższe okno serii", pc(pk.comboWindow));
    if(pk.fourthHit)       add("Czwarty cios serii", "x1,5");
    if(pk.arc>120.f)       add("Szerokość zamachu", num(pk.arc,0)+"°");
    if(pk.knockback>0)     add("Odrzut małych stworów", num(pk.knockback,1)+" m");
    if(pk.vsDown>0)        add("Przeciw powalonym", pc(pk.vsDown));
    if(pk.leech>0)         add("Pijawka (zdrowie z obrażeń)", num(pk.leech*100.f,0)+"%");
    if(pk.hardSkin>0)      add("Twarda skóra (3 s po ciosie)", "+"+std::to_string(pk.hardSkin)+" ochrony");
    if(pk.shotSpeed>0)     add("Szybszy naciąg", pc(pk.shotSpeed));
    if(pk.pierce>0)        add("Strzała przebija", std::to_string(pk.pierce)+" wrog.");
    if(pk.farAim>0)        add("Celność powyżej 15 m", pc(pk.farAim));
    if(pk.whirl)           add("Wir (ostatni cios serii)", "wokół");
    if(pk.chargeWindow>0.2f+1e-4f) add("Złote okno ładowania", num(pk.chargeWindow,1)+" s");
    if(pk.sureHand)        add("Pewna ręka (późne puszczenie)", "x2");
    if(pk.getUpDelay>0)    add("Wstrząs (powaleni dłużej)", "+"+num(pk.getUpDelay,1)+" s");
    if(pk.guardBreak)      add("Łamacz gardy", "tak");
    if(pk.waveRange>0)     add("Fala uderzeniowa", num(pk.waveRange/100.f,0)+" m");
    if(pk.battleRush)      add("Pęd bitewny (zabójstwo = bez czekania)", "tak");
    if(pk.piercingShot)    add("Strzała przeszywająca", "tak");
    if(pk.volley>0)        add("Grad strzał", "+"+std::to_string(pk.volley));
    if(pk.heavyHead>0)     add("Ciężki grot", pk.heavyHead>1 ? "powala" : "odpycha");
    if(pk.innosWrath)      add("Gniew Innosa", "ogień 4 s");
    if(pk.rollCdCut>0)     add("Krótsza przerwa fikołka", "-"+num(pk.rollCdCut,2)+" s");
    if(pk.secondWind)      add("Drugi oddech", "2 fikołki");
    if(pk.rollDist>0)      add("Dalszy przewrót", pc(pk.rollDist));
    if(pk.rollThrough)     add("Przez szereg wrogów", "tak");
    if(pk.counter>0)       add("Kontratak (2 s po fikołku)", pc(pk.counter));
    if(pk.rollStrike)      add("Cios z przewrotu", "tak");
    if(pk.sandEyes>0)      add("Piasek w oczy", num(pk.sandEyes,0)+" s");
    if(pk.trip)            add("Podcięcie", "tak");
    if(pk.acrobat)         add("Akrobata (upadki)", "bez obrażeń");
    if(pk.arrowDodge>0)    add("Wyczucie strzał (1 s)", num(pk.arrowDodge*100.f,0)+"%");
    if(pk.shadow)          add("Cień (po fikołku)", "1 s");
  }
  const int smallLine = f.small->pixelSize()+int(5*k);

  y = cr.y;
  text(p, *f.title, cr.x, y, gtxt("Umiejętności Gothica"));
  y += f.title->pixelSize()+int(2*k);
  const int perksH     = perkRows.empty() ? 0 : f.sub->pixelSize()+int(30*k)+int(perkRows.size())*smallLine;
  const int talentsEnd = cr.y+cr.h-(6*line+f.sub->pixelSize()+int(30*k))-perksH; // room for the fight below
  if(w!=nullptr) {
    auto& sc   = w->script();
    auto* tal  = sc.findSymbol("TXT_TALENTS");
    auto* talV = sc.findSymbol("TXT_TALENTS_SKILLS");
    if(tal!=nullptr && talV!=nullptr) {
      y += int(8*k);
      const bool g2  = Gothic::inst().version().game==2;
      const int  max = g2 ? TALENT_MAX_G2 : TALENT_MAX_G1;
      for(int i=0; i<max; ++i) {
        auto& name = tal->get_string(uint16_t(i));
        if(name.empty())
          continue;
        // Diathic: the weapons always, the other talents only when the hero has them (room for the trees below)
        if(!(i>=TALENT_1H && i<=TALENT_CROSSBOW) && pl.talentSkill(Talent(i))<=0)
          continue;
        if(y+line>talentsEnd)
          break;
        // skill level: "a|b|c" from the scripts, picked by the talent's skill
        std::string_view lv = talV->get_string(uint16_t(i));
        int sk = pl.talentSkill(Talent(i));
        for(int s=0; s<sk; ++s) {
          auto pos = lv.find('|');
          if(pos==std::string_view::npos) {
            lv = {};
            break;
            }
          lv = lv.substr(pos+1);
          }
        lv = lv.substr(0, lv.find('|'));
        std::string val(lv);
        if(i>=TALENT_1H && i<=TALENT_CROSSBOW) {
          std::snprintf(buf, sizeof(buf), "  %d%%", int(g2 ? pl.hitChance(Talent(i)) : pl.talentValue(Talent(i))));
          val += buf;
          }
        row(cr, y, name, val);
        }
      }
    }

  section(cr, y, gtxt("Walka"));
  {
    auto dmgOf = [](const Item* it) { return it!=nullptr ? int(it->handle().damage_total) : 0; };
    auto& inv  = pl.inventory();
    std::snprintf(buf, sizeof(buf), "%d", int(pl.attribute(ATR_STRENGTH))+dmgOf(inv.equippedIn(0)));
    row(cr, y, gtxt("Obrażenia wręcz (siła + broń)"), buf);
    if(inv.equippedIn(1)!=nullptr) {
      std::snprintf(buf, sizeof(buf), "%d", (int(pl.attribute(ATR_DEXTERITY))+dmgOf(inv.equippedIn(1)))/2);
      row(cr, y, gtxt("Obrażenia strzałą (x0,5)"), buf);
      }
    // with the skill trees
    std::snprintf(buf, sizeof(buf), "+%d%%", int(std::lround((0.2f+pk.atkSpeed)*100.f)));
    row(cr, y, gtxt("Szybkość ataku"), buf);
    std::snprintf(buf, sizeof(buf), "x%.1f, %.1f s, co %.1f s", double(pk.chargeMul), double(pk.chargeTime), double(std::max(0.5f, pk.chargeCd)));
    for(auto& c:buf)
      if(c=='.')
        c = ',';
    row(cr, y, gtxt("Atak ładowany"), gtxt(buf));
    std::snprintf(buf, sizeof(buf), "%.2f s", double(0.45f+pk.rollInvuln));
    for(auto& c:buf)
      if(c=='.')
        c = ',';
    row(cr, y, gtxt("Fikołek bez obrażeń"), buf);
  }
  if(!perkRows.empty()) {
    section(cr, y, gtxt("Z drzewek umiejętności"));
    const int bottom = cr.y+cr.h;
    for(size_t i=0; i<perkRows.size(); ++i) {
      if(y+smallLine>bottom) {
        text(p, *f.small, cr.x, y, "...");
        break;
        }
      text(p, *f.small, cr.x, y, gtxt(perkRows[i].first));
      textRight(p, *f.redS, cr.x+cr.w, y+(f.small->pixelSize()-f.redS->pixelSize())/2, gtxt(perkRows[i].second));
      y += smallLine;
      }
    }
  }

void ArpgMenu::paintTrees(Painter& p, const Fonts& f, const Rect& cl, const Rect& cr, InventoryRenderer& items, Npc& pl) {
  auto& trees = ArpgSkills::trees();
  tree = std::clamp(tree, 0, int(trees.size())-1);
  auto& t  = trees[size_t(tree)];
  auto  pr = progress();
  const auto    hero    = heroOf(pl);
  const int32_t lvl     = hero.level;
  const int32_t freePts = pr!=nullptr ? pr->freePoints(lvl) : 0;
  const bool    hasKey  = t.keystone.name!=nullptr;
  auto rankOf = [&](int b, int n) {
    return pr!=nullptr ? pr->rank(ArpgSkills::NodeId{size_t(tree), b, n}) : 0;
    };
  lockedNodes.clear();

  // what the cursor is on: -1 nothing, 0 the root, 1 a node, 2 the keystone
  int selKind = -1, selB = 0, selN = 0;

  // ---- right page: the tree as a graph (root - trunk - branches - keystone) ----
  int y = cr.y;
  text(p, *f.title, cr.x, y, gtxt(t.name));
  textRight(p, *f.red, cr.x+cr.w, y+(f.title->pixelSize()-f.red->pixelSize())/2, gtxt(t.key));
  y += f.title->pixelSize()+int(2*k);
  y += wrap(p, *f.small, cr.x, y, cr.w, gtxt(t.root))+int(6*k);
  drawRule(p, cr.x, y, cr.w);
  y += int(8*k);

  const int rows  = std::max(1, int(t.branches.size()));
  const int gy    = y;
  const int rh    = (cr.y+cr.h-gy)/rows;
  const int colW  = cr.w/5;
  const int nameH = f.redS->pixelSize()+int(4*k);      // branch name over the first node
  const int below = f.redS->pixelSize()+2*f.small->pixelSize()+int(6*k);
  const int ns    = std::max(int(24*k), std::min({int(64*k), colW-int(26*k), rh-nameH-below}));
  const int lw    = std::max(2, int(3*k));
  auto nodeRect = [&](int col, int row) {
    return Rect(cr.x+col*colW+(colW-ns)/2, gy+row*rh+nameH, ns, ns);
    };
  auto midY = [](const Rect& r) { return r.y+r.h/2; };
  auto grow = [](const Rect& r, int d) { return Rect(r.x-d, r.y-d, r.w+2*d, r.h+2*d); };

  // lines (under the nodes): the way to learned nodes in red
  const Rect root   = grow(nodeRect(0, rows/2), int(4*k));
  const int  trunkX = cr.x+colW;
  const bool any    = pr!=nullptr && pr->treePoints(size_t(tree))>0;
  fill(p, root.x+root.w, midY(root)-lw/2, trunkX-(root.x+root.w)+lw, lw, any ? LineRedC : LineC);
  fill(p, trunkX, midY(nodeRect(1,0))-lw/2, lw, midY(nodeRect(1,rows-1))-midY(nodeRect(1,0))+lw, LineC);
  for(int b=0; b<int(t.branches.size()); ++b) {
    auto&      br = t.branches[size_t(b)];
    const Rect n0 = nodeRect(1, b);
    fill(p, trunkX, midY(n0)-lw/2, n0.x-trunkX, lw, rankOf(b,0)>0 ? LineRedC : LineC);
    for(int n=1; n<int(br.nodes.size()); ++n) {
      const Rect a = nodeRect(n, b), c = nodeRect(n+1, b);
      fill(p, a.x+a.w, midY(a)-lw/2, c.x-(a.x+a.w), lw, rankOf(b,n)>0 ? LineRedC : LineC);
      }
    }

  // the keystone: red lines from the ends of the branches it needs
  std::vector<int> from = t.keyFrom;
  if(!hasKey)
    from.clear();
  else
  if(from.empty())
    for(int b=0; b<int(t.branches.size()); ++b)
      from.push_back(b);
  int keyRow = 0;
  for(int b:from)
    keyRow += b;
  keyRow = (keyRow+int(from.size())/2)/std::max(1, int(from.size()));
  const Rect key   = grow(nodeRect(4, keyRow), int(6*k));
  const int  collX = cr.x+4*colW-lw;
  int ymin = midY(key), ymax = midY(key);
  for(int b:from) {
    auto&      br = t.branches[size_t(b)];
    const Rect e  = nodeRect(int(br.nodes.size()), b);
    fill(p, e.x+e.w, midY(e)-lw/2, collX-(e.x+e.w)+lw, lw, LineRedC);
    ymin = std::min(ymin, midY(e));
    ymax = std::max(ymax, midY(e));
    }
  if(hasKey) {
    fill(p, collX, ymin-lw/2, lw, ymax-ymin+lw, LineRedC);
    fill(p, collX, midY(key)-lw/2, key.x-collX, lw, LineRedC);
    }

  // nodes: Gothic's slot frames with item models; learned = the red frame of worn things,
  // not open yet = dimmed (over the model, in paintOverlay)
  auto node = [&](const Rect& r, const char* icon, int kind, int b, int n) {
    const bool hv = r.contains(mouse);
    bool learned = (kind==0), open = true;
    if(kind>0 && pr!=nullptr) {
      const ArpgSkills::NodeId id{size_t(tree), kind==2 ? -1 : b, n};
      learned = pr->rank(id)>0;
      std::string why;
      open = learned || pr->canAdd(id, hero, &why) || why=="Brak wolnych punktów umiejętności";
      }
    drawSlot(p, r, learned, hv);
    if(auto itm = iconItem(icon)) {
      const int d = r.w/7;
      items.drawItem(r.x+d, r.y+d, r.w-2*d, r.h-2*d, *itm);
      }
    if(!open)
      lockedNodes.push_back(r);
    if(hv) {
      selKind = kind;
      selB    = b;
      selN    = n;
      }
    Hit h; h.r = r; h.kind = H_Node; h.id = b*16+n; h.count = kind;
    hits.push_back(h);
    };

  node(root, t.icon, 0, 0, 0);
  textCenter(p, *f.redS, root.x+root.w/2, root.y+root.h+int(3*k), gtxt(t.name));

  for(int b=0; b<int(t.branches.size()); ++b) {
    auto&      br = t.branches[size_t(b)];
    const Rect n0 = nodeRect(1, b);
    if(t.weapons && b<4 && pr!=nullptr) {
      char buf[128] = {};
      const int32_t eb = Gothic::inst().player()!=nullptr ? equipBonus(*Gothic::inst().player(), b+1) : 0;
      if(eb!=0)
        std::snprintf(buf, sizeof(buf), "%s   %d%% %+d%% z broni  (nauka do %d%%)", br.name, int(hero.pct[b+1]), int(eb), int(pr->cap(b+1))); else
        std::snprintf(buf, sizeof(buf), "%s   %d%%  (nauka do %d%%)", br.name, int(hero.pct[b+1]), int(pr->cap(b+1)));
      text(p, *f.redS, n0.x-int(2*k), n0.y-nameH, gtxt(buf));
      } else {
      text(p, *f.redS, n0.x-int(2*k), n0.y-nameH, gtxt(br.name));
      }
    for(int n=0; n<int(br.nodes.size()); ++n) {
      auto&      nd = br.nodes[size_t(n)];
      const Rect r  = nodeRect(n+1, b);
      node(r, nd.icon, 1, b, n);
      char rank[16] = {};
      std::snprintf(rank, sizeof(rank), "%d/%d", int(rankOf(b,n)), nd.max);
      textCenter(p, rankOf(b,n)>0 ? *f.red : *f.redS, r.x+r.w/2, r.y+r.h+int(2*k), rank);
      nameUnder(p, *f.small, r.x+r.w/2, r.y+r.h+int(3*k)+f.redS->pixelSize(), colW-int(16*k), gtxt(nd.name));
      }
    }

  if(hasKey) {
    node(key, t.keystone.icon, 2, 0, 0);
    textCenter(p, *f.redS,  key.x+key.w/2, key.y+key.h+int(3*k), gtxt(t.keystone.name));
    textCenter(p, *f.small, key.x+key.w/2, key.y+key.h+int(4*k)+f.redS->pixelSize(),
               gtxt(rankOf(-1,0)>0 ? "kluczowy, poznany" : "kluczowy"));
    }

  // ---- left page: the trees, the points, the note on the node under the cursor ----
  y = cl.y;
  text(p, *f.title, cl.x, y, gtxt("Umiejętności"));
  y += f.title->pixelSize()+int(8*k);
  const int eh = trees.size()>3 ? int(70*k) : int(84*k);
  for(size_t i=0; i<trees.size(); ++i) {
    const Rect r(cl.x, y, cl.w, eh);
    const bool on = (int(i)==tree), hv = r.contains(mouse);
    if(on) {
      fill(p, r.x, r.y, r.w, r.h, Color(0.55f, 0.09f, 0.05f, 0.08f));
      frame(p, r, LineRedC, std::max(1, int(2*k)));
      } else if(hv) {
      frame(p, r, RuleC, std::max(1, int(2*k)));
      }
    const Rect ic(r.x+int(6*k), r.y+int(6*k), eh-int(12*k), eh-int(12*k));
    drawSlot(p, ic, on, false);
    if(auto itm = iconItem(trees[i].icon)) {
      const int d = ic.w/7;
      items.drawItem(ic.x+d, ic.y+d, ic.w-2*d, ic.h-2*d, *itm);
      }
    int32_t inTree = 0;
    if(pr!=nullptr)
      inTree = pr->treePoints(i)+pr->rank(ArpgSkills::NodeId{i,-1,0});
    char buf[96] = {};
    if(trees[i].key[0]=='\0')
      std::snprintf(buf, sizeof(buf), "zawsze działa,  punkty w drzewku: %d", int(inTree)); else
      std::snprintf(buf, sizeof(buf), "klawisz %s,  punkty w drzewku: %d", trees[i].key, int(inTree));
    auto& fn = on ? *f.red : *f.ink;
    const int tx = ic.x+ic.w+int(12*k);
    text(p, fn,       tx, r.y+int(12*k), gtxt(trees[i].name));
    text(p, *f.small, tx, r.y+int(16*k)+fn.pixelSize(), gtxt(buf));
    Hit h; h.r = r; h.kind = H_Tree; h.id = int(i);
    hits.push_back(h);
    y += eh+int(6*k);
    }
  y += int(6*k);
  drawRule(p, cl.x, y, cl.w);
  y += int(12*k);
  {
    char buf[16] = {};
    std::snprintf(buf, sizeof(buf), "%d", int(std::max(0, freePts)));
    text(p, *f.ink, cl.x, y, gtxt("Punkty umiejętności:"));
    textRight(p, freePts>0 ? *f.red : *f.ink, cl.x+cl.w, y, buf);
    y += f.ink->pixelSize()+int(4*k);
  }
  {
    // points found for one branch (stone tablets)
    std::string extra;
    for(int b=0; pr!=nullptr && b<int(t.branches.size()); ++b) {
      const int32_t left = pr->branchLeft(size_t(tree), b);
      if(left<=0)
        continue;
      char buf[96] = {};
      std::snprintf(buf, sizeof(buf), "%s%s: %d", extra.empty() ? "Tylko do gałęzi " : ", ", t.branches[size_t(b)].name, int(left));
      extra += buf;
      }
    if(!extra.empty())
      y += wrap(p, *f.redS, cl.x, y, cl.w, gtxt(extra))+int(2*k);
  }
  y += wrap(p, *f.small, cl.x, y, cl.w, gtxt("1 za poziom, u nauczycieli 5 punktów nauki = 1. LPM na węźle = punkt, PPM = cofnięcie punktu dodanego teraz; całe drzewka zmienia nauczyciel za złoto."))+int(14*k);

  // the note: on a sheet of the game's letters
  const Rect nr(cl.x-int(16*k), y, cl.w+int(32*k), cl.y+cl.h-y);
  letter(p, nr, 1.f);
  const int pad = int(30*k);
  const int nx  = nr.x+pad;
  const int nw  = nr.w-2*pad;
  int       ny  = nr.y+pad;
  std::string name, desc, branch;
  int  maxRank = 0, curRank = 0;
  bool isNode  = false;
  ArpgSkills::NodeId id{size_t(tree), 0, 0};
  switch(selKind) {
    case 1: {
      auto& br = t.branches[size_t(selB)];
      auto& nd = br.nodes[size_t(selN)];
      name    = nd.name;
      desc    = nd.desc;
      maxRank = nd.max;
      branch  = std::string("Gałąź: ")+br.name;
      id      = ArpgSkills::NodeId{size_t(tree), selB, selN};
      isNode  = true;
      break;
      }
    case 2:
      name    = t.keystone.name;
      desc    = t.keystone.desc;
      maxRank = t.keystone.max;
      branch  = std::string("Węzeł kluczowy - wymaga: ")+t.keyReqText;
      id      = ArpgSkills::NodeId{size_t(tree), -1, 0};
      isNode  = true;
      break;
    default:
      name   = t.name;
      desc   = t.root;
      branch = t.key[0]!='\0' ? std::string("Klawisz: ")+t.key : std::string("Działa zawsze (bez klawisza)");
      break;
    }
  if(isNode && pr!=nullptr)
    curRank = pr->rank(id);
  text(p, *f.sub, nx, ny, gtxt(name));
  if(maxRank>0) {
    char rank[16] = {};
    std::snprintf(rank, sizeof(rank), "%d / %d", curRank, maxRank);
    textRight(p, *f.red, nx+nw, ny+(f.sub->pixelSize()-f.red->pixelSize())/2, rank);
    }
  ny += f.sub->pixelSize()+int(6*k);
  ny += wrap(p, *f.redS, nx, ny, nw, gtxt(branch))+int(8*k);
  ny += wrap(p, *f.body, nx, ny, nw, gtxt(desc))+int(10*k);
  if(isNode && pr!=nullptr) {
    // what a click does now
    std::string why;
    if(pr->canAdd(id, hero, &why))
      ny += wrap(p, *f.ink, nx, ny, nw, gtxt("LPM: dodaj punkt"))+int(4*k);
    else if(curRank<maxRank)
      ny += wrap(p, *f.redS, nx, ny, nw, gtxt(why))+int(4*k);
    if(pr->canRemove(id, undoBase))
      ny += wrap(p, *f.ink, nx, ny, nw, gtxt("PPM: cofnij punkt"))+int(4*k);
    }
  if(selKind<=0)
    wrap(p, *f.small, nx, ny, nw, gtxt("Najedź na węzeł drzewka, żeby przeczytać jego opis."));
  text(p, *f.small, nx, nr.y+nr.h-pad-f.small->pixelSize(),
       gtxt(t.weapons ? "Biegłość działa od razu (procent Gothica)." : "Punkty działają od razu w walce."));
  }

void ArpgMenu::paintOverlay(Painter& p, float /*scale*/, Npc&) {
  // over the item models: how many, the key of each rune slot, the price in a trade
  if(page==P_Skills) {
    // skill nodes that cannot take a point yet: dimmed
    p.setBrush(Brush(Color(0.10f, 0.07f, 0.04f, 0.55f), Painter::Alpha));
    for(auto& r:lockedNodes)
      p.drawRect(r.x, r.y, r.w, r.h);
    return;
    }
  if(page!=P_Equip && page!=P_Trade)
    return;
  auto& fN = Resources::font("font_old_10_white.tga", Resources::FontType::Normal, 0.95f*k);
  auto& fD = Resources::font("font_old_10_white.tga", Resources::FontType::Hi,     0.85f*k);
  auto& fG = tinted(Resources::font("font_old_10_white.tga", Resources::FontType::Hi, 0.85f*k), 2, Color(1.f, 0.82f, 0.42f, 1.f));
  p.setBrush(Color(1.f));
  for(auto& h:hits) {
    if((h.kind==H_Pack || h.kind==H_Other) && h.price>0) {
      char txt[16] = {};
      std::snprintf(txt, sizeof(txt), "%d", int(h.price));
      text(p, fG, h.r.x+int(6*k), h.r.y+int(4*k), txt);
      }
    if((h.kind==H_Pack || h.kind==H_Other) && h.count>1) {
      char txt[16] = {};
      std::snprintf(txt, sizeof(txt), "%d", int(h.count));
      textRight(p, fN, h.r.x+h.r.w-int(6*k), h.r.y+h.r.h-fN.pixelSize()-int(5*k), txt);
      }
    if(h.kind==H_Key) {
      char txt[8] = {};
      std::snprintf(txt, sizeof(txt), "%d", int(h.count));
      text(p, fD, h.r.x+int(5*k), h.r.y+int(3*k), txt);
      }
    }
  }

// ---------------------------------------------------------------------------------------------------------------
// trade, chest, looting: the other's page on the left (a red book), ours on the right

void ArpgMenu::tradeMove(bool fromOther, size_t cls, int32_t count, bool equipped, Npc& pl) {
  if(invMenu==nullptr || cls==0)
    return;
  const auto st = invMenu->isOpen();
  if(!fromOther) {
    if(st==InventoryMenu::State::Ransack) {
      // looting a body: our own things work as in the equipment window
      if(equipped)
        pl.unequipItem(cls); else
        pl.useItem(cls);
      return;
      }
    if(equipped)
      return; // worn things are not sold or put away
    }
  invMenu->arpgMove(fromOther, cls, size_t(std::max<int32_t>(1, count)));
  }

void ArpgMenu::tradeDrop(const Point& pos, Npc& pl) {
  // dragged to the other page: the whole stack
  if(invMenu==nullptr)
    return;
  const bool fromOther = (drag.kind==H_Other);
  if(fromOther && pageR.contains(pos))
    tradeMove(true, drag.cls, drag.count, false, pl);
  else if(!fromOther && pageL.contains(pos) && invMenu->isOpen()!=InventoryMenu::State::Ransack)
    tradeMove(false, drag.cls, drag.count, drag.equipped, pl);
  }

void ArpgMenu::paintTrade(Painter& p, const Fonts& f, const Rect& cl, const Rect& cr, InventoryRenderer& items, Npc& pl) {
  if(invMenu==nullptr)
    return;
  const auto       st      = invMenu->isOpen();
  const bool       tradeOn = (st==InventoryMenu::State::Trade);
  const bool       chestOn = (st==InventoryMenu::State::Chest);
  Npc*             other   = invMenu->arpgOther();
  Interactive*     chest   = invMenu->arpgChest();
  const Inventory* oinv    = invMenu->arpgOtherInventory();
  auto             wrld    = Gothic::inst().world();
  const size_t     goldId  = [&]() -> size_t {
    if(wrld!=nullptr)
      if(auto g = wrld->script().goldId())
        return g->index();
    return size_t(-1);
    }();

  static const char* cats[8] = {"Wszystko","Broń","Pancerze","Magia","Mikstury","Jedzenie","Dokumenty","Inne"};

  auto pagePaint = [&](const Rect& c, bool otherPage) {
    int y = c.y;

    // whose page it is and what is done with it
    std::string title, caption;
    if(otherPage) {
      if(chestOn && chest!=nullptr)
        title = std::string(chest->displayName()); // names from the scripts: already in the game's code page
      else if(other!=nullptr)
        title = std::string(other->displayName());
      if(title.empty())
        title = gtxt(chestOn ? "Skrzynia" : "Handlarz");
      caption = gtxt(tradeOn ? "Handlarz - od niego kupujesz" :
                     (chestOn ? "Skrzynia - bierzesz z niej" : "Przeszukujesz - bierzesz"));
      } else {
      title   = gtxt("Bezimienny");
      caption = gtxt(tradeOn ? "Twój plecak - sprzedajesz" :
                     (chestOn ? "Twój plecak - odkładasz do skrzyni" : "Twój plecak"));
      }
    text(p, *f.title, c.x, y, title);
    const int gy = y+(f.title->pixelSize()-f.red->pixelSize())/2;
    if(otherPage && !tradeOn) {
      // take everything: a button on a sheet of the game's letters
      const std::string s  = gtxt("Zabierz wszystko");
      const int         bw = f.ink->textSize(s).w+int(44*k);
      const int         bh = int(42*k);
      const Rect        b(c.x+c.w-bw+int(10*k), y-int(6*k), bw, bh);
      const bool        hv = b.contains(mouse);
      letter(p, b, hv ? 1.f : 0.85f);
      auto& fn = hv ? *f.red : *f.ink;
      textCenter(p, fn, b.x+b.w/2, b.y+(b.h-fn.pixelSize())/2, s);
      Hit h; h.r = b; h.kind = H_TakeAll;
      hits.push_back(h);
      } else {
      const size_t      gold = otherPage ? (oinv!=nullptr ? oinv->goldCount() : 0) : pl.inventory().goldCount();
      const std::string v    = std::to_string(gold);
      textRight(p, *f.red, c.x+c.w, gy, v);
      textRight(p, *f.ink, c.x+c.w-f.red->textSize(v).w-int(8*k), gy, gtxt("Złoto:"));
      }
    y += f.title->pixelSize()+int(2*k);
    text(p, *f.redS, c.x, y, caption);
    y += f.redS->pixelSize()+int(6*k);
    drawRule(p, c.x, y, c.w);
    y += int(8*k);

    // kinds of things
    int& kcat = otherPage ? catL : cat;
    {
      const int cw = c.w/4;
      const int ch = f.body->pixelSize()+int(12*k);
      for(int i=0; i<8; ++i) {
        const Rect r(c.x+(i%4)*cw, y+(i/4)*ch, cw, ch);
        const bool on = (kcat==i), hv = r.contains(mouse);
        auto& fn = on ? *f.red : (hv ? *f.ink : *f.body);
        const std::string s = gtxt(cats[i]);
        textCenter(p, fn, r.x+r.w/2, r.y+(r.h-fn.pixelSize())/2, s);
        if(on) {
          const int tw = fn.textSize(s).w;
          fill(p, r.x+(r.w-tw)/2, r.y+r.h-int(4*k), tw, std::max(1,int(2*k)), LineRedC);
          }
        Hit h; h.r = r; h.kind = otherPage ? H_CatL : H_Cat; h.id = i;
        hits.push_back(h);
        }
      y += 2*ch+int(10*k);
    }

    // the things
    struct Cell { const Item* it; size_t cls; int32_t count; bool eq; int32_t price; };
    std::vector<Cell> list;
    auto add = [&](const Inventory& inv, Inventory::IteratorType type, bool skipGold) {
      for(auto it = inv.iterator(type); it.isValid(); ++it) {
        const Item& itm = *it;
        if(skipGold && itm.clsId()==goldId)
          continue;
        if(kcat!=0 && itemCategory(itm)!=kcat)
          continue;
        int32_t price = 0;
        if(tradeOn)
          price = otherPage ? inv.priceOf(itm.clsId()) : inv.sellPriceOf(itm.clsId());
        list.push_back({&itm, itm.clsId(), int32_t(it.count()), it.isEquipped(), price});
        }
      };
    if(otherPage) {
      if(oinv!=nullptr)
        add(*oinv, invMenu->arpgOtherIter(), tradeOn); // gold is loot in chests and on bodies
      } else {
      add(pl.inventory(), Inventory::T_Inventory, true);
      }

    const int infoMin = int(190*k);
    const int cols    = 6;
    const int gap     = int(5*k);
    const int cs      = (c.w-(cols-1)*gap)/cols;
    const int ind     = f.small->pixelSize()+int(4*k);
    const int rows    = std::max(1, (c.y+c.h-infoMin-int(10*k)-ind-y+gap)/(cs+gap));
    int&      scr     = otherPage ? scrollL    : scroll;
    int&      maxScr  = otherPage ? maxScrollL : maxScroll;
    const int totalRows = int((list.size()+cols-1)/cols);
    maxScr = std::max(0, totalRows-rows);
    scr    = std::clamp(scr, 0, maxScr);

    const Cell* hover = nullptr;
    for(int i=0; i<rows*cols; ++i) {
      const Rect   r(c.x+(i%cols)*(cs+gap), y+(i/cols)*(cs+gap), cs, cs);
      const size_t id = size_t(i+scr*cols);
      if(id>=list.size()) {
        drawSlot(p, r, false, false);
        continue;
        }
      auto&      cell = list[id];
      const bool hv   = r.contains(mouse);
      drawSlot(p, r, cell.eq, hv);
      items.drawItem(r.x+cs/8, r.y+cs/8, cs-cs/4, cs-cs/4, *cell.it);
      Hit h; h.r = r; h.kind = otherPage ? H_Other : H_Pack; h.cls = cell.cls; h.equipped = cell.eq;
      h.count = cell.count; h.price = cell.price;
      hits.push_back(h);
      if(hv)
        hover = &cell;
      }
    if(maxScr>0) {
      char txt[64] = {};
      std::snprintf(txt, sizeof(txt), "%s %d / %d", gtxt("kółko myszy - strona").c_str(), scr+1, maxScr+1);
      textRight(p, *f.small, c.x+c.w, y+rows*(cs+gap), txt);
      }

    // the note: the thing under the cursor, with its price
    std::string extra;
    if(hover!=nullptr && tradeOn) {
      char buf[96] = {};
      if(otherPage)
        std::snprintf(buf, sizeof(buf), gtxt("Cena: %d złota").c_str(), int(hover->price));
      else if(hover->eq)
        std::snprintf(buf, sizeof(buf), "%s", gtxt("Założone - zdejmij, żeby sprzedać").c_str());
      else
        std::snprintf(buf, sizeof(buf), gtxt("Dostaniesz: %d złota").c_str(), int(hover->price));
      extra = buf;
      }
    std::string help;
    if(otherPage)
      help = gtxt(tradeOn ? "Kliknij, żeby kupić jedną sztukę.\nShift+klik albo PPM: cały stos.\nAlbo przeciągnij na swoją stronę." :
                            "Kliknij, żeby zabrać.\nAlbo przeciągnij na swoją stronę.");
    else
      help = gtxt(tradeOn ? "Kliknij, żeby sprzedać jedną sztukę.\nShift+klik albo PPM: cały stos.\nAlbo przeciągnij na stronę handlarza." :
                 (chestOn ? "Kliknij, żeby odłożyć jedną sztukę.\nShift+klik albo PPM: cały stos." :
                            "Kliknij: załóż albo użyj, jak w ekwipunku."));
    const int infoY = y+rows*(cs+gap)+ind+int(6*k);
    paintInfo(p, f, Rect(c.x-int(14*k), infoY, c.w+int(28*k), c.y+c.h-infoY), items,
              hover!=nullptr ? hover->it : nullptr, extra, help);
    if(hover!=nullptr)
      hoverCls = hover->cls;
    };

  pagePaint(cl, true);
  pagePaint(cr, false);
  }

#include "arpgworldmap.h"

#include <Tempest/Application>
#include <Tempest/Pixmap>

#include <algorithm>
#include <cmath>
#include <functional>

#include "game/arpgmapdata.h"
#include "game/gamesession.h"
#include "game/inventory.h"
#include "game/playercontrol.h"
#include "ui/arpgmenu.h"
#include "utils/gthfont.h"
#include "world/objects/npc.h"
#include "world/objects/item.h"
#include "world/world.h"
#include "camera.h"
#include "gothic.h"
#include "resources.h"

using namespace Tempest;

static constexpr float Pi = 3.14159265f;

ArpgWorldMap::ArpgWorldMap()  = default;
ArpgWorldMap::~ArpgWorldMap() = default;

ArpgMapData* ArpgWorldMap::data() const {
  auto gs = Gothic::inst().gameSession();
  return gs!=nullptr ? &gs->arpgMap() : nullptr;
  }

void ArpgWorldMap::tick(World& world, Npc& hero, float sight) {
  if(auto d = data()) {
    const auto p = hero.position();
    d->explore(world.name(), p.x, p.z, sight);
    }
  }

void ArpgWorldMap::setOpen(bool o) {
  if(open==o)
    return;
  open     = o;
  dragging = false;
  editPin  = -1;
  layersAt = 0; // read the maps of the hero again
  if(open) {
    zoom = 1.f;
    pan  = Vec2(0.5f,0.5f);
    }
  }

// ---------------------------------------------------------------------------------------------------------------
// layers and markers

void ArpgWorldMap::updateLayers(World& world, Npc& hero) {
  const uint64_t now = Application::tickCount();
  if(worldName==world.name() && layersAt!=0 && now<layersAt+500)
    return;
  if(worldName!=world.name())
    layer = 0;
  worldName = std::string(world.name());
  layersAt  = now;

  std::string prev = layer<layers.size() ? layers[layer].texture : "";
  layers.clear();
  for(auto& m:world.script().arpgAllMaps()) {
    if(m.level!=worldName || m.right==m.left || m.bottom==m.top)
      continue;
    Layer l;
    l.texture = m.texture;
    l.left    = float(m.left);
    l.top     = float(m.top);
    l.right   = float(m.right);
    l.bottom  = float(m.bottom);
    l.whole   = m.whole;
    for(auto it = hero.inventory().iterator(Inventory::T_Inventory); it.isValid(); ++it) {
      if(m.fn!=uint32_t(-1) && it->handle().on_state[0]==int32_t(m.fn)) {
        l.owned = true;
        l.title = it->handle().name;
        break;
        }
      }
    if(!l.whole && !l.owned)
      continue; // a city or treasure map the hero does not have
    if(l.title.empty())
      l.title = ArpgMenu::gtxt(l.whole ? "Mapa (szkic)" : "Mapa");
    bool dup = false;
    for(auto& o:layers)
      dup |= (o.texture==l.texture);
    if(!dup)
      layers.push_back(std::move(l));
    }
  std::stable_sort(layers.begin(), layers.end(), [](const Layer& a, const Layer& b) { return a.whole && !b.whole; });
  layer = 0;
  for(size_t i=0; i<layers.size(); ++i)
    if(layers[i].texture==prev)
      layer = i;
  }

void ArpgWorldMap::updateMarkers(World& world) {
  const uint64_t now = Application::tickCount();
  if(markersAt!=0 && now<markersAt+1000 && markersWorld==world.name())
    return;
  markersWorld = std::string(world.name());
  markersAt = now;
  markers.clear();

  auto& script = world.script();
  auto& topics = script.arpgMarkerTopics();
  auto& texts  = script.arpgLogNpcs();
  auto& log    = script.questLog();

  auto add = [&](const Npc* npc, bool teacher) {
    const uint32_t id = npc->instanceSymbol();
    for(auto& m:markers)
      if(m.instance==id) {
        (teacher ? m.teacher : m.trader) = true;
        return;
        }
    Marker m;
    m.instance = id;
    (teacher ? m.teacher : m.trader) = true;
    markers.push_back(m);
    };

  for(size_t q=0; q<log.questCount(); ++q) {
    auto& quest = log.quest(q);
    auto  tp    = topics.find(quest.name);
    if(tp==topics.end())
      continue;
    for(auto& entry:quest.entry) {
      auto it = texts.find(entry);
      if(it==texts.end())
        continue;
      // the dialog's NPC, unless the entry names somebody else (e.g. a pirate telling about Garett)
      const Npc* npc = world.findNpcByInstance(it->second.npc);
      if(npc!=nullptr && entry.find(npc->displayName())!=std::string::npos) {
        add(npc, it->second.teacher);
        continue;
        }
      const Npc* other = nullptr;
      for(uint32_t i=0; i<world.npcCount() && other==nullptr; ++i) {
        auto n = world.npcById(i);
        if(n==nullptr || n==npc || n->isDead() || n->isPlayer())
          continue;
        auto nm = n->displayName();
        if(nm.size()>=4 && entry.find(nm)!=std::string::npos)
          other = n;
        }
      if(other!=nullptr)
        add(other, it->second.teacher);
      else if(npc!=nullptr)
        add(npc, it->second.teacher);
      }
    }
  }

const Texture2d* ArpgWorldMap::sketchOf(const std::string& texture, bool inkOnly) {
  auto& s = sketches[texture];
  if(s.tried) {
    auto& t = inkOnly ? s.ink : s.tex;
    return t.isEmpty() ? nullptr : &t;
    }
  s.tried = true;

  // the painted map as the hero's ink sketch: edges of the (slightly blurred) picture in dark ink on pale paper
  Pixmap src = Resources::loadPixmapRgba(texture);
  if(src.isEmpty())
    return nullptr;
  const int w = int(src.w()), h = int(src.h());
  const uint8_t* in = reinterpret_cast<const uint8_t*>(src.data());
  std::vector<float> lum(size_t(w*h)), blur(size_t(w*h));
  for(int i=0; i<w*h; ++i)
    lum[size_t(i)] = (0.3f*float(in[4*i]) + 0.59f*float(in[4*i+1]) + 0.11f*float(in[4*i+2]))/255.f;
  auto at = [&](const std::vector<float>& v, int x, int y) {
    x = std::clamp(x,0,w-1);
    y = std::clamp(y,0,h-1);
    return v[size_t(y*w+x)];
    };
  for(int y=0; y<h; ++y)
    for(int x=0; x<w; ++x) {
      float sum = 0;
      for(int j=-1; j<=1; ++j)
        for(int i=-1; i<=1; ++i)
          sum += at(lum,x+i,y+j);
      blur[size_t(y*w+x)] = sum/9.f;
      }

  Pixmap out(uint32_t(w), uint32_t(h), TextureFormat::RGBA8);
  Pixmap lines(uint32_t(w), uint32_t(h), TextureFormat::RGBA8);
  uint8_t* o = reinterpret_cast<uint8_t*>(out.data());
  uint8_t* q = reinterpret_cast<uint8_t*>(lines.data());
  const float paper[3] = {0.86f, 0.78f, 0.60f};
  const float ink[3]   = {0.20f, 0.14f, 0.09f};
  for(int y=0; y<h; ++y)
    for(int x=0; x<w; ++x) {
      const float gx = -at(blur,x-1,y-1) - 2*at(blur,x-1,y) - at(blur,x-1,y+1)
                       +at(blur,x+1,y-1) + 2*at(blur,x+1,y) + at(blur,x+1,y+1);
      const float gy = -at(blur,x-1,y-1) - 2*at(blur,x,y-1) - at(blur,x+1,y-1)
                       +at(blur,x-1,y+1) + 2*at(blur,x,y+1) + at(blur,x+1,y+1);
      const float g     = std::sqrt(gx*gx+gy*gy);
      const float line  = std::clamp((g-0.08f)*4.f, 0.f, 1.f);
      const float shade = std::clamp((0.55f-at(blur,x,y))*0.5f, 0.f, 0.25f); // darker land = light hatching
      const size_t i = size_t(y*w+x)*4;
      for(int c=0; c<3; ++c) {
        float v = paper[c]*(1.f-shade);
        v = v*(1.f-0.85f*line) + ink[c]*0.85f*line;
        o[i+size_t(c)] = uint8_t(std::clamp(v,0.f,1.f)*255.f);
        q[i+size_t(c)] = 255;
        }
      o[i+3] = 255;
      q[i+3] = uint8_t(std::clamp(line*1.2f,0.f,1.f)*255.f);
      }
  s.tex = Resources::loadTexturePm(out);
  s.ink = Resources::loadTexturePm(lines);
  auto& t = inkOnly ? s.ink : s.tex;
  return t.isEmpty() ? nullptr : &t;
  }

const Texture2d* ArpgWorldMap::wholeMap(World& world, Npc& hero) {
  updateLayers(world, hero);
  for(auto& l:layers)
    if(l.whole)
      return l.owned ? Resources::loadTexture(l.texture) : sketchOf(l.texture);
  return nullptr;
  }

// ---------------------------------------------------------------------------------------------------------------
// geometry

Rect ArpgWorldMap::mapRect(const Ctx& c, const Texture2d& tex) const {
  const float top    = 70.f*c.scale, bottom = 44.f*c.scale;
  const float availW = float(c.w)*0.94f, availH = float(c.h)-top-bottom;
  const float aspect = float(tex.w())/float(std::max(1,tex.h()));
  float W = availW, H = W/aspect;
  if(H>availH) {
    H = availH;
    W = H*aspect;
    }
  W *= zoom;
  H *= zoom;
  const float cx = float(c.w)*0.5f, cy = top + availH*0.5f;
  return Rect(int(cx - pan.x*W), int(cy - pan.y*H), int(W), int(H));
  }

Vec2 ArpgWorldMap::toMap(const Layer& l, const Rect& r, float wx, float wz) const {
  const float u = (wx-l.left)/(l.right-l.left);
  const float v = (wz-l.top)/(l.bottom-l.top);
  return Vec2(float(r.x)+u*float(r.w), float(r.y)+v*float(r.h));
  }

Vec2 ArpgWorldMap::toWorld(const Layer& l, const Rect& r, float sx, float sy) const {
  const float u = (sx-float(r.x))/float(std::max(1,r.w));
  const float v = (sy-float(r.y))/float(std::max(1,r.h));
  return Vec2(l.left + u*(l.right-l.left), l.top + v*(l.bottom-l.top));
  }

void ArpgWorldMap::drawExplored(Painter& p, const Layer& l, const Texture2d& tex,
                                const std::function<Vec2(float,float)>& toScreen,
                                float x0, float z0, float x1, float z1, const Color& cl, bool both) const {
  auto d = data();
  if(d==nullptr)
    return;
  auto set = d->explored(worldName);
  if(set==nullptr || set->empty())
    return;
  constexpr float cell = ArpgMapData::Cell;
  const float tw = float(tex.w()), th = float(tex.h());
  auto uv = [&](float wx, float wz) {
    return Vec2((wx-l.left)/(l.right-l.left)*tw, (wz-l.top)/(l.bottom-l.top)*th);
    };
  p.setBrush(Brush(tex, cl, Painter::Alpha, ClampMode::ClampToEdge));

  const int i0 = int(std::floor(std::min(x0,x1)/cell)), i1 = int(std::floor(std::max(x0,x1)/cell));
  const int j0 = int(std::floor(std::min(z0,z1)/cell)), j1 = int(std::floor(std::max(z0,z1)/cell));
  for(int j=j0; j<=j1; ++j) {
    int i = i0;
    while(i<=i1) {
      if(set->find(ArpgMapData::key(i,j))==set->end()) {
        ++i;
        continue;
        }
      int e = i;
      while(e+1<=i1 && set->find(ArpgMapData::key(e+1,j))!=set->end())
        ++e;
      // explored cells i..e of row j: one strip
      const float ax = float(i)*cell, bx = float(e+1)*cell, az = float(j)*cell, bz = float(j+1)*cell;
      const Vec2 s0 = toScreen(ax,az), s1 = toScreen(bx,az), s2 = toScreen(bx,bz), s3 = toScreen(ax,bz);
      const Vec2 t0 = uv(ax,az),       t1 = uv(bx,az),       t2 = uv(bx,bz),       t3 = uv(ax,bz);
      p.drawTriangle(s0.x,s0.y,t0.x,t0.y, s1.x,s1.y,t1.x,t1.y, s2.x,s2.y,t2.x,t2.y);
      p.drawTriangle(s0.x,s0.y,t0.x,t0.y, s2.x,s2.y,t2.x,t2.y, s3.x,s3.y,t3.x,t3.y);
      if(both) { // the interface draws one winding only (Tempest: CullMode::Back)
        p.drawTriangle(s0.x,s0.y,t0.x,t0.y, s2.x,s2.y,t2.x,t2.y, s1.x,s1.y,t1.x,t1.y);
        p.drawTriangle(s0.x,s0.y,t0.x,t0.y, s3.x,s3.y,t3.x,t3.y, s2.x,s2.y,t2.x,t2.y);
        }
      i = e+1;
      }
    }
  }

void ArpgWorldMap::drawHero(Painter& p, const Vec2& at, const Vec2& dir, float scale) const {
  Vec2 hd = dir;
  const float l = std::sqrt(hd.x*hd.x+hd.y*hd.y);
  hd = (l>0.f) ? hd/l : Vec2(0,-1);
  const Vec2  pr  = {-hd.y, hd.x};
  const float a   = 11.f*scale, b = 7.f*scale;
  const Vec2  tip = at + hd*a;
  const Vec2  b0  = at - hd*b + pr*b;
  const Vec2  b1  = at - hd*b - pr*b;
  p.setBrush(Brush(Color(0.10f,0.06f,0.03f,1.f), Painter::Alpha));
  const Vec2 o = Vec2(1.5f,1.5f)*scale;
  p.drawTriangle(tip.x+o.x,tip.y+o.y,0,0, b0.x+o.x,b0.y+o.y,0,0, b1.x+o.x,b1.y+o.y,0,0);
  p.drawTriangle(tip.x+o.x,tip.y+o.y,0,0, b1.x+o.x,b1.y+o.y,0,0, b0.x+o.x,b0.y+o.y,0,0);
  p.setBrush(Brush(Color(1.f,0.8f,0.25f,1.f), Painter::Alpha));
  p.drawTriangle(tip.x,tip.y,0,0, b0.x,b0.y,0,0, b1.x,b1.y,0,0);
  p.drawTriangle(tip.x,tip.y,0,0, b1.x,b1.y,0,0, b0.x,b0.y,0,0);
  }

// a small diamond (both windings)
static void diamond(Painter& p, float cx, float cy, float r, const Color& cl) {
  p.setBrush(Brush(Color(0.08f,0.05f,0.02f,0.9f), Painter::Alpha));
  const float R = r+1.5f;
  p.drawTriangle(cx,cy-R,0,0, cx+R,cy,0,0, cx,cy+R,0,0);  p.drawTriangle(cx,cy-R,0,0, cx,cy+R,0,0, cx+R,cy,0,0);
  p.drawTriangle(cx,cy-R,0,0, cx,cy+R,0,0, cx-R,cy,0,0);  p.drawTriangle(cx,cy-R,0,0, cx-R,cy,0,0, cx,cy+R,0,0);
  p.setBrush(Brush(cl, Painter::Alpha));
  p.drawTriangle(cx,cy-r,0,0, cx+r,cy,0,0, cx,cy+r,0,0);  p.drawTriangle(cx,cy-r,0,0, cx,cy+r,0,0, cx+r,cy,0,0);
  p.drawTriangle(cx,cy-r,0,0, cx,cy+r,0,0, cx-r,cy,0,0);  p.drawTriangle(cx,cy-r,0,0, cx-r,cy,0,0, cx,cy+r,0,0);
  }

static void pinShape(Painter& p, float cx, float cy, float s, bool hot) {
  // a needle with a red head; the point of the needle is the place
  p.setBrush(Brush(Color(0.15f,0.12f,0.10f,1.f), Painter::Alpha));
  p.drawRect(int(cx), int(cy-9.f*s), std::max(1,int(1.5f*s)), int(9.f*s));
  const float r = (hot ? 4.5f : 3.5f)*s;
  p.setBrush(Brush(hot ? Color(1.f,0.35f,0.25f,1.f) : Color(0.80f,0.12f,0.08f,1.f), Painter::Alpha));
  p.drawRect(int(cx-r+0.5f*s), int(cy-9.f*s-r), int(2.f*r), int(2.f*r));
  }

// ---------------------------------------------------------------------------------------------------------------
// painting

void ArpgWorldMap::paint(Painter& p, const Ctx& c) {
  if(!open)
    return;
  updateLayers(c.world, c.hero);
  updateMarkers(c.world);
  tabRects.clear();
  lastRect = Rect();

  const float s   = c.scale;
  lastScale = s;
  auto        fnt = Resources::font(s);
  auto        big = Resources::font(Resources::FontType::Normal, s*1.4f);

  p.setBrush(Brush(Color(0.02f,0.015f,0.01f,0.88f), Painter::Alpha));
  p.drawRect(0, 0, c.w, c.h);

  if(layers.empty()) {
    const std::string txt = ArpgMenu::gtxt("Nie ma mapy tego miejsca");
    auto ts = big.textSize(txt);
    big.drawText(p, (c.w-ts.w)/2, c.h/2, txt);
    return;
    }
  if(layer>=layers.size())
    layer = 0;
  const Layer& L = layers[layer];
  const Texture2d* tex = L.owned ? Resources::loadTexture(L.texture) : sketchOf(L.texture);
  if(tex==nullptr || tex->isEmpty()) {
    const std::string txt = ArpgMenu::gtxt("Nie da się wczytać mapy");
    auto ts = big.textSize(txt);
    big.drawText(p, (c.w-ts.w)/2, c.h/2, txt);
    return;
    }
  const Rect r = mapRect(c, *tex);
  lastRect = r;

  if(L.owned) {
    p.setBrush(Brush(*tex, Painter::Alpha));
    p.drawRect(r.x, r.y, r.w, r.h, 0, 0, tex->w(), tex->h());
    } else {
    // empty parchment, the explored land sketched on it
    if(auto paper = Resources::loadTexture("LETTERS.TGA")) {
      p.setBrush(Brush(*paper, Painter::Alpha));
      p.drawRect(r.x, r.y, r.w, r.h, 0, 0, paper->w(), paper->h());
      } else {
      p.setBrush(Brush(Color(0.86f,0.78f,0.60f,1.f), Painter::Alpha));
      p.drawRect(r.x, r.y, r.w, r.h);
      }
    auto toScreen = [&](float wx, float wz) { return toMap(L, r, wx, wz); };
    drawExplored(p, L, *tex, toScreen, L.left, L.top, L.right, L.bottom, Color(1.f,1.f,1.f,1.f), true);
    }

  // teachers and traders the diary knows
  const Npc*    hotNpc  = nullptr;
  const Marker* hotMark = nullptr;
  int           hotPin  = -1;
  const float   hit     = 9.f*s;
  for(auto& m:markers) {
    const Npc* npc = c.world.findNpcByInstance(m.instance);
    if(npc==nullptr || npc->isDead())
      continue;
    const auto pos = npc->position();
    const Vec2 sp  = toMap(L, r, pos.x, pos.z);
    if(sp.x<0 || sp.y<0 || sp.x>float(c.w) || sp.y>float(c.h))
      continue;
    const float rr = 5.f*s;
    if(m.trader && m.teacher) {
      diamond(p, sp.x-rr*0.7f, sp.y, rr, Color(0.95f,0.75f,0.20f,1.f));
      diamond(p, sp.x+rr*0.7f, sp.y, rr, Color(0.45f,0.68f,0.98f,1.f));
      } else {
      diamond(p, sp.x, sp.y, rr, m.trader ? Color(0.95f,0.75f,0.20f,1.f) : Color(0.45f,0.68f,0.98f,1.f));
      }
    if(std::abs(float(mouse.x)-sp.x)<hit && std::abs(float(mouse.y)-sp.y)<hit) {
      hotNpc  = npc;
      hotMark = &m;
      }
    }

  // pins
  if(auto d = data()) {
    auto& pins = d->pins(worldName);
    for(size_t i=0; i<pins.size(); ++i) {
      const Vec2 sp = toMap(L, r, pins[i].x, pins[i].z);
      if(std::abs(float(mouse.x)-sp.x)<hit && float(mouse.y)>sp.y-14.f*s && float(mouse.y)<sp.y+3.f*s)
        hotPin = int(i);
      pinShape(p, sp.x, sp.y, s, hotPin==int(i) || editPin==int(i));
      }
    }

  // the hero
  {
    const auto  pos = c.hero.position();
    const float rot = c.hero.rotation()*Pi/180.f;
    const Vec2  a   = toMap(L, r, pos.x, pos.z);
    const Vec2  b   = toMap(L, r, pos.x+std::cos(rot)*100.f, pos.z+std::sin(rot)*100.f);
    drawHero(p, a, b-a, s);
  }

  // tooltip: marker / pin
  auto tooltip = [&](const std::string& txt) {
    if(txt.empty())
      return;
    auto ts = fnt.textSize(txt);
    const int x = std::min(mouse.x+int(14.f*s), c.w-ts.w-int(10.f*s));
    const int y = mouse.y+int(22.f*s);
    p.setBrush(Brush(Color(0.f,0.f,0.f,0.75f), Painter::Alpha));
    p.drawRect(x-int(6.f*s), y-ts.h-int(4.f*s), ts.w+int(12.f*s), ts.h+int(10.f*s));
    fnt.drawText(p, x, y, txt);
    };
  if(editPin>=0) {
    if(auto d = data()) {
      auto& pins = d->pins(worldName);
      if(size_t(editPin)<pins.size()) {
        const Vec2 sp = toMap(L, r, pins[size_t(editPin)].x, pins[size_t(editPin)].z);
        const std::string txt = ArpgMenu::gtxt(pins[size_t(editPin)].note+"_");
        auto ts = fnt.textSize(txt);
        p.setBrush(Brush(Color(0.f,0.f,0.f,0.8f), Painter::Alpha));
        p.drawRect(int(sp.x)+int(8.f*s), int(sp.y)-ts.h-int(14.f*s), ts.w+int(12.f*s), ts.h+int(10.f*s));
        fnt.drawText(p, int(sp.x)+int(14.f*s), int(sp.y)-int(10.f*s), txt);
        }
      }
    }
  else if(hotPin>=0) {
    if(auto d = data()) {
      auto& pin = d->pins(worldName)[size_t(hotPin)];
      tooltip(ArpgMenu::gtxt(pin.note.empty() ? std::string("(bez opisu)") : pin.note));
      }
    }
  else if(hotNpc!=nullptr && hotMark!=nullptr) {
    std::string txt = std::string(hotNpc->displayName());
    auto& m = *hotMark;
    txt += ArpgMenu::gtxt(m.trader && m.teacher ? " - handlarz, nauczyciel" : (m.trader ? " - handlarz" : " - nauczyciel"));
    if(hotNpc->hasState(BS_LIE))
      txt += ArpgMenu::gtxt(" (śpi)");
    tooltip(txt);
    }

  // tabs: the maps of this place the hero has
  {
    int x = int(20.f*s);
    const int y = int(16.f*s), th = int(34.f*s);
    for(size_t i=0; i<layers.size(); ++i) {
      const auto& t  = layers[i].title;
      const int   tw = fnt.textSize(t).w + int(24.f*s);
      const Rect  tr(x, y, tw, th);
      tabRects.push_back(tr);
      const bool sel = (i==layer);
      const bool hov = (mouse.x>=tr.x && mouse.x<tr.x+tr.w && mouse.y>=tr.y && mouse.y<tr.y+tr.h);
      p.setBrush(Brush(sel ? Color(0.55f,0.42f,0.22f,0.95f) : Color(0.18f,0.14f,0.10f,hov ? 0.95f : 0.8f), Painter::Alpha));
      p.drawRect(tr.x, tr.y, tr.w, tr.h);
      fnt.drawText(p, tr.x+int(12.f*s), tr.y+th/2+fnt.pixelSize()/3, t);
      x += tw + int(8.f*s);
      }
  }

  // help
  {
    const std::string txt = ArpgMenu::gtxt(editPin>=0
      ? "Pisz opis pinezki, Enter: gotowe"
      : "LPM: przesuń   Kółko: przybliż   PPM: pinezka (na pinezce: usuń)   LPM na pinezce: opis   M / Esc: zamknij");
    auto ts = fnt.textSize(txt);
    fnt.drawText(p, (c.w-ts.w)/2, c.h-int(16.f*s), txt);
  }
  }

void ArpgWorldMap::paintOverlay(Painter& p, const Ctx& c) {
  updateLayers(c.world, c.hero);
  updateMarkers(c.world);
  if(layers.empty())
    return;
  const Layer& L = layers[0];                    // the map of the whole place
  const Texture2d* tex = sketchOf(L.texture, true); // only its lines, light, like the lines of Diablo II
  if(tex==nullptr || tex->isEmpty())
    return;

  const auto  hero = c.hero.position();
  const float half = 4500.f;                     // cm from the hero to the top of the screen
  const float k    = (float(c.h)*0.5f)/half;
  const float cx   = float(c.w)*0.5f, cy = float(c.h)*0.5f;
  // like the minimap: screen up = where the camera looks
  const float yaw = c.camera.arpgYaw()*Pi/180.f;
  const Vec2  f   = {std::cos(yaw),  std::sin(yaw)};
  const Vec2  rt  = {std::sin(yaw), -std::cos(yaw)};
  auto toScreen = [&](float wx, float wz) {
    const float dx = wx-hero.x, dz = wz-hero.z;
    return Vec2(cx + k*(dx*rt.x + dz*rt.y), cy - k*(dx*f.x + dz*f.y));
    };
  const float reach = half*1.2f*std::max(1.f, float(c.w)/float(std::max(1,c.h)));
  drawExplored(p, L, *tex, toScreen, hero.x-reach, hero.z-reach, hero.x+reach, hero.z+reach,
               Color(1.f,0.86f,0.55f,0.85f), true);

  const float s = c.scale;
  for(auto& m:markers) {
    const Npc* npc = c.world.findNpcByInstance(m.instance);
    if(npc==nullptr || npc->isDead())
      continue;
    const auto pos = npc->position();
    const Vec2 sp  = toScreen(pos.x, pos.z);
    if(sp.x<0 || sp.y<0 || sp.x>float(c.w) || sp.y>float(c.h))
      continue;
    diamond(p, sp.x, sp.y, 4.f*s, m.trader ? Color(0.95f,0.75f,0.20f,0.9f) : Color(0.45f,0.68f,0.98f,0.9f));
    }
  if(auto d = data()) {
    for(auto& pin:d->pins(worldName)) {
      const Vec2 sp = toScreen(pin.x, pin.z);
      if(sp.x>=0 && sp.y>=0 && sp.x<=float(c.w) && sp.y<=float(c.h))
        pinShape(p, sp.x, sp.y, s, false);
      }
    }
  const float rot = c.hero.rotation()*Pi/180.f;
  const Vec2  a   = toScreen(hero.x, hero.z);
  const Vec2  b   = toScreen(hero.x+std::cos(rot)*100.f, hero.z+std::sin(rot)*100.f);
  drawHero(p, a, b-a, s);
  }

// ---------------------------------------------------------------------------------------------------------------
// input

bool ArpgWorldMap::mouseDown(MouseEvent& e) {
  if(!open)
    return false;
  mouse = e.pos();
  auto d = data();

  for(size_t i=0; i<tabRects.size(); ++i) {
    auto& t = tabRects[i];
    if(e.button==Event::ButtonLeft && mouse.x>=t.x && mouse.x<t.x+t.w && mouse.y>=t.y && mouse.y<t.y+t.h) {
      layer   = i;
      editPin = -1;
      return true;
      }
    }
  if(layers.empty() || layer>=layers.size() || lastRect.w<=0 || d==nullptr)
    return true;
  const Layer& L    = layers[layer];
  auto&        pins = d->pins(worldName);
  const float  s    = lastScale;
  int hotPin = -1;
  for(size_t i=0; i<pins.size(); ++i) {
    const Vec2 sp = toMap(L, lastRect, pins[i].x, pins[i].z);
    if(std::abs(float(mouse.x)-sp.x)<9.f*s && float(mouse.y)>sp.y-14.f*s && float(mouse.y)<sp.y+3.f*s)
      hotPin = int(i);
    }

  if(e.button==Event::ButtonRight) {
    if(hotPin>=0) {
      pins.erase(pins.begin()+hotPin);
      editPin = -1;
      return true;
      }
    if(mouse.x>=lastRect.x && mouse.x<lastRect.x+lastRect.w && mouse.y>=lastRect.y && mouse.y<lastRect.y+lastRect.h) {
      const Vec2 w = toWorld(L, lastRect, float(mouse.x), float(mouse.y));
      ArpgMapData::Pin pin;
      pin.x = w.x;
      pin.z = w.y;
      pins.push_back(pin);
      editPin = int(pins.size())-1;
      Gothic::inst().emitGlobalSound(Gothic::inst().loadSoundFx("MENU_SELECT"));
      }
    return true;
    }
  if(e.button==Event::ButtonLeft) {
    if(hotPin>=0) {
      editPin = hotPin;
      return true;
      }
    editPin  = -1;
    dragging = true;
    dragFrom = mouse;
    panFrom  = pan;
    }
  return true;
  }

bool ArpgWorldMap::mouseUp(MouseEvent& e) {
  if(!open)
    return false;
  mouse    = e.pos();
  dragging = false;
  return true;
  }

bool ArpgWorldMap::mouseMove(MouseEvent& e) {
  if(!open)
    return false;
  mouse = e.pos();
  if(dragging && lastRect.w>0 && lastRect.h>0) {
    pan.x = panFrom.x - float(mouse.x-dragFrom.x)/float(lastRect.w);
    pan.y = panFrom.y - float(mouse.y-dragFrom.y)/float(lastRect.h);
    pan.x = std::clamp(pan.x, 0.f, 1.f);
    pan.y = std::clamp(pan.y, 0.f, 1.f);
    }
  return true;
  }

bool ArpgWorldMap::mouseWheel(MouseEvent& e) {
  if(!open)
    return false;
  if(lastRect.w<=0 || lastRect.h<=0)
    return true;
  // zoom around the point under the mouse
  const float nz = std::clamp(zoom*(e.delta>0 ? 1.25f : 0.8f), 1.f, 6.f);
  if(nz==zoom)
    return true;
  const float mx = (float(mouse.x)-float(lastRect.x))/float(lastRect.w);
  const float my = (float(mouse.y)-float(lastRect.y))/float(lastRect.h);
  const float W  = float(lastRect.w)/zoom*nz, H = float(lastRect.h)/zoom*nz;
  // new rect: x = mouse.x - mx*W; the center of the screen shows pan: x = cx - pan.x*W
  const float cx = float(lastRect.x) + pan.x*float(lastRect.w);
  const float cy = float(lastRect.y) + pan.y*float(lastRect.h);
  pan.x = std::clamp((cx - (float(mouse.x) - mx*W))/W, 0.f, 1.f);
  pan.y = std::clamp((cy - (float(mouse.y) - my*H))/H, 0.f, 1.f);
  zoom  = nz;
  if(zoom<=1.f)
    pan = Vec2(0.5f,0.5f);
  return true;
  }

bool ArpgWorldMap::keyDown(KeyEvent& e) {
  if(!open)
    return false;
  if(editPin<0)
    return false;
  auto d = data();
  if(d==nullptr) {
    editPin = -1;
    return true;
    }
  auto& pins = d->pins(worldName);
  if(size_t(editPin)>=pins.size()) {
    editPin = -1;
    return true;
    }
  auto& note = pins[size_t(editPin)].note;
  if(e.key==Event::K_Return || e.key==Event::K_ESCAPE) {
    editPin = -1;
    return true;
    }
  if(e.key==Event::K_Back) {
    // remove the last UTF-8 character
    while(!note.empty() && (uint8_t(note.back())&0xC0)==0x80)
      note.pop_back();
    if(!note.empty())
      note.pop_back();
    return true;
    }
  const uint32_t ch = e.code;
  if(ch>=32 && ch!=127 && note.size()<80) {
    if(ch<0x80) {
      note.push_back(char(ch));
      } else if(ch<0x800) {
      note.push_back(char(0xC0 | (ch>>6)));
      note.push_back(char(0x80 | (ch&0x3F)));
      } else {
      note.push_back(char(0xE0 | (ch>>12)));
      note.push_back(char(0x80 | ((ch>>6)&0x3F)));
      note.push_back(char(0x80 | (ch&0x3F)));
      }
    }
  return true;
  }

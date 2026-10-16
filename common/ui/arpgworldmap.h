#pragma once

#include <Tempest/Painter>
#include <Tempest/Event>
#include <Tempest/Texture2d>
#include <Tempest/Point>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class World;
class Npc;
class Camera;
class PlayerControl;
class ArpgMapData;

// Diathic: the big map. M = parchment over the whole screen (zoom, drag, layers of the maps the hero owns, pins with
// notes, teachers and traders from the diary); the overlay (Tab, after the minimap) = the explored map drawn over the
// game around the hero like in Diablo II. Without the map of the level the explored land is the hero's own ink sketch.
class ArpgWorldMap final {
  public:
    ArpgWorldMap();
    ~ArpgWorldMap();

    struct Ctx {
      World&         world;
      Npc&           hero;
      Camera&        camera;
      PlayerControl& player;
      int            w = 0, h = 0;
      float          scale = 1.f;
      };

    // every frame in the game: what the hero sees becomes explored
    void tick(World& world, Npc& hero, float sight);

    // the map of the whole place for the minimap: painted when the hero has it, else his sketch
    const Tempest::Texture2d* wholeMap(World& world, Npc& hero);

    bool isOpen() const { return open; }
    void setOpen(bool o);
    void paint(Tempest::Painter& p, const Ctx& c);         // parchment (when open)
    void paintOverlay(Tempest::Painter& p, const Ctx& c);  // the overlay over the game

    // input while the parchment is open (true: used)
    bool mouseDown (Tempest::MouseEvent& e);
    bool mouseUp   (Tempest::MouseEvent& e);
    bool mouseMove (Tempest::MouseEvent& e);
    bool mouseWheel(Tempest::MouseEvent& e);
    bool keyDown   (Tempest::KeyEvent& e);    // also the text of a pin's note
    bool isEditing() const { return editPin>=0; }

  private:
    struct Layer {
      std::string  title;     // cp1250 (from the scripts)
      std::string  texture;
      float        left = 0, top = 0, right = 0, bottom = 0;
      bool         owned = false;
      bool         whole = false;
      };
    struct Marker {
      uint32_t     instance = 0;     // the NPC's instance (found in the world every frame: NPCs come and go)
      bool         teacher  = false;
      bool         trader   = false;
      };
    struct Sketch {
      Tempest::Texture2d tex;   // ink on paper
      Tempest::Texture2d ink;   // only the lines (white, alpha = line): the overlay over the game
      bool               tried = false;
      };

    void              updateLayers(World& world, Npc& hero);
    void              updateMarkers(World& world);
    const Tempest::Texture2d* sketchOf(const std::string& texture, bool inkOnly = false);
    ArpgMapData*      data() const;

    // map rect on the screen for the current zoom / pan (whole layer)
    Tempest::Rect     mapRect(const Ctx& c, const Tempest::Texture2d& tex) const;
    Tempest::Vec2     toMap(const Layer& l, const Tempest::Rect& r, float wx, float wz) const;
    Tempest::Vec2     toWorld(const Layer& l, const Tempest::Rect& r, float sx, float sy) const;

    void              drawExplored(Tempest::Painter& p, const Layer& l, const Tempest::Texture2d& tex,
                                   const std::function<Tempest::Vec2(float,float)>& toScreen,
                                   float x0, float z0, float x1, float z1, const Tempest::Color& cl, bool both) const;
    void              drawHero(Tempest::Painter& p, const Tempest::Vec2& at, const Tempest::Vec2& dir, float scale) const;

    bool                          open = false;
    std::string                   worldName;

    // Diathic: the overlay (Tab): walls and cliffs of the world (steep triangles of the landscape) as thin lines, like
    // the automap of Diablo II - not the painted map blown up; per 4 m cell (ArpgMapData::Cell), made once per world
    struct WallSeg { float x0, z0, x1, z1, y0, y1; };
    const void*                                       wallWorld = nullptr;
    std::unordered_map<int64_t, std::vector<WallSeg>> walls;
    void                          buildWalls(World& w);
    std::vector<Layer>            layers;
    size_t                        layer = 0;
    uint64_t                      layersAt = 0;
    std::vector<Marker>           markers;
    std::string                   markersWorld;
    uint64_t                      markersAt = 0;
    std::unordered_map<std::string, Sketch> sketches;

    float                         zoom = 1.f;
    Tempest::Vec2                 pan;            // in parts of the map (0..1)
    bool                          dragging = false;
    Tempest::Point                dragFrom;
    Tempest::Vec2                 panFrom;
    Tempest::Point                mouse;
    Tempest::Rect                 lastRect;       // where the map was drawn
    std::vector<Tempest::Rect>    tabRects;
    int                           editPin = -1;   // pin whose note is being typed
    float                         lastScale = 1.f;
  };

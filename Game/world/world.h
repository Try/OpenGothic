#pragma once

#include <Tempest/VertexBuffer>
#include <Tempest/IndexBuffer>
#include <Tempest/Matrix4x4>
#include <string>

#include <daedalus/DaedalusVM.h>
#include <zenload/zTypes.h>
#include <zenload/zTypes.h>

#include "graphics/worldview.h"
#include "graphics/staticobjects.h"
#include "physics/dynamicworld.h"
#include "worldscript.h"
#include "npc.h"
#include "interactive.h"
#include "resources.h"
#include "trigger.h"

class Gothic;
class RendererStorage;
class Focus;

class World final {
  public:
    World()=default;
    World(const World&)=delete;
    World(Gothic &gothic,const RendererStorage& storage, std::string file);

    struct Block {
      const Tempest::Texture2d*      texture = nullptr;
      Tempest::IndexBuffer<uint32_t> ibo;
      bool                           alpha=false;
      };

    struct Dodad {
      const ProtoMesh*       mesh   = nullptr;
      const PhysicMeshShape* physic = nullptr;
      Tempest::Matrix4x4     objMat;
      };

    bool isEmpty() const { return wname.empty(); }
    const std::string& name() const { return wname; }

    const Tempest::VertexBuffer<Resources::Vertex>& landVbo()    const { return vbo; }
    const std::vector<Block>&                       landBlocks() const { return blocks; }

    std::vector<Dodad>       staticObj;
    std::vector<Interactive> interactiveObj;

    const ZenLoad::zCWaypointData* findPoint(const std::string& s) const { return findPoint(s.c_str()); }
    const ZenLoad::zCWaypointData* findPoint(const char* name) const;

    WorldView*    view()   const { return wview.get(); }
    DynamicWorld* physic() const { return wdynamic.get(); }

    StaticObjects::Mesh getView(const std::string& visual);
    StaticObjects::Mesh getView(const std::string& visual, int32_t headTex, int32_t teetTex, int32_t bodyColor);
    DynamicWorld::Item  getPhysic(const std::string& visual);

    void updateAnimation();
    Npc* player() const { return npcPlayer; }

    void     tick(uint64_t dt);
    uint64_t tickCount() const;
    void     setDayTime(int32_t h,int32_t min);
    gtime    time() const;

    Daedalus::PARSymbol& getSymbol(const char* s) const;

    Focus findFocus(const Npc& pl,const Tempest::Matrix4x4 &mvp, int w, int h);
    Focus findFocus(const Tempest::Matrix4x4 &mvp, int w, int h);

    const Trigger* findTrigger(const std::string& s) const { return findTrigger(s.c_str()); }
    const Trigger* findTrigger(const char* name) const;

    void marchInteractives(Tempest::Painter& p, const Tempest::Matrix4x4 &mvp, int w, int h) const;

    std::vector<WorldScript::DlgChoise> updateDialog(const WorldScript::DlgChoise &dlg);
    void exec(const WorldScript::DlgChoise& dlg, Npc& player,Npc& hnpc);

    void aiProcessInfos(Npc &player, Npc& npc);
    void aiOutput(const char* msg);
    void aiCloseDialog();

    void printScreen(const char* msg, int x, int y, int time,const Tempest::Font &font);
    void print      (const char* msg);

  private:
    std::string                           wname;
    Gothic*                               gothic=nullptr;
    ZenLoad::zCWayNetData                 wayNet;
    std::vector<ZenLoad::zCWaypointData>  freePoints, startPoints;
    std::vector<ZenLoad::zCWaypointData*>  indexPoints;

    Tempest::VertexBuffer<Resources::Vertex> vbo;
    std::vector<Block>                       blocks;

    Npc*                                  npcPlayer=nullptr;
    std::vector<Trigger>                  triggers;

    std::unique_ptr<DynamicWorld>         wdynamic;
    std::unique_ptr<WorldView>            wview;
    std::unique_ptr<WorldScript>          vm;

    void    adjustWaypoints(std::vector<ZenLoad::zCWaypointData>& wp);
    void    loadVob(const ZenLoad::zCVobData &vob);
    void    addStatic(const ZenLoad::zCVobData &vob);
    void    addInteractive(const ZenLoad::zCVobData &vob);
    void    addTrigger(const ZenLoad::zCVobData &vob);

    void    initScripts(bool firstTime);
    int32_t runFunction(const std::string &fname,bool clearDataStack);

    Interactive* findInteractive(const Npc& pl,const Tempest::Matrix4x4 &mvp, int w, int h);
    Npc*         findNpc(const Npc& pl,const Tempest::Matrix4x4 &mvp, int w, int h);
  };

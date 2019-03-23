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
#include "item.h"
#include "npc.h"
#include "interactive.h"
#include "resources.h"
#include "trigger.h"
#include "worldobjects.h"
#include "waypoint.h"
#include "waymatrix.h"

class Gothic;
class RendererStorage;
class Focus;
class WayMatrix;

class World final {
  public:
    World()=delete;
    World(const World&)=delete;
    World(Gothic &gothic, const RendererStorage& storage, std::string file, std::function<void(int)> loadProgress);

    bool isEmpty() const { return wname.empty(); }
    const std::string& name() const { return wname; }

    const WayPoint* findPoint(const std::string& s) const { return findPoint(s.c_str()); }
    const WayPoint* findPoint(const char* name) const;
    const WayPoint* findWayPoint(const std::array<float,3>& pos) const;
    const WayPoint* findWayPoint(float x,float y,float z) const;

    const WayPoint* findFreePoint(const std::array<float,3>& pos,const char* name) const;
    const WayPoint* findFreePoint(float x,float y,float z,const char* name) const;

    const WayPoint* findNextFreePoint(const Npc& pos,const char* name) const;
    const WayPoint* findNextPoint(const WayPoint& pos) const;

    WayPath       wayTo(const Npc& pos,const WayPoint& end) const;
    WayPath       wayTo(float npcX,float npcY,float npcZ,const WayPoint& end) const;

    WorldView*    view()   const { return wview.get();    }
    DynamicWorld* physic() const { return wdynamic.get(); }
    WorldScript*  script() const { return vm.get();       }

    StaticObjects::Mesh getView(const std::string& visual) const;
    StaticObjects::Mesh getView(const std::string& visual, int32_t headTex, int32_t teetTex, int32_t bodyColor) const;
    StaticObjects::Mesh getStaticView(const std::string &visual,int32_t tex) const;
    DynamicWorld::Item  getPhysic(const std::string& visual);

    void     updateAnimation();

    Npc*     player() const { return npcPlayer; }
    void     tick(uint64_t dt);
    uint64_t tickCount() const;
    void     setDayTime(int32_t h,int32_t min);
    gtime    time() const;

    Daedalus::PARSymbol& getSymbol(const char* s) const;
    size_t               getSymbolIndex(const char* s) const;

    Focus findFocus(const Npc& pl,const Tempest::Matrix4x4 &mvp, int w, int h);
    Focus findFocus(const Tempest::Matrix4x4 &mvp, int w, int h);

    Trigger*       findTrigger(const std::string& name) { return findTrigger(name.c_str()); }
    Trigger*       findTrigger(const char*        name);
    Interactive*   aviableMob(const Npc &pl, const std::string& name);

    void   marchInteractives(Tempest::Painter& p, const Tempest::Matrix4x4 &mvp, int w, int h) const;
    void   marchPoints      (Tempest::Painter& p, const Tempest::Matrix4x4 &mvp, int w, int h) const;

    auto   updateDialog(const WorldScript::DlgChoise &dlg, Npc &player, Npc &npc) -> std::vector<WorldScript::DlgChoise>;
    void   exec(const WorldScript::DlgChoise& dlg, Npc& player,Npc& hnpc);

    void   aiProcessInfos(Npc &player, Npc& npc);
    bool   aiOutput(Npc &player, const char* msg);
    void   aiForwardOutput(Npc &player, const char* msg);
    bool   aiCloseDialog();
    bool   aiIsDlgFinished();
    bool   aiUseMob  (Npc &pl, const std::string& name);

    void   printScreen(const char* msg, int x, int y, int time,const Tempest::Font &font);
    void   print      (const char* msg);

    void   onInserNpc (Daedalus::GEngineClasses::C_Npc* handle, const std::string &s);
    Item*  addItem    (size_t itemInstance, const char *at);
    Item*  takeItem   (Item& it);
    void   removeItem (Item &it);
    size_t hasItems(const std::string& tag,size_t itemCls);

    void   sendPassivePerc(Npc& self,Npc& other,Npc& victum,int32_t perc);

  private:
    std::string                           wname;
    Gothic&                               gothic;

    std::unique_ptr<WayMatrix>            wmatrix;

    Npc*                                  npcPlayer=nullptr;

    std::unique_ptr<DynamicWorld>         wdynamic;
    std::unique_ptr<WorldView>            wview;
    WorldObjects                          wobj;
    std::unique_ptr<WorldScript>          vm;

    void         loadVob(ZenLoad::zCVobData &vob);
    void         addStatic(const ZenLoad::zCVobData &vob);
    void         addInteractive(const ZenLoad::zCVobData &vob);
    void         addItem(const ZenLoad::zCVobData &vob);

    void         initScripts(bool firstTime);
  };

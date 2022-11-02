#include "respawnobject.h"

#include "game/serialize.h"
#include "gothic.h"
#include <string>
#include <vector>
#include <sstream>
#include <stdarg.h>

using namespace Tempest;

std::vector<RespawnObject> RespawnObject::respawnList = {};

RespawnObject::RespawnObject(Serialize &fin){
    fin.read(inst,wp,respawnDay);
}

void RespawnObject::save(Serialize &fout) const{
    fout.write(inst,wp,respawnDay);
}

void RespawnObject::registerObject(size_t inst, std::string wp, uint32_t guild){
    if (!RespawnObject::npcShouldRespawn(guild)) {
        printMsg("Not a respawnable monster (Guild ID " + std::to_string(guild) + ")");
        return;
    }
    RespawnObject ro;
    ro.inst = inst;
    ro.wp = wp;
    uint32_t minDays = 3; // Should be at least 1
    uint32_t maxDays = 10;
    uint32_t currDay = (uint32_t)(Gothic::inst().world())->time().day();
    uint32_t offset = minDays + (Gothic::inst().world())->script().rand(maxDays - minDays);
    ro.respawnDay = currDay + offset;

    printMsg("Register respawn for day " + std::to_string(ro.respawnDay));

    // Store for respawn in respawn list
    RespawnObject::respawnList.push_back(ro);
}

void RespawnObject::processRespawnList(){
    printMsg("Process respawn list, currently " + std::to_string(respawnList.size()) + " monsters registered");
    uint32_t currDay = (uint32_t)(Gothic::inst().world())->time().day();
    uint32_t cnt = 0;
    std::vector<RespawnObject>::iterator iter;
    for (iter = respawnList.begin(); iter != respawnList.end(); ){
        if ((*iter).respawnDay <= currDay) {
            cnt++;
            (Gothic::inst().world())->addNpc((*iter).inst, (*iter).wp);
            iter = respawnList.erase(iter);
        } else
            ++iter;
    }
    printMsg("Respawned " + std::to_string(cnt) + " monsters", 3);
    printMsg("Still " + std::to_string(respawnList.size()) + " entries in respawn-list registered", 5);
    return;
}

void RespawnObject::saveRespawnState(Serialize &fout){
    fout.write(respawnList);
}

void RespawnObject::loadRespawnState(Serialize &fin){
    fin.read(respawnList);
}

bool RespawnObject::npcShouldRespawn(uint32_t guild){
    // Check which NPCs should respawn -- return true if npc should respawn, otherwise false
    // Skip humans and orcs (test for monsters in general)
    if (Guild::GIL_SEPERATOR_HUM > guild || guild > Guild::GIL_SEPERATOR_ORC)
        return false;
    // Skip monsters with guild higher than GIL_SWAMPGOLEM
    if (guild > Guild::GIL_SWAMPGOLEM)
        return false;
    // Skip specific monsters
    uint32_t skipGuilds[] = {
        // No summoned monsters
        GIL_SUMMONED_GOBBO_SKELETON,
        GIL_SUMMONED_WOLF,
        GIL_SUMMONED_SKELETON,
        GIL_SUMMONED_GOLEM,
        GIL_SUMMONED_DEMON,
        GIL_SummonedGuardian,
        GIL_SummonedZombie,
        // No dragons
        GIL_DRAGON,
        // No stoneguardians and gargoyles
        GIL_Stoneguardian,
        GIL_Gargoyle
    };
    auto it = std::find(std::begin(skipGuilds), std::end(skipGuilds), guild);
    // Test guild is not in skip guilds array
    if (it != std::end(skipGuilds))
        return false;
    return true;
}

void RespawnObject::printMsg(std::string msg){
    printMsg(msg, 1);
}
void RespawnObject::printMsg(std::string msg, int y){
    if(!Gothic::inst().doFrate())
        return;
    int x = 1;
    int timeInSec = 5;
    Gothic::inst().onPrintScreen(
        msg.c_str(),
        x, (y+1),
        timeInSec,
        Resources::font()
    );
}
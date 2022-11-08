#include "respawnobject.h"

#include <Tempest/Log>

#include "game/serialize.h"
#include "world/world.h"
#include "gothic.h"

#include <vector>
#include <map>

using namespace Tempest;

std::vector<RespawnObject> RespawnObject::respawnList = {};
std::string RespawnObject::processedDayWorld = "";
uint32_t RespawnObject::minDays = 2;  // Should be at least 1
uint32_t RespawnObject::maxDays = 10; // Must be bigger than minDays

RespawnObject::RespawnObject(Serialize &fin){
    fin.read(inst,wp,respawnDay,world);
}

void RespawnObject::save(Serialize &fout) const{
    fout.write(inst,wp,respawnDay,world);
}

void RespawnObject::registerObject(size_t inst, std::string wp, uint32_t guild){
    if (!RespawnObject::npcShouldRespawn(guild))
        return;
    World *world = Gothic::inst().world();
    // Skip if waypoint is unknown
    if (!world->findPoint(wp))
        return;
    RespawnObject ro;
    ro.inst = inst;
    ro.wp = wp;
    uint32_t currDay = getCurrDay(world);
    uint32_t offset = minDays + world->script().rand(maxDays - minDays);
    ro.respawnDay = currDay + offset;
    ro.world = world->name();

    printMsg("Register respawn for day " + std::to_string(ro.respawnDay) + " (" + ro.world + ")");

    // Store for respawn in respawn list
    RespawnObject::respawnList.push_back(ro);
}

void RespawnObject::processRespawnList(){
    // Is respawn activated ?
    if (!Gothic::inst().doRespawn())
        return;
    World* world  = Gothic::inst().world();
    // Ensure respawn list is not processed during world transition / loading sequences
    if(world==nullptr || Gothic::inst().player()==nullptr) {
        printMsg("Skip processing respawn list (world not fully loaded)");
        return;
    }
    uint32_t currDay = getCurrDay(world);
    // Check against cache value for early abort (no multi-processing on same day in same world)
    std::string currDayWld = std::to_string(currDay) + std::string("_") + std::string(world->name());
    if (processedDayWorld.compare(currDayWld) == 0) {
        printMsg("Skip processing respawn list (day+world already processed today)");
        return;
    }
    // Store current day and world as cache value
    processedDayWorld = currDayWld;
    uint32_t cnt = 0;
    std::vector<RespawnObject>::iterator iter;
    for (iter = respawnList.begin(); iter != respawnList.end(); ){
        if ((*iter).respawnDay <= currDay && world->name().compare((*iter).world) == 0) {
            cnt++;
            world->addNpc((*iter).inst, (*iter).wp);
            iter = respawnList.erase(iter);
        } else
            ++iter;
    }
    printMsg("Respawned " + std::to_string(cnt) + " monsters (" + std::to_string(respawnList.size()) + " still registered)");
    return;
}

void RespawnObject::saveRespawnState(Serialize &fout){
    fout.write(respawnList);
}

void RespawnObject::loadRespawnState(Serialize &fin){
    fin.read(respawnList);
}

/**
 * Handle one of following commands:
 * - 'show': Make a printlog to the game screen
 * - 'clear': Clear respawn list completely
 */
bool RespawnObject::handleCommand(std::string_view cmd){
    // Is respawn activated ?
    if (!Gothic::inst().doRespawn()) {
        printMsg("OpenGothic respawn system is deactivated globally");
        return false;
    }
    if (cmd.compare("show") == 0) {
        // Make print output
        std::map<std::string, int> map;
        for (auto iter = respawnList.begin(); iter != respawnList.end(); ++iter){
            if (map.contains((*iter).world))
                map[(*iter).world] = map[(*iter).world] +1;
            else
                map[(*iter).world] = 0;
        }
        int line = 1;
        printMsg("Respawn list stats", line++, true);
        printMsg("-------------------", line++, true);
        printMsg("Monsters will respawn between " + std::to_string(minDays) + " and " + std::to_string(maxDays) + " days after being killed", line++, true);
        printMsg("Currently " + std::to_string(respawnList.size()) + " monsters stored for respawn", line++, true);
        for (auto iter = map.begin(); iter != map.end(); ++iter)
            printMsg(" - " + iter->first + ": " + std::to_string(iter->second) + " entries", line++, true);
    } else if (cmd.compare("clear") == 0) {
        // Clear respawn list
        respawnList.clear();
        printMsg("Respawn list cleared", 1, true);
    } else if (cmd.compare("process") == 0) {
        // process now
        processRespawnList();
    } else
        return false;
    return true;
}

bool RespawnObject::npcShouldRespawn(uint32_t guild){
    // Is respawn activated ?
    if (!Gothic::inst().doRespawn())
        return false;
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
        // No shadowbeasts and bloodhounds (due to shadowbeast-horn quest)
        GIL_SHADOWBEAST,
        GIL_SHADOWBEAST_SKELETON,
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

uint32_t RespawnObject::getCurrDay(World* world) {
    return (uint32_t)world->time().day();
}

void RespawnObject::printMsg(std::string msg){
    printMsg(msg, 1);
}

void RespawnObject::printMsg(std::string msg, int y){
    printMsg(msg, 1, false);
}

void RespawnObject::printMsg(std::string msg, int y, bool forcePrint){
    Log::i("[respawn] ", msg);
    if(!(forcePrint || Gothic::inst().doFrate()))
        return;
    int x = 1;
    int timeInSec = 5;
    Gothic::inst().onPrintScreen(
        msg.c_str(),
        x, 2*y+1, // Double y value due to low line height
        timeInSec,
        Resources::font()
    );
}

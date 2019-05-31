## Daedalus scripts
daedalus is script language, made for original gothic game. OpenGothic uses VM from ZenLib library and implements most of game-engine script api:

### types api
* func string ConcatStrings(var string lhs,var string rhs)
Returns a newly constructed string object with its value being the concatenation of the characters in lhs followed by those of rhs.
* func string IntToString(var int x)
A string object containing the representation of 'x' as a sequence of characters.
* func int FloatToString(var float x)
A string object containing the representation of 'x' as a sequence of characters.

* float	IntToFloat(var int x)
Return closest to x floating point number
* int FloatToInt(var float x)
Rounds x downward, returning the largest integral value that is not greater than x.

### helpers
* func int Hlp_Random(var int max)
Return random value in range [0..max-1]. Retunrns 0, if max<=0

* func int Hlp_StrCmp(var string s1,var string s2)
Retunrn True, if s1 is equal to s2; False otherwise

* func int Hlp_IsValidNpc(VAR C_Npc self)
Returns True, if 'self' is valid reference to C_Npc; False otherwise

* func int Hlp_IsValidItem(var C_Item item)
Returns True, if 'item' is valid reference to C_Item; False otherwise

* func int Hlp_IsItem(var C_Item item,var int instanceName)
Returns True, if 'item' is valid reference to C_Item and belongs to instanceName 'class'; False otherwise
Example usage if( Hlp_IsItem(item,ItMuArrow)==TRUE ) { ... }

* func C_Npc Hlp_GetNpc(var int instanceName)
Retunrn any C_Npc instance of class instanceName. Returns invalid reference, if no instances found.

* func int Hlp_GetInstanceID(var C_Npc npc)
Return instance-id of npc

* func int Hlp_GetInstanceID(var C_Item item)
Return instance-id of item

### npc api
* func int Npc_GetStateTime(var C_Npc npc)
Return how much time 'npc' spent in current state

* func void Npc_SetStateTime(var C_Npc npc,var int seconds)
Overrides state-time for 'npc'

* func string Npc_GetNearestWP(var C_Npc npc)
Return name of nearest to 'npc' waypoint

* func string Npc_GetNextWP(var C_Npc npc)
Return name of second nearest to 'npc' waypoint

* func int Npc_GetBodyState(var C_Npc npc) 
Return current bodystate of 'npc', as bitset.
Not user-friendly, use helper function C_BodyStateContains(self,bodystate)

* ![!](ni.png) func int Npc_HasBodyFlag(var C_Npc npc, var int bodyFlag)
Not implemented.

* func void Npc_SetToFistMode(var C_Npc npc)
Set npc to melee fight-mode, with no weapon in hands

* func void Npc_SetToFightMode(var C_Npc npc, var int weaponId)
Set npc to melee fight-mode, with weapon, specified by 'weaponId', in hands
weapon will be created automatically, if npc doesn't have it in inventory

* func int Npc_IsInFightMode(var C_Npc self, var int fmode) 
Return TRUE, if npc have appropriate weapon in hands
Note: fmode is not a bitset!

| code | desription | value |
---|---|--
| FMODE_NONE | no weapon | 0x0
| FMODE_FIST | fist | 0x1
| FMODE_MELEE | melee weapon | 0x2
| FMODE_FAR | bow or crossbow | 0x5
| FMODE_MAGIC | scroll or rune | 0x7

* func C_Item Npc_GetReadiedWeapon(var C_Npc npc)
Returns current active weapon(sword for FMODE_MELEE, bow for FMODE_FAR, etc)

* ![!](ni.png) func int Npc_HasReadiedWeapon(var C_Npc npc)
Returns TRUE, if npc has any weapon in hands
Not implemented.

* ![!](ni.png) func int Npc_HasReadiedMeleeWeapon(var C_Npc npc)
Returns TRUE, if npc has melee weapon in hands
Not implemented.

* ![!](ni.png) func int Npc_HasReadiedRangedWeapon(var C_Npc npc)
Returns TRUE, if npc has ranged weapon in hands
Not implemented.

* ![!](ni.png) func int Npc_HasRangedWeaponWithAmmo(var C_Npc npc)
Returns TRUE, if npc has ranged weapon in hands and appropriate ammo in inventory
Not implemented.

* func int Npc_GetTarget(var C_Npc npc)
If, npc had a internal target to attack, then function returns TRUE, and assign global variable 'other' to npc.target
Returns FALSE, if npc.target is empty

* func int Npc_GetNextTarget(var C_Npc npc)
Perform search for a closest enemy(see ATTITUDE api). The enemy must not be dead, or be in unconscious state.
Returns TRUE and save result to 'other' and npc.target, if target is found.
FALSE, if target not found.
Note: in original Gothic you have to call Npc_PerceiveAll, before process any search; OpenGothic doesn't care.

* ![!](ni.png) func int Npc_IsNextTargetAvailable(var C_Npc npc)
similar to Npc_GetNextTarget, but not changing 'other' and npc.target
Not implemented.

* func void Npc_SetTarget(var C_Npc npc, var C_Npc other)
assign internal reference npc.target to 'other'

* ![!](ni.png) func int Npc_AreWeStronger(var C_Npc npc, var C_Npc other) 
Not implemented.

* ![!](ni.png) func int Npc_IsAiming(var C_Npc npc, var C_Npc other)
return TRUE, if 'npc' is aiming to 'other', with bow, crossbow or magic
Not implemented.

* func int Npc_IsOnFP(var C_Npc npc, var string fpname)
Returns TRUE, if 'npc' is standing of fp with name 'fpname'

* ![!](ni.png) func int Npc_IsWayBlocked(var C_Npc self) 
Not implemented. Seems to be unused in original game

* ![!](ni.png) func C_Item Npc_GetInvItem(var C_Npc npc, var int itemInstance)
Not implemented. 

* func int Npc_HasItems(var C_Npc npc, var int itemInstance)
Return count of items with class 'itemInstance' in 'npc' inventory

* ![!](ni.png) func int Npc_GetInvItemBySlot(var C_Npc npc, var int category, var int slotNr)
Not implemented. 

* func void Npc_RemoveInvItem(var C_Npc npc, var int itemInstance)
shortcur for Npc_RemoveInvItems(npc,itemInstance,1)

* func void Npc_RemoveInvItems(var C_Npc npc, var int itemInstance, var int amount)
removes 'amount' of items with class 'itemInstance' from 'npc' inventory
this function takes no effect, if amount<1

### ai module
Each npc in gothic have a ai-queue - array of actions to perform sequentialy.
Example:
```c++
  AI_RemoveWeapon(diego);
  AI_Wait(diego,0.5);
  AI_UseItem(diego,ItPo_Health_03);
```
  In this example Diego will remove weapon, then wait for 500 ms, and then use healing potion
* func void AI_Wait(var C_Npc npc,var float s)
Wait for 's' seconds

* func void AI_PlayAni(var C_Npc npc,var string anim)
play animation 'anim', for 'npc'.

* ![!](ni.png) func void AI_StandUp(var C_Npc npc)
dettach 'npc' from MOBSI object.
it's not clear how exactly this function should work

* func void AI_StandUpQuick(var C_Npc npc)
same as AI_StandUp, but skips animation

* ![!](ni.png) func void AI_Quicklook(var C_Npc npc,var C_Npc other)
turn head of 'npc' to 'other', for a two seconds
Not implemented.

* ![!](ni.png) func void AI_LookAt(var C_Npc npc,var string name)
'npc' starts look to point with name='name'
Not implemented.

* func void AI_LookAtNpc(var C_Npc npc,var C_Npc other)
'npc' starts look to 'other'

* func void AI_StopLookAt(var C_Npc npc)
'npc' stops looking to npc/point

* ![!](ni.png) func void AI_PointAt(var C_Npc npc,var string name)
Not implemented.

* ![!](ni.png) func void AI_PointAtNpc(var C_Npc npc,var C_Npc other)
Not implemented.

* ![!](ni.png) func void AI_StopPointAt(var C_Npc npc)
Not implemented.

* ![!](ni.png) func void AI_TakeItem(var C_Npc npc, var C_Item item) 
Not implemented.

* ![!](ni.png) func void AI_DropItem(var C_Npc npc, var int itemid) 
Not implemented.

* func void AI_UseItem(var C_Npc npc,var int itemInstance)
use item of class 'itemInstance'. Nop, if 'npc' doesn't have any of 'itemInstance' items.

* ![!](ni.png) func void AI_UseItemToState(var C_Npc npc,var int itemInstance,var int state);
Not implemented.

* func void AI_UseMob(var C_Npc npc,var string schemeName,var int targetState)
'npc' stats using MOBSIS with scheme 'schemeName', if MOBSIS is not busy

* func void AI_SetWalkmode(var C_Npc npc,var int mode)
set 'npc' walkmode

| code | desription | value |
---|---|--
| NPC_RUN | Run | 0x0
| NPC_WALK | Walk | 0x1
| NPC_SNEAK | Sneak (Not implemented) | 0x2
| NPC_RUN_WEAPON | Run with weapon  | 0x80
| NPC_WALK_WEAPON | Walk with weapon | 0x81
| NPC_SNEAK_WEAPON | Sneak with weapon (Not implemented) | 0x82

* func void AI_GotoWP(var C_Npc npc,var string wpname)
'npc' stats walk to WayPoint with name 'wpname'. The path is going to be calculated, by using waynet

* func void AI_GotoFP(var C_Npc npc,var string fpname)
'npc' stats walk to FreePoint with name 'fpname'. Point must be available.
The point must be less then 20 meters away, from npc. 
Once 'npc' start mode toward FP, point going to became 'locked'.

* func void AI_GotoNextFP(var C_Npc npc,var string fpname)
'npc' stats walk to FreePoint with name 'fpname'.
Similar to AI_GotoFP, but 'current' point of 'npc' is going to be excluded from search.

* func void AI_GotoNpc(var C_Npc npc,var C_Npc other)
'npc' stats walk to 'other'

* ![!](ni.png) func void AI_GotoItem(var C_Npc npc,var C_Item item)
'npc' stats walk to 'item'
Not implemented.

* ![!](ni.png) func void AI_GotoSound(var C_Npc npc)
'npc' stats closest sound source
Not implemented.

* func void AI_Teleport(var C_NPC npc, var string waypoint);
'npc' teleports to 'waypoint'

* func void AI_TurnToNpc(var C_Npc npc, var C_Npc other)
'npc' turns to 'other'

* ![!](ni.png) func void AI_TurnAway(var C_Npc npc, var C_Npc other)
'npc' turns away from 'other'
Not implemented.

* ![!](ni.png) func void AI_WhirlAround(var C_Npc npc, var C_Npc other) 
'npc' turns (but fast!) to 'other' 
Not implemented.

* ![!](ni.png) func void AI_TurnToSound(var C_Npc npc)
'npc' turns to closest source of sound
Not implemented.

* func void AI_AlignToWP(var C_Npc npc)
'npc' turns to direction specified by current npc-waypoint

* func void AI_Dodge(var C_Npc npc)
'npc' make one step backward

* ![!](ni.png) func void AI_PlayAniBS(var C_Npc npc, var string aniname, var int bodystate)
'npc' start a new animation sequence with name='aniname'
bodystate is not handled - so it's same as AI_PlayAni(npc,aniname)

* func void AI_RemoveWeapon(var C_Npc npc)
'npc' removes any active weapon

* ![!](ni.png) func void AI_DrawWeapon(var C_Npc npc)
'npc' draws any weapon
Not implemented.

* func void AI_ReadyMeleeWeapon(var C_Npc npc)
'npc' draws melee weapon

* func void AI_ReadyRangedWeapon(var C_Npc npc)
'npc' draws bow or crossbow 

* ![!](ni.png) func void AI_Attack(var C_Npc npc)
Not implemented.

* ![!](ni.png) func void AI_FinishingMove(var C_Npc npc, var C_Npc other)
npc performs killing move on 'other'
Not implemented.

* ![!](ni.png) func void AI_Defend(var C_Npc npc)
Not implemented. Seems to be unused by original Gothic 2

* ![!](ni.png) func void AI_Flee(var C_Npc npc)
Flee away from npc.target, counter function to AI_Attack
Not implemented.

* ![!](ni.png) func void AI_AimAt(var C_Npc npc, var C_Npc other)
make 'npc' aim to 'other'
Not implemented.

* ![!](ni.png) func void AI_ShootAt(var C_Npc npc, var C_Npc other)
make 'npc' shoot to 'other'
Not implemented.

* ![!](ni.png) func void AI_StopAim(var C_Npc npc)
'npc' stops aiming
Not implemented.

* ![!](ni.png) func void AI_LookForItem(var C_Npc npc, var int instance)
Not implemented.

### inventory api
* func void CreateInvItem(var C_Npc npc, var int itemId)
shortcut for CreateInvItems(npc,itemId,1)

* func void CreateInvItems(var C_Npc npc, var int itemId, var int number) 
add 'number' of items with class described by 'itemId' to 'npc' inventory
affect result of hlp_getinstanceid(itemId) 
this function akes no effect, if 'number'<1

### world api
* func int Wld_GetDay()
Return days from start of adventure, starting from zero.

* func int Wld_IsTime (var int hourS,var int minS,var int hourE,var int minE)
Return TRUE, if current game time is in range [S .. E) or [E .. S)

* func void Wld_InsertNpc(var int npcInstance,var string spawnPoint)
Inserts an C_Npc object in world, at 'spawnPoint' location

* ![!](ni.png) func void Wld_InsertNpcAndRespawn(var int instance,var string spawnPoint,var float spawnDelay)
Not implemented. Seems to be unused in original game

* func int Wld_IsMobAvailable(var C_Npc npc, var string schemeName) 
Checks if MOBSI object is available, in 10 meters around npc. Returns TRUE, if MOBSI has a free slot or npc already connected to this MOBSI

* ![!](ni.png) func int Wld_GetMobState(var C_Npc self,var string schemeName)
Not implemented.

* func int Wld_IsFPAvailable(var C_Npc npc,var string fpName)
Returns TRUE, if nearest to 'npc' free point, with name 'fpName' is available.
If point is locked by npc - also returns TRUE

* func int Wld_IsNextFPAvailable(var C_Npc npc,var string fpName)
same as Wld_IsFPAvailable, but current waypoint is going to be excluded from search

* func void Wld_InsertItem(var int itemInstance, var string spawnPoint)
Inserts an C_Item object in world, at ‘spawnPoint’ location

* ![!](ni.png) func int Wld_RemoveItem(var C_Item item)
Removes 'item' from world
Not implemented.

### view model api
* func void Mdl_ApplyOverlayMds(var C_Npc npc, var string set)
override an animation set of 'npc' with animation-set 'set'
Examlpe 
```c++
Mdl_ApplyOverlayMds(self,"HUMANS_FLEE.MDS"); // overrides run animation, to run very-very fast
```

* func void Mdl_RemoveOverlayMDS(var C_Npc npc, var string set)
remove override-set, from npc

* func void Mdl_ApplyOverlayMDSTimed(var C_Npc npc, var string set, var float timeTicks);
override an animation set of 'npc' with animation-set 'set', for limited amout of time
timeTicks - time in miliseconds
Examlpe 
```c++
Mdl_ApplyOverlayMdsTimed(self,"HUMANS_SPRINT.MDS",120000); // SchnellerHering potion effect
```

* ![!](ni.png) func void Mdl_ApplyRandomAni(var C_Npc npc, var string s1, var string s2 )
Not implemented.

* ![!](ni.png) func void Mdl_ApplyRandomAniFreq(var C_Npc npc, var string s1, var float f2)
Not implemented.

* ![!](ni.png) func void Mdl_StartFaceAni(var C_Npc npc, var string name, var float intensity, var float holdTime)
Not implemented.

* ![!](ni.png) func void Mdl_ApplyRandomFaceAni(var C_Npc self, var string name, var float timeMin, var float timeMinVar, var float timeMax, var float timeMaxVar, var float probMin)
Not implemented.

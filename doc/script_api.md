## Daedalus scripts
Daedalus is script language, made for original gothic game. OpenGothic uses VM from ZenLib library and implements most of game-engine script api.

### legend
* ![!](ni.png) - Not implemented function. Can affect gameplay.
* ![#](nu.png) - Not implemented and not used function. Cannot affect gameplay of game itself, can break some mods.

### types api
* `func string ConcatStrings(var string lhs,var string rhs)`  
  Returns a newly constructed string object with its value being the concatenation of the characters in lhs followed by those of rhs.

* `func string IntToString(var int x)`  
  A string object containing the representation of 'x' as a sequence of characters.

* `func int FloatToString(var float x)`  
  A string object containing the representation of 'x' as a sequence of characters.

* `float IntToFloat(var int x)`  
  Return closest to x floating point number

* `int FloatToInt(var float x)`  
  Rounds x downward, returning the largest integral value that is not greater than x.

### helpers
* `func int Hlp_Random(var int max)`  
  Return random value in range [0..max-1]. Retunrns 0, if max<=0

* `func int Hlp_StrCmp(var string s1,var string s2)`  
  Retunrn True, if s1 is equal to s2; False otherwise

* `func int Hlp_IsValidNpc(var C_Npc self)`  
  Returns True, if 'self' is valid reference to C_Npc; False otherwise

* `func int Hlp_IsValidItem(var C_Item item)`  
  Returns True, if 'item' is valid reference to C_Item; False otherwise

* `func int Hlp_IsItem(var C_Item item,var int instanceName)`  
  Returns True, if 'item' is valid reference to C_Item and belongs to instanceName 'class'; False otherwise
  Example usage `if( Hlp_IsItem(item,ItMuArrow)==TRUE ) { ... }`

* `func C_Npc Hlp_GetNpc(var int instanceName)`  
  Return any C_Npc instance of class instanceName. Returns invalid reference, if no instances found.

* `func int Hlp_GetInstanceID(var C_Npc npc)`  
  Return instance-id of npc

* `func int Hlp_GetInstanceID(var C_Item item)`  
  Return instance-id of item

* ![#](nu.png) `func int Hlp_CutscenePlayed (var string csName)`  
  Return TRUE, if cutscene is played  
  Not implemented.

### npc api
* `func int Npc_GetStateTime(var C_Npc npc)`  
  Return how much time 'npc' spent in current state

* `func void Npc_SetStateTime(var C_Npc npc,var int seconds)`  
  Overrides state-time for 'npc'

* `func string Npc_GetNearestWP(var C_Npc npc)`  
  Return name of nearest to 'npc' waypoint

* `func string Npc_GetNextWP(var C_Npc npc)`  
  Return name of second nearest to 'npc' waypoint

* `func int Npc_GetBodyState(var C_Npc npc)`  
  Return current bodystate of 'npc', as bitset.
  Not user-friendly, use helper function C_BodyStateContains(self,bodystate)

* `func int Npc_HasBodyFlag(var C_Npc npc, var int bodyFlag)`  
  Checks npc state for specified flag. Most used argument for bodyFlag is BS_FLAG_INTERRUPTABLE.

* `func void Npc_SetToFistMode(var C_Npc npc)`  
  Set npc to melee fight-mode, with no weapon in hands

* `func void Npc_SetToFightMode(var C_Npc npc, var int weaponId)`  
  Set npc to melee fight-mode, with weapon, specified by 'weaponId', in hands.  
  Weapon will be created automatically, if npc doesn't have it in inventory

* `func int Npc_IsInFightMode(var C_Npc self, var int fmode)`  
  Return TRUE, if npc have appropriate weapon in hands.  
  Note: fmode is not a bitset!

| code | desription | value |
---|---|--
| FMODE_NONE | no weapon | 0x0
| FMODE_FIST | fist | 0x1
| FMODE_MELEE | melee weapon | 0x2
| FMODE_FAR | bow or crossbow | 0x5
| FMODE_MAGIC | scroll or rune | 0x7

* `func C_Item Npc_GetReadiedWeapon(var C_Npc npc)`  
  Returns current active weapon(sword for FMODE_MELEE, bow for FMODE_FAR, etc)

* ![!](ni.png) `func bool Npc_IsDrawingWeapon(var C_Npc npc)`  
  Not implemented.

* ![#](nu.png) `func int Npc_HasReadiedWeapon(var C_Npc npc)`  
  Returns TRUE, if npc has any weapon in hands  
  Not implemented.

* ![#](nu.png) `func int Npc_HasReadiedMeleeWeapon(var C_Npc npc)`  
  Returns TRUE, if npc has melee weapon in hands.  
  Not implemented.

* ![#](nu.png) `func int Npc_HasReadiedRangedWeapon(var C_Npc npc)`  
  Returns TRUE, if npc has ranged weapon in hands.  
  Not implemented.

* ![#](nu.png) `func int Npc_HasRangedWeaponWithAmmo(var C_Npc npc)`  
  Returns TRUE, if npc has ranged weapon in hands and appropriate ammo in inventory.  
  Not implemented.

* `func int Npc_GetTarget(var C_Npc npc)`  
  If, npc had a internal target to attack, then function returns TRUE, and assign global variable 'other' to npc.target.  
  Returns FALSE, if npc.target is empty.

* `func int Npc_GetNextTarget(var C_Npc npc)`  
  Perform search for a closest enemy(see ATTITUDE api). The enemy must not be dead, or be in unconscious state.  
  Returns TRUE and save result to 'other' and npc.target, if target is found.  
  FALSE, if target not found.  
  Note: in original Gothic you have to call Npc_PerceiveAll, before process any search; OpenGothic doesn't care.

* ![#](nu.png) `func int Npc_IsNextTargetAvailable(var C_Npc npc)`  
  Similar to Npc_GetNextTarget, but not changing 'other' and npc.target.  
  Not implemented.

* `func void Npc_SetTarget(var C_Npc npc, var C_Npc other)`  
  Assign internal reference npc.target to 'other'.

* ![#](nu.png) `func int Npc_AreWeStronger(var C_Npc npc, var C_Npc other)`  
  Not implemented.

* ![#](nu.png) `func int Npc_IsAiming(var C_Npc npc, var C_Npc other)`  
  Return TRUE, if 'npc' is aiming to 'other', with bow, crossbow or magic.  
  Not implemented.

* `func int Npc_IsOnFP(var C_Npc npc, var string fpname)`  
  Returns TRUE, if 'npc' is standing of fp with name 'fpname'.

* ![#](nu.png) `func int Npc_IsWayBlocked(var C_Npc self)`  
  Not implemented. Seems to be unused in original game.

* ![!](ni.png) `func C_Item Npc_GetInvItem(var C_Npc npc, var int itemInstance)`  
  Not implemented.

* `func int Npc_HasItems(var C_Npc npc, var int itemInstance)`  
  Return count of items with class 'itemInstance' in 'npc' inventory.

* ![#](nu.png) `func int Npc_GetInvItemBySlot(var C_Npc npc, var int category, var int slotNr)`  
  Not implemented.

* `func void Npc_RemoveInvItem(var C_Npc npc, var int itemInstance)`  
  Shortcut for Npc_RemoveInvItems(npc,itemInstance,1)

* `func void Npc_RemoveInvItems(var C_Npc npc, var int itemInstance, var int amount)`  
  Removes 'amount' of items with class 'itemInstance' from 'npc' inventory.  
  this function takes no effect, if amount<1.

* `func int Npc_ClearInventory(var C_Npc npc)`  
  Remove all items from inventory of 'npc', except mission items and equipped items.

* `func C_Item Npc_GetEquippedMeleeWeapon(var C_Npc npc)`  
  Returns current melee weapon of 'npc'.

* `func C_Item Npc_GetEquippedRangedWeapon(var C_Npc npc)`  
  Returns current ranged weapon of 'npc'.

* `func C_Item Npc_GetEquippedArmor(var C_Npc npc)`  
  Returns current armour of 'npc'.

* `func int Npc_HasEquippedWeapon(var C_Npc npc)`  
  Returns TRUE, if 'npc' has any equipped weapon.  
  Same as: `Npc_HasEquippedMeleeWeapon(npc) || Npc_HasEquippedRangedWeapon(npc)`

* `func int Npc_HasEquippedMeleeWeapon(var C_Npc npc)`  
  Returns TRUE, if 'npc' has equipped melee weapon.  

* `func int Npc_HasEquippedRangedWeapon(var C_Npc npc)`  
  Returns TRUE, if 'npc' has equipped ranged weapon.  

* `func int Npc_HasEquippedArmor(var C_Npc npc)`  
  Returns TRUE, if 'npc' has equipped armor.  

* `func int Npc_OwnedByNpc(var C_Item item, var C_Npc npc)`  
  Returns TRUE, if 'npc' is owner of 'item'.  

* ![#](nu.png) `func int Npc_OwnedByGuild(var C_Item item, var int guildId)`  
  Returns TRUE, if guild, specified by 'guildId', is owner of 'item'.  
  Not implemented.

* `func int Npc_IsDetectedMobOwnedByNpc(var C_Npc npc, var C_Npc user)`  
  Returns TRUE, if 'user' is currently use an mob owned by 'npc'.  

* ![#](nu.png) `func int Npc_IsDetectedMobOwnedByGuild(var C_Npc npc, var int guildId)`  
  Not implemented.

* ![#](nu.png) `func void Npc_GiveItem(var C_Npc from, var C_Item item, var C_Npc to)`  
  move 'item' from inventory of 'from' to inventory of 'to'.  
  Not implemented. Seem to be unused in G2.

* ![#](nu.png) `func int Npc_StartItemReactModules(var C_Npc npc, var C_Npc other, var C_Item item)`  
  Not implemented.

* ![#](nu.png) `func int Npc_HasOffered(var C_Npc npc, var C_Npc other, var int itemId)`  
  Not implemented.

* `func void Npc_SetRefuseTalk(var C_Npc npc, var int timeSec)`  
  'npc' is awailable for talk for next 'timeSec' seconds
  take no effect, if timeSec<1

* `func int Npc_RefuseTalk(var C_Npc npc)`  
  Returns TRUE, if npc still refuses to talk

* ![#](nu.png) `func void Npc_MemoryEntry(var C_Npc npc, var int source, var C_Npc offender, var int newsid, var C_Npc victim)`  
  Not implemented.

* ![#](nu.png) `func void Npc_MemoryEntryGuild(var C_Npc npc, var int source, var C_Npc offender, var int newsid, var C_Npc victimguild)`  
  Not implemented.

* ![#](nu.png) `func int Npc_HasNews(var C_Npc npc, var int newsID, var C_Npc offender, var C_Npc victim)`  
  Not implemented.

* ![#](nu.png) `func int Npc_IsNewsGossip(var C_Npc npc, var int newsNumber)`  
  Not implemented.

* ![#](nu.png) `func C_Npc Npc_GetNewsWitness(var C_Npc npc, var int newsNumber)`  
  Not implemented.

* ![#](nu.png) `func C_Npc Npc_GetNewsVictim(var C_Npc npc, var int newsNumber)`  
  Not implemented.

* ![#](nu.png) `func C_Npc Npc_GetNewsOffender(var C_Npc npc, var int newsNumber)`  
  Not implemented.

* `func int Npc_IsDead(var C_Npc npc)`  
  Returns TRUE, if 'npc' state is ZS_DEAD

* `func int Npc_KnowsInfo(var C_Npc npc, var int infoId)`  
  Returns TRUE, if 'npc' knowns information with id = 'infoId'

* `func int Npc_CheckInfo(var C_Npc npc, var int important)`  
  Returns TRUE, if 'npc' has any info to tell, with important level equals 'important'

* ![#](nu.png) `func int NPC_GiveInfo(var C_Npc npc, var int important)`  
  Not implemented.

* ![#](nu.png) `func int Npc_CheckAvailableMission(var C_Npc npc, var int missionState, var int important)`  
  Not implemented.

* ![#](nu.png) `func int Npc_CheckRunningMission(var C_Npc npc, var int important)`  
  Not implemented.

* ![#](nu.png) `func int Npc_CheckOfferMission(var C_Npc npc, var int important)`  
  Not implemented.

* `func int Npc_IsPlayer(var C_Npc npc)`  
  Return TRUE, if 'npc' is the player

* ![#](nu.png) `func int Npc_HasDetectedNpc(var C_Npc npc, var C_Npc other)`  
  Not implemented.

* ![#](nu.png) `func int Npc_IsNear(var C_Npc npc, var C_Npc other)`  
  Not implemented.

* `func int Npc_GetDistToNpc(var C_Npc npc1, var C_Npc npc2)`  
  Return distance from 'npc1' to 'npc2' in santimeters.

* `func int Npc_GetHeightToNpc(var C_Npc npc1, var C_Npc npc2)`  
  Return distance by Y axis from 'npc1' to 'npc2' in santimeters.

* ![!](ni.png) `func int Npc_GetHeightToItem(var C_Npc npc1, var C_Item itm)`  
  Return distance by Y axis from 'npc1' to 'itm' in santimeters.

* `func int Npc_GetDistToWP(var C_Npc npc, var string wpName)`  
  Return distance from 'npc' to waypoint with name 'wpName'.

* ![!](ni.png) `func int Npc_GetDistToItem(var C_Npc npc, var C_Item item)`  
  Return distance from 'npc' to item 'item'.  
  Not implemented.

* ![!](ni.png) `func int Npc_GetDistToPlayer(var C_Npc npc)`  
  Not implemented.

* `func int Npc_SetTrueGuild(var C_Npc npc, var int guildId)`  
  Set 'true' guildId for 'npc'

* `func int Npc_GetTrueGuild(var C_Npc npc)`  
  Return 'true' guildId for 'npc'

* `func void Npc_SetAttitude(var C_Npc npc, var int att)`  
  Set attitude for npc to player

* `func int Npc_GetPermAttitude(var C_Npc npc, var C_Npc other)`  
  Return permanent attitude for npc to other

| code | desription | value |
---|---|--
| ATT_HOSTILE | enemy | 0
| ATT_ANGRY | enemy | 1
| ATT_NEUTRAL | neutral | 2
| ATT_FRIENDLY | friendly | 3

* `func void Npc_SetTempAttitude(var C_Npc npc, var int att)`  
  Set temporary attitude for npc to player,

* `func int Npc_GetAttitude(var C_Npc npc, var C_Npc other)`  
  Return attitude betwen 'npc' and 'other'

* `func int Npc_GetGuildAttitude(var C_Npc npc, var C_Npc other)`  
  Return attitude betwen guilds of 'npc' and 'other'

* ![#](nu.png) `func void Npc_SetKnowsPlayer(var C_Npc npc, var C_Npc player)`  
  Not implemented.

* ![#](nu.png) `func int Npc_KnowsPlayer(var C_Npc npc, var C_Npc player)`  
  Not implemented.

* `func void Npc_ClearAIQueue(var C_Npc npc)`  
  Removes every queued action from npc ai-queue

* `func int Npc_IsInState(var C_Npc npc, var func state_fn)`  
  Return TRUE, if current state of 'npc' is equal to 'state_fn'

* `func int Npc_WasInState(var C_Npc npc, var func state_fn)`  
  Return TRUE, if previous state of 'npc' is equal to 'state_fn'

* ![!](ni.png) `func int Npc_IsInRoutine(var C_Npc npc, var func state_fn)`  
  Return TRUE, if current routine of 'npc' is equal to 'state_fn'.  
  Seem to be used only in test-scripts.  
  Not implemented.

* `func void Npc_ExchangeRoutine(var C_Npc npc, var string routineName)`  
  Clear routines list of npc and create a new by calling function Rtn_{routineName}_{npc.id}

* `func void Npc_ChangeAttribute(var C_Npc npc, var int atr, var int value)`  
  Sets a new 'value' to attribute 'att' if 'npc'. if value of 'att' is not in range [0..7], this function takes no effect.

| code | desription | value |
---|---|--
| ATR_HITPOINTS | Hitpoints | 0
| ATR_HITPOINTSMAX | Max hitpoints | 1
| ATR_MANA | Mana | 2
| ATR_MANAMAX | Max mana | 3
| ATR_STRENGTH | Strength | 4
| ATR_DEXTERITY | Dexterity | 5
| ATR_REGENERATEHP | Hit point regeneration | 6
| ATR_REGENERATEMANA | Mana regeneration | 7

* ![#](nu.png) `func void Npc_HasTalent(var C_Npc npc, var int tal)`  
  Not implemented.

* ![#](nu.png) `func void Npc_HasFightTalent(var C_Npc npc, var int tal)`  
  Not implemented.

* ![#](nu.png) `func void Npc_CreateSpell(var C_Npc npc, var int spellnr)`  
  Not implemented.

* ![#](nu.png) `func void Npc_LearnSpell(var C_Npc npc, var int spellnr)`  
  Not implemented.

* ![#](nu.png) `func void Npc_SetTeleportPos(var C_Npc npc)`  
  Not implemented.

* ![#](nu.png) `func int Npc_GetActiveSpell(var C_Npc npc)`  
  Not implemented.

* `func int Npc_GetLastHitSpellID(var C_Npc npc)`  
  Return Id of spell witch hit 'npc' last

* `func int Npc_GetLastHitSpellCat(var C_Npc npc)`  
  Return category of spell witch hit 'npc' last

| code | desription | value |
---|---|--
| SPELL_GOOD | Good/Frienly spell | 0
| SPELL_NEUTRAL | Neutral spell | 1
| SPELL_BAD | Bad/Aggressive spell | 2

* ![!](ni.png) `func int Npc_SetActiveSpellInfo(var C_Npc npc, var int i1)`  
  Not implemented.

* ![#](nu.png) `func int Npc_GetActiveSpellLevel(var C_Npc npc)`  
  Not implemented.

* `func int Npc_GetActiveSpellCat(var C_Npc npc)`  
  Return category of active spell, for caster 'npc'

* ![#](nu.png) `func int Npc_SetActiveSpellInfo(var C_Npc npc,int value)`  
  Assign a new 'value' to internal field of 'npc'.
  Return previous value.
  Not implemented.

* ![#](nu.png) `func int Npc_HasSpell(var C_Npc npc, var int spellId)`  
  Returns TRUE, if 'npc' has specified by 'spellId' spell in inventory.  
  Not implemented.

* `func void Npc_PercEnable(var C_Npc npc, var int percId, var func callback)`  
  assign a new perception-procedure to 'npc'. 'percId' has to be one of enum values, described bellow:

| code | desription | implementation note | value |
---|---|---|---
| PERC_ASSESSPLAYER | player is near | Done | 1
| PERC_ASSESSENEMY | enemy is near | Done | 2
| PERC_ASSESSFIGHTER | ? | Not implemented | 3
| PERC_ASSESSBODY | ? | Not implemented | 4
| PERC_ASSESSITEM | ? | Not implemented | 5
| PERC_ASSESSMURDER | other was killed | Done | 6
| PERC_ASSESSDEFEAT | npc fall in unconscious | Done | 7
| PERC_ASSESSDAMAGE | 'npc' receive damage | Done | 8
| PERC_ASSESSOTHERSDAMAGE | 'npc' nearby receives damage | Done | 9
| PERC_ASSESSTHREAT | ? | Not implemented | 10
| PERC_ASSESSREMOVEWEAPON | ? | Not implemented | 11
| PERC_OBSERVEINTRUDER | ? | Not implemented | 12
| PERC_ASSESSFIGHTSOUND | fight happening nearby | Done | 13
| PERC_ASSESSQUIETSOUND | ? | Not implemented | 14
| PERC_ASSESSWARN | ? | Not implemented | 15
| PERC_CATCHTHIEF | ? | Not implemented | 16
| PERC_ASSESSTHEFT | player takes an item | Done | 17
| PERC_ASSESSCALL | ? | Not implemented | 18
| PERC_ASSESSTALK | player want's to start dialog | Done | 19
| PERC_ASSESSGIVENITEM | ? | Not implemented | 20
| PERC_ASSESSFAKEGUILD | ? | Not implemented | 21
| PERC_MOVEMOB | ? | Not implemented | 22
| PERC_MOVENPC | ? | Not implemented | 23
| PERC_DRAWWEAPON | ? | Not implemented | 24
| PERC_OBSERVESUSPECT | ? | Not implemented | 25
| PERC_NPCCOMMAND | ? | Not implemented | 26
| PERC_ASSESSMAGIC | Process spell casted on 'npc' | Done | 27
| PERC_ASSESSSTOPMAGIC | ? | Not implemented | 28
| PERC_ASSESSCASTER | ? | Not implemented | 29
| PERC_ASSESSSURPRISE | ? | Not implemented | 30
| PERC_ASSESSENTERROOM | player is inside some room  | Not implemented | 31
| PERC_ASSESSUSEMOB | other uses any of mobsi objects | Done | 32

* `func void Npc_PercDisable(var C_Npc npc, var int percId)`  
  clear perception callback with 'percId' index for 'npc'.

* `func void Npc_SetPercTime(var C_Npc npc, var float seconds)`  
  Set timer interval for perceptions of 'npc'

* ![#](nu.png) `func void Perc_SetRange(var int percId, var int range)`  
  Set range for perception with id equals to 'percId'.  
  Not implemented.

* `func void Npc_SendPassivePerc(var C_Npc npc, var int percId, var C_Npc victum, var C_Npc other)`  
  make an invokation of `npc.perc[percId]`, with preinitialized global variables 'victum' and 'other'.  
  The invocation is asynchronous.  
  Example:
```c++
Npc_SendPassivePerc(self,PERC_ASSESSFIGHTSOUND,self,other); // calls PERC_ASSESSFIGHTSOUND from 'self'
```

* ![#](nu.png) `func void Npc_SendSinglePerc(var C_Npc npc, var C_Npc target, var int percId)`  
  Same as `Npc_SendPassivePerc`, but without overriding `self` and `other`.  
  Not implemented.

* ![#](nu.png) `func void Npc_PerceiveAll(var C_Npc npc)`  
  Implemented as nop.

* `func string Npc_GetDetectedMob(var C_Npc npc)`  
  Returns scheme name of MOB, whitch is currently in use by 'npc'. If 'npc' is not using any of MOB returns empty string.

* `func int Npc_CanSeeNpc(var C_Npc npc1, var C_Npc npc2)`  
  Returns TRUE, if npc2 is visible from 'npc1' point of view.  
  This fuction also check 'npc1' view direction, with +- 100 degress margin.  

* `func int Npc_CanSeeNpcFreeLOS(var C_Npc npc1, var C_Npc npc2)`  
  Returns TRUE, if 'npc2' is visible from 'npc1' point of view, without checking view direction.

* ![#](nu.png) `func int Npc_CanSeeItem(var C_Npc npc, var C_Item item)`  
  Returns TRUE, if 'item' is visible from 'npc' point of view.  
  Not implemented.

* ![!](ni.png) `func int Npc_CanSeeSource(var C_Npc npc)`  
  Returns TRUE, if source of sound is visible from 'npc' point of view.  
  Not implemented.

* ![#](nu.png) `func int Npc_IsInCutscene(var C_Npc npc)`  
  Not implemented.

* ![#](nu.png) `func int Npc_IsPlayerInMyRoom(var C_Npc npc)`  
  Not implemented.

* ![#](nu.png) `func int Npc_WasPlayerInMyRoom(var C_Npc npc)`  
  Not implemented.

* `func void Npc_PlayAni(var C_Npc npc,var string anim)`  
  Play animation 'anim', for 'npc'.

* `func int Npc_GetPortalGuild(var C_Npc npc)`  
  if 'npc' located inside room, return guild assigned for this room.  
  otherwise returns GIL_NONE  

* `func int Npc_IsInPlayersRoom(var C_Npc npc)`  
  Returns TRUE, if 'npc' is in same room as player.

### missions module
* ![#](nu.png) `func void Mis_SetStatus(var int missionName, var int newStatus)`  
  Not implemented.

* ![#](nu.png) `func int Mis_GetStatus(var int missionName)`  
  Not implemented.

* ![#](nu.png) `func int Mis_OnTime(var int missionName)`  
  Not implemented.

### quest-log module
* `func void Log_CreateTopic(var string name,var int section)`  
  Add a new entry in quest-log section described by `section`.  
  Nop, if section argument is none of `LOG_NOTE` and `LOG_MISSION`

| code | desription | value |
---|---|--
| LOG_MISSION | Normal quest | 0
| LOG_NOTE | Note | 1

* `func void Log_SetTopicStatus(var string name, var int status)`  
  Set a new status for quest-log entry with name==`name`  

| code | desription | value |
---|---|--
| Running | Active quest | 1
| Success | Finished quest | 2
| Failed | Failed quest | 3
| Obsolete | Canceled quest | 4

* `func void Log_AddEntry(var string topic, var string entry)`  
  Add a new entry to quest-topic.

### ai module
Each npc in gothic have a ai-queue - array of actions to perform sequentialy.
Example:
```c++
  AI_RemoveWeapon(diego);
  AI_Wait(diego,0.5);
  AI_UseItem(diego,ItPo_Health_03);
```
  In this example Diego will remove weapon, then wait for 500 ms, and then use healing potion
* `func void AI_Wait(var C_Npc npc,var float s)`  
  Wait for 's' seconds

* `func void AI_PlayAni(var C_Npc npc,var string anim)`  
  Play animation 'anim', for 'npc'.

* ![#](nu.png) `func void AI_StandUp(var C_Npc npc)`  
  Dettach 'npc' from MOBSI object.  
  Note: it's not clear how exactly this function should work.

* `func void AI_StandUpQuick(var C_Npc npc)`  
  Same as AI_StandUp, but skips animation.

* ![#](nu.png) `func void AI_Quicklook(var C_Npc npc,var C_Npc other)`  
  Turn head of 'npc' to 'other', for a two seconds.  
  Not implemented.

* ![#](nu.png) `func void AI_LookAt(var C_Npc npc,var string name)`  
  'npc' starts look to point with name='name'.  
  Not implemented.

* `func void AI_LookAtNpc(var C_Npc npc,var C_Npc other)`  
  'npc' starts look to 'other'.

* `func void AI_StopLookAt(var C_Npc npc)`  
  'npc' stops looking to npc/point

* ![!](ni.png) `func void AI_PointAt(var C_Npc npc,var string name)`  
  plays a specific animation of pointing to something(Jorgen use it to show where Pedro did go)
  Not implemented.

* ![!](ni.png) `func void AI_PointAtNpc(var C_Npc npc,var C_Npc other)`  
  Not implemented.

* ![#](nu.png) `func void AI_StopPointAt(var C_Npc npc)`  
  Not implemented.

* ![!](ni.png) `func void AI_TakeItem(var C_Npc npc, var C_Item item)`  
  Not implemented.

* ![#](nu.png) `func void AI_DropItem(var C_Npc npc, var int itemid)`  
  Not implemented.

* `func void AI_UseItem(var C_Npc npc,var int itemInstance)`  
  use item of class 'itemInstance'. Nop, if 'npc' doesn't have any of 'itemInstance' items.

* ![!](ni.png) `func void AI_UseItemToState(var C_Npc npc,var int itemInstance,var int state)`  
  Not implemented.

* `func void AI_UseMob(var C_Npc npc,var string schemeName,var int targetState)`  
  'npc' stats using MOBSIS with scheme 'schemeName', if MOBSIS is not busy.

* `func void AI_SetWalkmode(var C_Npc npc,var int mode)`  
  Set 'npc' walkmode

| code | desription | value |
---|---|--
| NPC_RUN | Run | 0x0
| NPC_WALK | Walk | 0x1
| NPC_SNEAK | Sneak (Not implemented) | 0x2
| NPC_RUN_WEAPON | Run with weapon  | 0x80
| NPC_WALK_WEAPON | Walk with weapon | 0x81
| NPC_SNEAK_WEAPON | Sneak with weapon (Not implemented) | 0x82

* `func void AI_GotoWP(var C_Npc npc,var string wpname)`  
  'npc' stats walk to WayPoint with name 'wpname'. The path is going to be calculated, by using waynet.

* `func void AI_GotoFP(var C_Npc npc,var string fpname)`  
  'npc' stats walk to FreePoint with name 'fpname'. Point must be available.  
  The point must be less then 20 meters away, from npc.  
  Once 'npc' start mode toward FP, point going to became 'locked'.  

* `func void AI_GotoNextFP(var C_Npc npc,var string fpname)`  
  'npc' stats walk to FreePoint with name 'fpname'.  
  Similar to AI_GotoFP, but 'current' point of 'npc' is going to be excluded from search.  

* `func void AI_GotoNpc(var C_Npc npc,var C_Npc other)`  
  'npc' stats walk to 'other'.

* ![!](ni.png) `func void AI_GotoItem(var C_Npc npc,var C_Item item)`  
  'npc' stats walk to 'item'.  
  Not implemented.

* ![#](nu.png) `func void AI_GotoSound(var C_Npc npc)`  
  'npc' stats closest sound source.  
  Not implemented.

* `func void AI_Teleport(var C_NPC npc, var string waypoint)`  
  'npc' teleports to 'waypoint'.

* `func void AI_TurnToNpc(var C_Npc npc, var C_Npc other)`  
  'npc' turns to 'other'.

* ![#](nu.png) `func void AI_TurnAway(var C_Npc npc, var C_Npc other)`  
  'npc' turns away from 'other'.  
  Not implemented.

* ![#](nu.png) `func void AI_WhirlAround(var C_Npc npc, var C_Npc other)`  
  'npc' turns (but fast!) to 'other'.  
  Not implemented.

* ![#](nu.png) `func void AI_TurnToSound(var C_Npc npc)`  
  'npc' turns to closest source of sound.  
  Not implemented.

* `func void AI_AlignToWP(var C_Npc npc)`  
  'npc' turns to direction specified by current npc-waypoint.  

* `func void AI_Dodge(var C_Npc npc)`  
  'npc' make one step backward.

* ![#](nu.png) `func void AI_PlayAniBS(var C_Npc npc, var string aniname, var int bodystate)`  
  'npc' start a new animation sequence with name='aniname'.  
  Bodystate is not handled for now - so it's same as AI_PlayAni(npc,aniname).  

* `func void AI_RemoveWeapon(var C_Npc npc)`  
  'npc' removes any active weapon.  

* ![!](ni.png) `func void AI_DrawWeapon(var C_Npc npc)`  
  'npc' draws any weapon  
  Not implemented.  

* `func void AI_ReadyMeleeWeapon(var C_Npc npc)`  
  'npc' draws melee weapon.

* `func void AI_ReadyRangedWeapon(var C_Npc npc)`  
  'npc' draws bow or crossbow.

* `func void AI_Attack(var C_Npc npc)`  
  Fetch next instruction from C_FightAI, corresponding to npc.fight_tactic

* ![!](ni.png) `func void AI_FinishingMove(var C_Npc npc, var C_Npc other)`  
  npc performs killing move on 'other'.  
  Not implemented.

* ![#](nu.png) `func void AI_Defend(var C_Npc npc)`  
  Not implemented. Seems to be unused by original Gothic 2

* ![#](nu.png) `func void AI_Flee(var C_Npc npc)`  
  Flee away from npc.target, counter function to AI_Attack
  Not implemented.

* ![#](nu.png) `func void AI_AimAt(var C_Npc npc, var C_Npc other)`  
  make 'npc' aim to 'other'.
  Not implemented.

* ![#](nu.png) `func void AI_ShootAt(var C_Npc npc, var C_Npc other)`  
  make 'npc' shoot to 'other'.
  Not implemented.

* ![#](nu.png) `func void AI_StopAim(var C_Npc npc)`  
  'npc' stops aiming.  
  Not implemented.

* ![#](nu.png) `func void AI_LookForItem(var C_Npc npc, var int instance)`  
  Not implemented.

* `func void AI_EquipBestMeleeWeapon(var C_Npc npc)`  
  Equips 'npc' with best melee weapon from his inventory.

* `func void AI_EquipBestRangedWeapon(var C_Npc npc)`  
  Equips 'npc' with best range weapon from his inventory.

* `func void AI_EquipBestArmor(var C_Npc npc)`  
  Equips 'npc' with best armor from his inventory.

* `func void AI_UnequipWeapons(var C_Npc npc)`  
  Unequips all current weapons of 'npc'.

* `func void AI_UnequipArmor(var C_Npc npc)`  
  Unequips current armor of 'npc'.

* `func void AI_EquipArmor(var C_Npc owner, var int itemId)`  
  Equips 'npc' with armor specified by 'itemId'. Armor with class 'itemId' must be in 'npc' inventory.

* `func void AI_Output(var C_Npc npc, var C_Npc target, var string outputName)`  
  'npc' tells to 'target' a dialog line determinated by 'outputName'.  
  'npc' also starts a random dialog animation.  
  Example:
```c++
AI_Output(self,other,"INTRO_Xardas_Speech_14_00"); // A single prisoner altered the fate of hundreds.
```

* `func void AI_OutputSVM(var C_Npc npc, var C_Npc target, var string svmname)`  
  Similar to AI_Output, but sound and text is determinated by SVM subsystem  
Example:
```c++
AI_OutputSVM(self,self,"$SMALLTALK01");
```

* `func void AI_OutputSVM_Overlay(var C_Npc npc, var C_Npc target, var string svmname)`  
  Similar to AI_OutputSVM, but works wih no animation.  

* ![#](nu.png) `func void AI_WaitTillEnd(var C_Npc npc, var C_Npc other)`  
  'npc' waits for 'other' to finish current AI_ command.  
  Not implemented. Seems to be unused in Gothic2.

* ![#](nu.png) `func void AI_Ask(var C_Npc npc, var func anserYes, var func answerNo)`  
  Not implemented.

* ![#](nu.png) `func void AI_AskText(var C_Npc npc, var func funcYes, var func funcNo, var string strYes, var string strNo)`  
  Not implemented.

* ![#](nu.png) `func void AI_WaitForQuestion(var C_Npc npc, var func scriptFunc)`  
  Not implemented.

* `func void AI_StopProcessInfos(var C_Npc npc)`  
  Stop dialog mode for player and 'npc'.  
  Takes no effect, if player doesn't speek with 'npc'.

* `func void AI_StartState(var C_Npc npc, var func state, var int doFinalize, var string wpName)`  
  starts a new state for npc, described by 'state' function.  
  if 'doFinalize' is non zero, current 'npc' state is going to be finalized, by 'STATE_End' function.  
  In Gothic state is triple of functions: init, loop, end.  

| function name | desription
---|---
| STATE | Run
| STATE_Loop | Called every second
| STATE_End | finalizer

* `func void AI_SetNpcsToState(var C_Npc npc, var fnuc aiStateFunc, var int radius)`  
  Set every C_Npc around 'npc' to new state described by 'aiStateFunc'.  
  Not recomended to use in original Gothic.

* `func void AI_ContinueRoutine(var C_Npc npc)`  
  'npc' returns back to his dayly routine.

* `func void AI_ReadySpell(var C_Npc npc, var int spellID, var int investMana)`  
  'npc' draw a spell with specified 'spellId'.

* ![#](nu.png) `func void AI_UnreadySpell(var C_Npc npc)`  
  Not implemented.

* ![#](nu.png) `func void AI_PlayCutscene(var C_Npc npc, var string csName)`  
  Not implemented.
  
* ![!](ni.png) `func AI_StopFx(var C_Npc npc, var string csName)`  
  Not implemented.

### inventory api
* `func void CreateInvItem(var C_Npc npc, var int itemId)`  
  Shortcut for CreateInvItems(npc,itemId,1).

* `func void CreateInvItems(var C_Npc npc, var int itemId, var int number)`  
  Add 'number' of items with class described by 'itemId' to 'npc' inventory.  
  Affect result of hlp_getinstanceid(itemId).  
  This function akes no effect, if 'number'<1

* `func void EquipItem(var C_Npc npc, var int itemId)`  
  Equip 'npc' with, an item of 'itemId' class. Can be used for armor/weapons/runes/etc  
  if Npc_HasItems(npc,itemId)==0, new item will be created  inside of 'npc' inventory.  
  Calls C_Item.on_equip callback, of specified item.  
  Function do nothing, if this item is already equipped.

### mobsi api
* ![#](nu.png) `func void Mob_CreateItems(var string mobName, var int itemId, var int amount)`  
  Create 'amount' of items with class 'itemId' inside a  chest with name 'mobName'.  
  Not implemented.

* `func int Mob_HasItems(var string mobName, var int itemId)`  
  Return count of items with class 'itemId', inside of the chest with name 'mobName'  

### world api
* `func int Wld_GetDay()`  
  Return days from start of adventure, starting from zero.

* `func int Wld_IsTime (var int hourS,var int minS,var int hourE,var int minE)`  
  Return TRUE, if current game time is in range [S .. E) or [E .. S).

* `func void Wld_InsertNpc(var int npcInstance,var string spawnPoint)`  
  Inserts an C_Npc object in world, at 'spawnPoint' location.

* ![#](nu.png) `func void Wld_InsertNpcAndRespawn(var int instance,var string spawnPoint,var float spawnDelay)`  
  Not implemented. Seems to be unused in original game.

* `func int Wld_IsMobAvailable(var C_Npc npc, var string schemeName)`  
  Checks if MOBSI object is available, in 10 meters around npc. Returns TRUE, if MOBSI has a free slot or npc already connected to this MOBSI.  

* ![!](ni.png) `func int Wld_GetMobState(var C_Npc self,var string schemeName)`  
  Not implemented.

* `func int Wld_IsFPAvailable(var C_Npc npc,var string fpName)`  
  Returns TRUE, if nearest to 'npc' free point, with name 'fpName' is available.  
  If point is locked by npc - also returns TRUE.

* `func int Wld_IsNextFPAvailable(var C_Npc npc,var string fpName)`  
  Same as Wld_IsFPAvailable, but current waypoint is going to be excluded from search.

* `func void Wld_InsertItem(var int itemInstance, var string spawnPoint)`  
  Inserts an C_Item object in world, at ‘spawnPoint’ location.

* ![#](nu.png) `func int Wld_RemoveItem(var C_Item item)`  
  Removes 'item' from world.  
  Not implemented.

* ![#](nu.png) `func int Wld_DetectPlayer(var C_Npc npc)`  
  Return TRUE, if player is near to 'npc'.  
  Not implemented.

* `func void Wld_SetGuildAttitude(var int guild1, var int attitude, var int guild2)`  
  Assign a new attitude value for pair of guilds.

* `func int Wld_GetGuildAttitude(var int guild1, var int guild2)`  
  Return a attitude betwen 'guild1' and 'guild2'.

* ![#](nu.png) `func void Wld_ExchangeGuildAttitudes(var string name)`  
  Not implemented.

* ![#](nu.png) `func void Wld_SetObjectRoutine(var int hour1, var int min1, var string objName, var int state)`  
  Not implemented.

* ![!](ni.png) `func void Wld_SetMobRoutine(var int hour1, var int min1, var strnig objName, var int state)`  
  Not implemented.

* `func int Wld_DetectNpc(var C_Npc npc, var int instance, var func aiState, var int guild)`  
  Detects an object of class `C_Npc` around 'npc' with specified  properties:  
  `dest.instance` must be equal to `instance`, if param `instance` is not -1  
  `dest.aiState` must be equal to `aiState`, if param `aiState` is not NOFUNC  
  `dest.guild` must be equal to `guild`, if param `guild` is not -1  
  If such `dest` exist, then function returns TRUE and write result to global variable `other`.

* `func int Wld_DetectNpcEx(var C_Npc npc, var int npcInstance, var func aiState, var int guild, var int detectPlayer)`  
  Same as Wld_DetectNpc, but excludes player from search, if `detectPlayer`!=TRUE.  

* ![!](ni.png) `func int Wld_DetectItem(var C_Npc npc, var int flags)`  
  Not implemented.

* `func void Wld_AssignRoomToGuild(var string room, var int guild)`  
  assigns a new 'guildId' to room, specified by 'room'

* ![#](nu.png) `func void Wld_AssignRoomToNpc(var string rootm, var C_Npc owner)`  
  Not implemented.

* ![#](nu.png) `func C_Npc Wld_GetPlayerPortalOwner()`  
  Not implemented.

* `func int Wld_GetPlayerPortalGuild()`  
  if 'player' located inside room, return guild assigned for this room.  
  otherwise returns GIL_NONE  
  same as `Npc_GetPortalGuild(player)`

* ![#](nu.png) `func C_Npc Wld_GetFormerPlayerPortalOwner()`  
  Not implemented.

* ![!](ni.png) `func int Wld_GetFormerPlayerPortalGuild()`  
  Not implemented.

* ![!](ni.png) `func void Wld_SpawnNpcRange(var C_Npc npc, var int clsId, var int count, var int lifetime)`  
  Not implemented.

* ![!](ni.png) `func bool Wld_IsRaining()`  
  Not implemented.

### view model api
* `func void Mdl_ApplyOverlayMds(var C_Npc npc, var string set)`  
  Override an animation set of 'npc' with animation-set 'set'  
Example
```c++
Mdl_ApplyOverlayMds(self,"HUMANS_FLEE.MDS"); // overrides run animation, to run very-very fast
```

* `func void Mdl_RemoveOverlayMDS(var C_Npc npc, var string set)`  
  Remove override-set, from npc.

* `func void Mdl_ApplyOverlayMDSTimed(var C_Npc npc, var string set, var float timeTicks)`  
  Override an animation set of 'npc' with animation-set 'set', for limited amout of time.  
  timeTicks - time in miliseconds.  
Example
```c++
Mdl_ApplyOverlayMdsTimed(self,"HUMANS_SPRINT.MDS",120000); // SchnellerHering potion effect
```

* ![!](ni.png) `func void Mdl_ApplyRandomAni(var C_Npc npc, var string s1, var string s2)`  
  Not implemented.

* ![!](ni.png) `func void Mdl_ApplyRandomAniFreq(var C_Npc npc, var string s1, var float f2)`  
  Not implemented.

* ![!](ni.png) `func void Mdl_StartFaceAni(var C_Npc npc, var string name, var float intensity, var float holdTime)`  
  Not implemented.

* ![#](nu.png) `func void Mdl_ApplyRandomFaceAni(var C_Npc self, var string name, var float timeMin, var float timeMinVar, var float timeMax, var float timeMaxVar, var float probMin)`  
  Not implemented.

* `func void Mdl_SetModelScale(var C_Npc npc, var float x, var float y, var float z)`  
  Change scale of 'npc' view mesh.

* `func void Mdl_SetModelFatness(var C_Npc npc, var float fatness)`  
  Set how "fat" this 'npc' is.  
  Takes no effect in OpenGothic for now.

### document api
* `func int Doc_Create()`  
  Allocate new document object.  
  Returns integer handle to created document.  

* `func int Doc_CreateMap()`  
  Allocate new document/map object.  
  Difference between 'Doc_Create' and 'Doc_CreateMap' is a cursor showing player position in world  
  Returns integer handle to created document.  

* `func void Doc_SetPages(var int document, var int count)`  
  Set pages count for document with handle 'document'  

* `func void Doc_SetPage(var int document, var int page, var string texture, var int scale)`  
  Set background texture for 'page' of document  
  If page is -1, then texture is going to be applyed to the document itself  

* `func void Doc_SetMargins(var int document, var int page, var int left, var int top, var int right, var int bottom, var int pixels)`  
  Set margins for 'page' of document  
  If page is -1, then margins is going to be applyed to the document itself  

* ![!](ni.png) `func void Doc_SetFont(var int document, var int page, var string font)`  
  Not implemented.

* ![!](ni.png) `func void Doc_SetLevel(var int document, var string level)`  
  Not implemented.

* ![!](ni.png) `func void Doc_SetLevelCoords(var int document, var int left, var int top, var int right, var int bottom)`  
  Not implemented.

* `func void Doc_PrintLine(var int document, var int page, var string text)`  
  Append single line 'text' block to 'page' of the 'document'  

* `func void Doc_PrintLines(var int document, var int page, var string text)`  
  Append multiline line 'text' block to 'page' of the 'document'  

* `func void Doc_Show(var int document)`  
  Present document with handle 'document' to screen.  
  Handle is no longer valid after this call.  

### ai-timetable
* ![#](nu.png) `func void TA(var c_npc self, var int start_h, var int stop_h, var func state, var string waypoint)`  
  Shortcut for TA_Min.  
  Not implemented.

* `func void TA_Min(var c_npc npc, var int start_h, var int start_m, var int stop_h, var int stop_m, var func state, var string waypoint)`  
  Add a new routine to 'npc' timetable. (start_h,start_m) - hour and minute to start; (stop_h,stop_m) - hour and minute to stop. 'state' - state function for this routine.  
  if stop time less then start time, then routine going to be performed over midnight.  

* ![#](nu.png) `func void TA_BeginOverlay(var C_Npc npc)`  
  Not implemented.

* ![#](nu.png) `func void TA_EndOverlay(var C_Npc npc)`  
  Not implemented.

* ![#](nu.png) `func void TA_RemoveOverlay(var C_Npc npc)`  
  Not implemented.

* ![#](nu.png) `func void TA_CS(var C_Npc npc, var string csName, var string roleName)`  
  csName - name of cutscene file  
  roleName - role for npc  
  Not implemented.

* ![#](nu.png) `func void Rtn_Exchange(var string oldRoutine, var string newRoutine)`  
  Not implemented.

### game session api

* `func void ExitGame()`  
  Shutdown game client

* ![!](ni.png) `func void ExitSession()`  
  Not implemented.

* ![!](ni.png) `func int PlayVideo(var string filename)`  
  Returns FALSE, if video not played  
  Empty implementation.

* ![!](ni.png) `func int PlayVideoEx(var string filename, var int screenBlend, var int exitSession)`  
  Returns FALSE, if video not played.  
  Not implemented.

* ![#](nu.png) `func void SetPercentDone(var int percentDone)`  
  Loading progress bar handled by OpenGothic implementation.  
  Function seems to be unused in Gothic2(legacy from G1?)  
  Not implemented.

* `func void IntroduceChapter(var string title, var string subtitle, var string texture, var string sound, var int waitTime)`  
  Shows chapter logo.  
  title - Title text  
  subtitle - subtitle text  
  texture - background image (800x600 resolution recommended)  
  sound - *.wav file to play  
  waitTime - time to display chapter window, in milliseconds  

### sound api
* `func void Snd_Play(var string scheme)`  
  Emit a global sound described by sound scheme(C_SFX) with name `scheme`.

* ![!](ni.png) `func void Snd_Play3D(var C_Npc npc, var string scheme)`  
  Emit a sound described by sound scheme(C_SFX) with name `scheme`.  
  Sound positioning in world is going to be attached to `npc` position.

* ![#](nu.png) `func int Snd_IsSourceNpc(var C_Npc npc)`  
  if `npc` is source of last sound then set global variable `other` to `npc` and return TRUE.  
  Not implemented.

* ![#](nu.png) `func int Snd_IsSourceItem(var C_Npc npc)`  
  Not implemented.

* `func int Snd_GetDistToSource(var C_Npc npc)`  
  Return distance from 'npc' to closest sound source.

### language api(1.30)
Effect of this functions, in original game, is unknown. OpenGothic do nothing.
* `func void Game_InitGerman()`  
  
* `func void Game_InitEnglish()`  
  

### debug  api
all functions of debug api in OpenGothic write info to `std::out`.
Note: there is no logging implementation for G1, since G1 spams too much.
* `func void PrintDebugInst(string text)`  
  Writes `test` to debugoutput.

* `func void PrintDebugInstCh(int ch, String text)`  
  Writes `test` and `ch` to debugoutput.

* `func void PrintDebugCh(int ch, String text)`  
  Writes `test` and `ch` to debugoutput.

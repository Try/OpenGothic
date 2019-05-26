## Daedalus scripts
daedalus is script language, made for original gothic game. OpenGothic uses VM from ZenLib library and implements most of game-engine script api:

### strings api
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

### ai module
Each npc in gothic have a ai-queue - sequence of action to perform sequentialy.
Example:
```c++
  AI_RemoveWeapon(diego);
  AI_Wait(diego,0.5);
  AI_UseItem(diego,ItPo_Health_03);
```
  In this example Diego will remove weapon, then wait for 500 ms, and then use healing potion
* func void AI_Wait(var C_Npc npc,var float s)
Wait for 's' seconds

### world api
* func int Wld_GetDay()
Return days from start of adventure, starting from zero.
* func int Wld_IsTime (var int hourS,var int minS,var int hourE,var int minE)
Return TRUE, if current game time is in range [S .. E) or [E .. S)
* func void Wld_InsertNpc(var int npcInstance,var string spawnPoint)
Inserts an C_Npc object in world, at 'spawnPoint' location
* func void Wld_InsertNpcAndRespawn(var int instance,var string spawnPoint,var float spawnDelay)

![!](ni.png) Not implemented. Seems to be unused in original game

### OpenGothic
Open source remake of Gothic 2: Night of the raven.
Motivation: Original Gothic 1 and Gothic 2 is still great games to play, but it not easy to make them work on modern systems.
The goal of this project is to make feature complete Gothic client-app compatible with game itself and regular mods for it.

----
![Screenshoot](doc/scr0.png)
##### Work in progress
Core gameplay is done, you can complete first chapter as paladin; mercenary/mage are not tested yet.

##### How to play
1. Install original gothic game from CD/Steam/GOG/etc  
*you have to install original game, since OpenGothic does not have any game assets or game scripts as built-in*
2. Build OpenGothic from source
3. run '/OpenGothic/bin/Gothic2Notr.exe -g "C:\Program Files (x86)\Path\To\Gothic II"'

##### Features
* General
    * Walk - Done
    * Walk(in water) - Not Implemented
    * Run - Done
    * Sneak - Not Implemented
    * Jump - Done
    * Jump(pull-up) - Not Implemented
    * Swimming - Not Implemented
    * Physic - mostly Done, with Bullet collision. Need small tweaks/tuning
* Loot
    * Pick an object - Partial(no animation)
    * Chest - Done
    * Ransack a body - Done
    * Object ownership/theft reaction - Not Implemented 
* Dialogs
    * Dialog script - Done
    * Trading - Done ( only G2 style, sorry G1 fans )
* Battle
    * Hit box - Partial(same bbox for everyone) 
    * Melee combat - Done
    * Range - Not Implemented
    * Magic - Not Implemented
* Visual
    * Body animation - Done
    * Head/Morph animation - Not Implemented
    * Shadows - in progress
    * Mob-use animations(seat/cook/sleep) - Partial
* UI
    * Inventory - Partial (no item priviews)
    * Game menu/menu script - Done, except scrollbars
    * Character info screen - done
    * Quest log - Not Implemented 
* Sound
    * animation sfx/gfx - Done
    * sound blockers - Done(but implementation is very simple)
    * music - in progress

##### Scripting
Mostly done: [list of scripts api functions](doc/script_api.md)

##### Mods compatibility
Mods delivered as *.mod files shoud work, since *.mod contains visual content and scripts.
Don't expect mods created with AST-SDK to work, since original Gothic and OpenGothic are not binary compatible.
Don't expect DirectX11 mod to work, since technicaly it's not a mod. But Project is aiming to have a good graphics with shaders out of box.


##### Command line arguments
* -g specify gothic game catalog
* -nomenu - skip main menu
* -w <worldname.zen> - startup world; newworld.zen is default
* -save <q> - startup with quick save
* -window - window mode
* -rambo - reduce damage to player to 1hp
* -v -validation - enable Vulkan validation mode

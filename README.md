### OpenGothic
Open source remake of Gothic 2: Night of the raven.
Motivation: The original Gothic 1 and Gothic 2 are still great games, but it not easy to make them work on modern systems.
The goal of this project is to make a feature complete Gothic client compatible with the game and mods.

----
[![Latest build](https://img.shields.io/github/release-pre/Try/opengothic?style=for-the-badge)](https://github.com/Try/opengothic/releases/latest)
![Screenshoot](scr0.png)
##### Work in progress
[![Build status](https://ci.appveyor.com/api/projects/status/github/Try/opengothic?svg=true)](https://ci.appveyor.com/project/Try/opengothic)  
Core gameplay is done, you can complete the first chapter, as well as all addon content for any guild. 

##### Install on Windows
1. Install original gothic game from CD/Steam/GOG/etc  
*you have to install original game, since OpenGothic does not have any game assets or game scripts as built-in*
2. [Download latest stable build](https://github.com/Try/opengothic/releases/latest)
3. run '/OpenGothic/bin/Gothic2Notr.exe -g "C:\Program Files (x86)\Path\To\Gothic II"'
Common Gothic installation paths:  
- "C:\Program Files (x86)\JoWooD\Gothic II"
- "C:\Gothic II"
- "C:\Program Files (x86)\Steam\steamapps\common\Gothic II"
- "~/PlayOnLinux's virtual drives/Gothic2_gog/drive_c/Gothic II"

##### Build it for Linux
1. Install dependencies for Ubuntu 20.04:

`sudo apt install git cmake g++ glslang-tools libvulkan-dev libasound2-dev libx11-dev`

2. Clone this repo, including submodules:

`git clone --recurse-submodules https://github.com/Try/OpenGothic.git`

3. Create build dir and build as usual:

`mkdir OpenGothic/build` --> `cd OpenGothic/build` --> `cmake .. -DCMAKE_BUILD_TYPE:STRING=RelWithDebInfo` --> `make`

#### Gameplay video
[![Video](https://img.youtube.com/vi/R9MNhNsBVQ0/0.jpg)](https://www.youtube.com/watch?v=R9MNhNsBVQ0) [![Video](https://img.youtube.com/vi/6BvwNkPMbwM/0.jpg)](https://www.youtube.com/watch?v=6BvwNkPMbwM)

##### Features
* General
    * Walk - Done
    * Walk(in water) - Done
    * Run - Done
    * Sneak - Done
    * Jump - Done
    * Jump(pull-up) - Done
    * Swimming - Done
    * Physic - mostly Done, with Bullet collision. Need small tweaks/tuning
* Loot
    * Pick an object - Done
    * Chest - Done
    * Ransack a body - Done
    * Object ownership/theft reaction - Partial
* Dialogs
    * Dialog script - Done
    * Trading - Done ( only G2 style, sorry G1 fans )
* Battle
    * Hit box - Partial(same bbox for everyone) 
    * Melee combat - Done
    * Range - Done
    * Magic - Partial (only some of spells working)
* Visual
    * Body animation - Done
    * Head/Morph animation - Not Implemented
    * Shadows - Done
    * Mob-use animations(seat/cook/sleep) - Partial
* UI
    * Inventory - Done
    * Game menu/menu script - Done, except scrollbars
    * Character info screen - Done
    * Quest log - Partial 
* Sound
    * animation sfx/gfx - Done
    * sound blockers - Done(but implementation is very simple)
    * music - Partial (kinda working, but sound is inexact)


##### Mods compatibility
Mods delivered as *.mod files shoud work, since *.mod contains visual content and scripts.  
Don't expect mods created with AST-SDK to work, since original Gothic and OpenGothic are not binary compatible.  
Don't expect DirectX11 mod to work, since technicaly it's not a mod. But Project is aiming to have a good graphics with shaders out of box.  


##### Command line arguments
* -g specify gothic game catalog
* -nomenu - skip main menu
* -nofrate - disable FPS display in-game
* -w <worldname.zen> - startup world; newworld.zen is default
* -save \<q> - startup with quick save
* -save \<number> - startup with specified save-game slot
* -window - window mode
* -rambo - reduce damage to player to 1hp
* -v -validation - enable Vulkan validation mode

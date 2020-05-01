### OpenGothic
Open source remake of Gothic 2: Night of the raven.
Motivation: Original Gothic 1 and Gothic 2 is still great games to play, but it not easy to make them work on modern systems.
The goal of this project is to make feature complete Gothic client-app compatible with game itself and regular mods for it.

----
[![Latest build](https://img.shields.io/github/release-pre/Try/opengothic?style=for-the-badge)](https://github.com/Try/opengothic/releases/latest)
![Screenshoot](scr0.png)
##### Work in progress
[![Build status](https://ci.appveyor.com/api/projects/status/github/Try/opengothic?svg=true)](https://ci.appveyor.com/project/Try/opengothic)  
Core gameplay is done, you can complete first chapter, as well as all addon content for any guild. 

##### Install on Windows
1. Install original gothic game from CD/Steam/GOG/etc  
*you have to install original game, since OpenGothic does not have any game assets or game scripts as built-in*
2. [Download latest stable build](https://github.com/Try/opengothic/releases/latest)
3. run '/OpenGothic/bin/Gothic2Notr.exe -g "C:\Program Files (x86)\Path\To\Gothic II"'

##### Build it for Linux
1. Install dependencies:

* for Ubuntu:
`sudo apt install cmake g++ doxygen glslang-tools libgl-dev`

* for openSUSE:
`sudo zypper in git cmake gcc-c++ glslang-devel doxygen Mesa-devel`

2. Clone this repo, including submodules:

`git clone --recurse-submodules https://github.com/Try/OpenGothic.git`

3. Create build dir and build as usual:

`mkdir OpenGothic/build` --> `cd OpenGothic/build` --> `cmake ..` --> `make`

#### Gameplay video
[![Video](https://img.youtube.com/vi/21OTvuMdwb4/0.jpg)](https://www.youtube.com/watch?v=21OTvuMdwb4) [![Video](https://img.youtube.com/vi/MUVd-ZWliKY/0.jpg)](https://www.youtube.com/watch?v=MUVd-ZWliKY)

##### Features
* General
    * Walk - Done
    * Walk(in water) - Done
    * Run - Done
    * Sneak - Not Implemented
    * Jump - Done
    * Jump(pull-up) - Not Implemented
    * Swimming - Partial
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
    * Quest log - Not Implemented 
* Sound
    * animation sfx/gfx - Done
    * sound blockers - Done(but implementation is very simple)
    * music - in progress


##### Mods compatibility
Mods delivered as *.mod files shoud work, since *.mod contains visual content and scripts.  
Don't expect mods created with AST-SDK to work, since original Gothic and OpenGothic are not binary compatible.  
Don't expect DirectX11 mod to work, since technicaly it's not a mod. But Project is aiming to have a good graphics with shaders out of box.  


##### Command line arguments
* -g specify gothic game catalog
* -nomenu - skip main menu
* -nofrate - Disable FPS display in-game
* -w <worldname.zen> - startup world; newworld.zen is default
* -save \<q> - startup with quick save
* -save \<number> - startup with specified save-game slot
* -window - window mode
* -rambo - reduce damage to player to 1hp
* -v -validation - enable Vulkan validation mode

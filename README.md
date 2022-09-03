### OpenGothic
Open source remake of Gothic 2: Night of the raven.
Motivation: The original Gothic 1 and Gothic 2 are still great games, but it not easy to make them work on modern systems.  
The goal of this project is to make a feature complete Gothic game client compatible with the game and mods.

----
[![Latest build](https://img.shields.io/github/release-pre/Try/opengothic?style=for-the-badge)](https://github.com/Try/opengothic/releases/latest)
![Screenshoot](scr0.png)
##### Work in progress
[![Build status](https://ci.appveyor.com/api/projects/status/github/Try/opengothic?svg=true)](https://ci.appveyor.com/project/Try/opengothic)  

The base game has been replicated fully: you can complete the main quest and addon. 
Check out the [bugtracker](https://github.com/Try/OpenGothic/issues) for list of known issues.

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
```bash
# 1. Install dependencies
# 1.1 Latest vulkan SDK
wget -qO - http://packages.lunarg.com/lunarg-signing-key-pub.asc | sudo apt-key add -
sudo wget -qO /etc/apt/sources.list.d/lunarg-vulkan-focal.list http://packages.lunarg.com/vulkan/lunarg-vulkan-focal.list
sudo apt update
sudo apt install vulkan-sdk
# 1.2 Ubuntu 20.04:
sudo apt install git cmake g++ glslang-tools libvulkan-dev libasound2-dev libx11-dev libxcursor-dev
# 1.3 Arch:
sudo pacman -S git cmake gcc glslang vulkan-devel alsa-lib libx11 libxcursor vulkan-icd-loader libglvnd
# 2. Clone this repo, including submodules:
git clone --recurse-submodules https://github.com/Try/OpenGothic.git
# 2.1 if you pull a new version:
git pull --recurse-submodules
# 3. Create build dir and build as usual:
cd OpenGothic
cmake -B build -DBUILD_SHARED_LIBS=ON -DCMAKE_BUILD_TYPE:STRING=RelWithDebInfo
make -C build -j <number_of_cpucores>
# 4.
# locate executables at OpenGothic/build/opengothic
```

#### Gameplay video
[![Video](https://img.youtube.com/vi/R9MNhNsBVQ0/0.jpg)](https://www.youtube.com/watch?v=R9MNhNsBVQ0) [![Video](https://img.youtube.com/vi/6BvwNkPMbwM/0.jpg)](https://www.youtube.com/watch?v=6BvwNkPMbwM)

##### Mods compatibility
- [x] Content mods (retexture/reworld/animations)
- [ ] Mods bases on Ikarus/LeGo 
- [ ] AST sdk
- [ ] Mods bases on Union (not possible)
- [ ] DirectX11 - same as Union, but don't worry - OpenGothic has nice graphics out of the box

##### Command line arguments
* -g specify gothic game catalog
* -game:<modfile.init> specify game modification manifest (GothicStarter compatibility)
* -nomenu - skip main menu
* -nofrate - disable FPS display in-game
* -w <worldname.zen> - startup world; newworld.zen is default
* -save \<q> - startup with quick save
* -save \<number> - startup with specified save-game slot
* -window - window mode
* -v -validation - enable Vulkan validation mode
* -g1 - assume a Gothic 1 installation
* -g2 - assume a Gothic 2 installation
* -rt \<boolean> - explicitly enable or disable ray-query
* -ms \<boolean> - explicitly enable or disable meshlets

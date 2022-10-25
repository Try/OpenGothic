## OpenGothic
Open source remake of Gothic 2: Night of the Raven.

Motivation: The original Gothic 1 and Gothic 2 are still great games, but it's not easy to make them work on modern systems.
The goal of this project is to make a feature complete Gothic game client compatible with the original game data and mods.

----
[![Latest build](https://img.shields.io/github/release-pre/Try/opengothic?style=for-the-badge)](https://github.com/Try/opengothic/releases/latest)
![Screenshoot](scr0.png)

### Work in progress
[![Build status](https://ci.appveyor.com/api/projects/status/github/Try/opengothic?svg=true)](https://ci.appveyor.com/project/Try/opengothic)

The original game has been replicated fully; you can complete both the main quest and the addon.
Check out the [bugtracker](https://github.com/Try/OpenGothic/issues) for a list of known issues.

### Install on Windows
1. Install original Gothic game from CD/Steam/GOG/etc.
*you have to install the original game as OpenGothic does not provide any built-in game assets or game scripts*
2. [Download latest stable build](https://github.com/Try/opengothic/releases/latest)
3. Run '/OpenGothic/bin/Gothic2Notr.exe -g "C:\Program Files (x86)\Path\To\Gothic II"'

Common Gothic installation paths to be provided via `-g`:
- "C:\Program Files (x86)\JoWooD\Gothic II"
- "C:\Gothic II"
- "C:\Program Files (x86)\Steam\steamapps\common\Gothic II"
- "~/PlayOnLinux's virtual drives/Gothic2_gog/drive_c/Gothic II"

### Build on Linux
#### Dependencies
* Ubuntu 20.04
```bash
# latest Vulkan SDK provided externally as Ubuntu packages are usually older
wget -qO - http://packages.lunarg.com/lunarg-signing-key-pub.asc | sudo apt-key add -
sudo wget -qO /etc/apt/sources.list.d/lunarg-vulkan-focal.list http://packages.lunarg.com/vulkan/lunarg-vulkan-focal.list
sudo apt update
sudo apt install vulkan-sdk

# distro-provided packages
sudo apt install git cmake g++ glslang-tools libvulkan-dev libasound2-dev libx11-dev libxcursor-dev
```

* Arch
```bash
sudo pacman -S git cmake gcc glslang vulkan-devel alsa-lib libx11 libxcursor vulkan-icd-loader libglvnd
```

* Fedora
```bash
sudo dnf install git cmake gcc-c++ glslang vulkan-loader-devel alsa-lib-devel libX11-devel libXcursor-devel vulkan-validation-layers-devel libglvnd-devel
```

#### Compilation
```bash
# 1st time build:
git clone --recurse-submodules https://github.com/Try/OpenGothic.git
cd OpenGothic
cmake -B build -DBUILD_SHARED_LIBS=ON -DCMAKE_BUILD_TYPE:STRING=RelWithDebInfo
make -C build -j $(nproc)

# following builds:
git pull --recurse-submodules
make -C build -j $(nproc)

# locate the executables at OpenGothic/build/opengothic
```

### Gameplay video
[![Video](https://img.youtube.com/vi/R9MNhNsBVQ0/0.jpg)](https://www.youtube.com/watch?v=R9MNhNsBVQ0)
[![Video](https://img.youtube.com/vi/6BvwNkPMbwM/0.jpg)](https://www.youtube.com/watch?v=6BvwNkPMbwM)

### Mods compatibility
- [x] Content mods (retexture/reworld/animations)
- [ ] Mods bases on Ikarus/LeGo
- [ ] AST sdk
- [ ] Mods bases on Union (not possible)
- [ ] DirectX11 - same as Union, but don't worry - OpenGothic has nice graphics out of the box

### Command line arguments
| Argument(s)            | Description                                                      |
| ---------------------- | -------                                                          |
| `-g`                   | specify path containing Gothic game data                         |
| `-game:<modfile.init>` | specify game modification manifest (GothicStarter compatibility) |
| `-nomenu`              | skip main menu                                                   |
| `-nofrate`             | disable FPS display in-game                                      |
| `-w <worldname.zen>`   | startup world; newworld.zen is default                           |
| `-save q`              | load the quick save on start                                     |
| `-save <number>`       | load a specified save-game slot on start                         |
| `-v -validation`       | enable Vulkan validation mode                                    |
| `-dx12`                | force DirectX 12 renderer instead of Vulkan (Windows only)       |
| `-g1`                  | assume a Gothic 1 installation                                   |
| `-g2`                  | assume a Gothic 2 installation                                   |
| `-rt <boolean>`        | explicitly enable or disable ray-query                           |
| `-ms <boolean>`        | explicitly enable or disable meshlets                            |
| `-window`              | windowed debugging mode (not to be used for playing)             |

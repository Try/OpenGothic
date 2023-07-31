## OpenGothic
Open source re-implementation of Gothic 2: Night of the Raven.

Motivation: The original Gothic 1 and Gothic 2 are still great games, but it's not easy to make them work on modern systems.
The goal of this project is to make a feature complete Gothic game client compatible with the original game data and mods.

----
[![Latest build](https://img.shields.io/github/release-pre/Try/opengothic?style=for-the-badge)](https://github.com/Try/opengothic/releases/latest)
![Screenshoot](scr0.png)

#### Work in progress
[![Build status](https://ci.appveyor.com/api/projects/status/github/Try/opengothic?svg=true)](https://ci.appveyor.com/project/Try/opengothic)

The original game has been replicated fully; you can complete both the main quest and the addon.
Check out the [bugtracker](https://github.com/Try/OpenGothic/issues) for a list of known issues.

Development is focused on Gothic 2 and new features are not tested for Gothic 1 compatibility. While Gothic 1 is not officially supported pull requests that fix Gothic 1 — and general — bugs are welcome.

#### Prerequisites
An original Gothic game installation as OpenGothic does not provide any built-in game assets or scripts.

Supported systems and graphic api's are Windows (DX12, Vulkan), Linux (Vulkan) and MacOS (Metal).

#### Performance
OpenGothic is known to run poorly on Intel integrated graphics and is barely playable (<5 Fps) on Low-End devices such as Raspberry-Pi. Mainstream graphics cards like a GTX 1060 can run the game mostly at 100+ Fps in FullHD. Perfomance related ingame menu options are `Cloud Shadows` (SSAO, off by default) and `Radial Fog` (on). Raytracing is enabled by default if capable hardware is detected. To turn it off use `-rt 0` command line argument.

## How to play
### Windows
1. Download a [Pre-Release](https://github.com/Try/opengothic/releases/latest) or build from [CI](https://ci.appveyor.com/project/Try/opengothic/history) and extract into a folder of your choice.
2. Open `Gothic2Notr.bat`:

   `Gothic2Notr.exe -g "C:\Program Files (x86)\Steam\steamapps\common\Gothic II"`   

    Change path to your installation location. Common Gothic installation paths are:
 * "C:\Program Files (x86)\JoWooD\Gothic II"
 * "C:\Gothic II"
 * "C:\Program Files (x86)\Steam\steamapps\common\Gothic II"
3. Run `Gothic2Notr.bat`

### Linux/MacOS
1. Download a build from [CI](https://ci.appveyor.com/project/Try/opengothic/history) and extract into a folder of your choice.
2. Open `Gothic2Notr.sh`:

   ```bash
   #!/usr/bin/env bash
   DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
   export LD_LIBRARY_PATH="$DIR:$LD_LIBRARY_PATH"
   export DYLD_LIBRARY_PATH="$DIR:$DYLD_LIBRARY_PATH"
   if [[ $DEBUGGER != "" ]]; then
     exec $DEBUGGER --args "$DIR/Gothic2Notr" "$@"
   else
     exec "$DIR/Gothic2Notr" "$@"
   fi
   ```
   Use `-g` parameter and change the line `exec "$DIR/Gothic2Notr" "$@"` to reflect your Gothic path e.g.

   `exec "$DIR/Gothic2Notr" "$@" -g "~/PlayOnLinux's virtual drives/Gothic2_gog/drive_c/Gothic II"`

3. Run `Gothic2Notr.sh`
### Modifications
Mods can be installed as usual. Provide the `modfile.ini` to OpenGothic via `-game:` parameter to play. Example:

`Gothic2Notr.exe -g "C:\Program Files (x86)\Steam\steamapps\common\Gothic II" -game:Karibik.ini`

#### What's working?
Content mods (retexture/reworld/animations) that only rely on regular scripting and do not use memory hacking.

#### What's not?
- Ikarus/LeGo

There are ongoing efforts to support parts of it to make at least some popular mods like `Chronicles of Myrtana` playable. Progress can be tracked in the corresponding [issue](https://github.com/Try/OpenGothic/issues/231). An explanantion how Ikarus works is given [here](https://github.com/Try/OpenGothic/discussions/396#discussioncomment-4823499).
- Union (32 bit and Windows only, [not possible](https://github.com/Try/OpenGothic/issues/195))
- DX11 Renderer - same as Union, but don't worry - OpenGothic has nice graphics out of the box
- AST sdk
- Ninja

## Build Instructions
### Linux
Install dependencies:
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
```
Executables can be located at `OpenGothic/build/opengothic`.

### MacOS
```bash
brew install glslang
git clone --recurse-submodules https://github.com/Try/OpenGothic.git
cd OpenGothic
cmake -H. -Bbuild -DCMAKE_BUILD_TYPE:STRING=RelWithDebInfo
cmake --build ./build --target Gothic2Notr
```
Executables can be located at `OpenGothic/build/opengothic`.

## Video
[![Video](https://img.youtube.com/vi/TpayMkyZ58Y/0.jpg)](https://www.youtube.com/watch?v=TpayMkyZ58Y)

## Command line arguments
| Argument(s)            | Description                                                      |
| ---------------------- | -------                                                          |
| `-g`                   | specify path containing Gothic game data                         |
| `-game:<modfile.init>` | specify game modification manifest (GothicStarter compatibility) |
| `-nomenu`              | skip main menu                                                   |
| `-w <worldname.zen>`   | startup world; newworld.zen is default                           |
| `-save q`              | load the quick save on start                                     |
| `-save <number>`       | load a specified save-game slot on start                         |
| `-v -validation`       | enable validation mode                                    |
| `-dx12`                | force DirectX 12 renderer instead of Vulkan (Windows only)       |
| `-g1`                  | assume a Gothic 1 installation                                   |
| `-g2`                  | assume a Gothic 2 installation                                   |
| `-rt <boolean>`        | explicitly enable or disable ray-query                           |
| `-ms <boolean>`        | explicitly enable or disable meshlets                            |
| `-window`              | windowed debugging mode (not to be used for playing)             |

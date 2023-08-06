## OpenGothic
Open source re-implementation of Gothic 2: Night of the Raven.

Motivation: The original Gothic 1 and Gothic 2 are still great games, but it's not easy to make them work on modern systems.
The goal of this project is to make a feature complete Gothic game client compatible with the original game data and mods.

----
[![Latest build](https://img.shields.io/github/release-pre/Try/opengothic?style=for-the-badge)](https://github.com/Try/opengothic/releases/latest)
![Screenshoot](scr0.png)

#### Work in progress
[![Build status](https://ci.appveyor.com/api/projects/status/github/Try/opengothic?svg=true)](https://ci.appveyor.com/project/Try/opengothic)

Development is focused on Gothic 2 and new features are not tested for Gothic 1 compatibility. While Gothic 1 is not officially supported pull requests that fix Gothic 1 — and general — bugs are welcome.

The original game has been replicated fully; you can complete both the main quest and the addon. Check out the [bugtracker](https://github.com/Try/OpenGothic/issues) for a list of known issues.

OpenGothic is designed to utilize features of modern graphic hardware and api's like mesh shaders or ray tracing. Not mandatory but don't expect OpenGothic to run well on low-end or outdated graphic cards.

#### Prerequisites

Gothic 2: Night of the Raven is required as OpenGothic does not provide any built-in game assets or scripts.

Supported systems are:
* Windows (DX12, Vulkan)
* Linux (Vulkan)
* MacOS (Metal)

## How to play
### Windows
1. If not already done install Gothic. OpenGothic comes with auto-path detection if your Gothic files are in a common path.
    * "C:\Program Files (x86)\JoWooD\Gothic II"
    * "C:\Gothic II"
    * "C:\Program Files (x86)\Steam\steamapps\common\Gothic II"
2. Download OpenGothic and extract into a folder of your choice. Available options are:
    * A [Pre-Release](https://github.com/Try/opengothic/releases/latest) (recommended)
    * Alternatively a recent test build from [CI](https://ci.appveyor.com/project/Try/opengothic/history)
3. Run `Gothic2Notr.exe`.

   If nothing happens check `log.txt` and look for the line `invalid gothic path`. In this case OpenGothic fails to find your Gothic installation and you have to explicitly specify its location via `-g` paramter. Either you create a shortcut to `Gothic2Notr.exe` and change the target line in Properties to e.g.

   `Gothic2Notr.exe -g "C:\Program Files (x86)\Steam\steamapps\common\Gothic II"`

   or you can edit `Gothic2Notr.bat` and run this file instead.

### Linux
1. If not already done install Gothic via Wine/Proton or copy the game files from a Windows installation.
2. You can download a build from [CI](https://ci.appveyor.com/project/Try/opengothic/history) and extract into a folder of your choice. Alternatively OpenGothic can be built manually. For Arch the [AUR](https://aur.archlinux.org/packages/opengothic) provides a 3rd party package.

3. Run `Gothic2Notr.sh -g "~/PlayOnLinux's virtual drives/Gothic2_gog/drive_c/Gothic II"` (example path, use path to your Gothic installation instead)

   You can edit `Gothic2Notr.sh` and change the line `exec "$DIR/Gothic2Notr" "$@"` to

   `exec "$DIR/Gothic2Notr" "$@" -g "~/PlayOnLinux's virtual drives/Gothic2_gog/drive_c/Gothic II"`

   to not have to enter path manually every time. Then run `Gothic2Notr.sh` without arguments to start.

### MacOS
1. If not already done install Gothic. Instructions how to obtain the game files can be found [here](https://macsourceports.com/faq#getgamedata). OpenGothic comes with auto-path detection if your Gothic files are in `"~/Library/Application Support/OpenGothic"`.
2. Download a build from [Mac Source Ports](https://macsourceports.com/game/gothic2) and follow the installation instructions given there.
   Alternatively recent test builds are available from [CI](https://ci.appveyor.com/project/Try/opengothic/history) that can be extracted into a folder of your choice. You can compile a fresh build as well.
3. Run `Gothic2Notr.sh`

   If OpenGothic fails to find your Gothic files you have to explicitly specify its location via `-g` parameter.
   Change the line `exec "$DIR/Gothic2Notr" "$@"` to reflect your Gothic path e.g.

   `exec "$DIR/Gothic2Notr" "$@" -g "~/PlayOnLinux's virtual drives/Gothic2_gog/drive_c/Gothic II"`

---
### Modifications
Mods can be installed as usual. Provide the `modfile.ini` to OpenGothic via `-game:` parameter to play. Example:

`Gothic2Notr.exe -game:Karibik.ini`

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

## Available Graphic options

*  SSAO (mapped to `Cloud Shadows`)
*  Volumetric fog (mapped to `Radial Fog`)
*  Reflections
*  Ray tracing (commandline option, setting affects only capable hardware, off by default for igpu's, add `-rt 1` to enable)

Rendering distance is not customizable.

## Command line arguments
| Argument(s)            | Description                                                      |
| ---------------------- | -------                                                          |
| `-g`                   | specify path containing Gothic game data                         |
| `-game:<modfile.init>` | specify game modification manifest (GothicStarter compatibility) |
| `-nomenu`              | skip main menu                                                   |
| `-w <worldname.zen>`   | startup world; newworld.zen is default                           |
| `-save q`              | load the quick save on start                                     |
| `-save <number>`       | load a specified save-game slot on start                         |
| `-v -validation`       | enable validation layers for graphics api                        |
| `-dx12`                | force DirectX 12 renderer instead of Vulkan (Windows only)       |
| `-g1`                  | assume a Gothic 1 installation                                   |
| `-g2`                  | assume a Gothic 2 installation                                   |
| `-rt <boolean>`        | explicitly enable or disable ray-query                           |
| `-ms <boolean>`        | explicitly enable or disable meshlets                            |
| `-window`              | windowed debugging mode (not to be used for playing)             |

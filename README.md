## OpenGothic
Open source re-implementation of Gothic 2: Night of the Raven.

Motivation: The original Gothic 1 and Gothic 2 are still great games, but it's not easy to make them work on modern systems.
The goal of this project is to make a feature-complete Gothic game client, compatible with the original game data and mods.

----
[![Latest build](https://img.shields.io/github/release-pre/Try/opengothic?style=for-the-badge)](https://github.com/Try/opengothic/releases/latest)
[![](https://dcbadge.vercel.app/api/server/G9XvcFQnn6)](https://discord.gg/G9XvcFQnn6)

![Screenshoot](scr0.png)

#### Work in progress
[![Build status](https://img.shields.io/github/actions/workflow/status/Try/OpenGothic/build.yml?branch=master)](https://github.com/Try/OpenGothic/actions?query=build.yml?query=branch%3Amaster)


Development is focused on Gothic 2 and new features are not tested for compatibility with Gothic 1. While Gothic 1 is not officially supported, pull requests that fix Gothic 1 — and general — bugs are welcome.

The original game has been completely replicated; you can complete both the main quest and the addon. Check out the [bugtracker](https://github.com/Try/OpenGothic/issues) for a list of known issues.

OpenGothic is designed to utilize features of modern graphics hardware and APIs, like mesh shaders or ray tracing. While mesh shaders are not mandatory, don't expect OpenGothic to run well on low-end or outdated graphics cards.

#### Prerequisites

Gothic 2: Night of the Raven is required as OpenGothic does not provide any built-in game assets or scripts.

Supported systems are:
* Windows (DX12, Vulkan)
* Linux (Vulkan)
* MacOS (Metal)

## How to play
### Windows
1. If not already done, install Gothic 2. OpenGothic comes with automatic path detection if your Gothic files are in a common path.
    * "C:\Program Files (x86)\JoWooD\Gothic II"
    * "C:\Gothic II"
    * "C:\Program Files (x86)\Steam\steamapps\common\Gothic II"
2. Download OpenGothic and extract it into a folder of your choice. Available options are:
    * A [Pre-Release](https://github.com/Try/opengothic/releases/latest) (recommended)
    * Alternatively a recent test build from [CI](https://github.com/Try/OpenGothic/actions?query=build.yml?query=branch%3Amaster)
3. Run `Gothic2Notr.exe`.

   If nothing happens, check `log.txt` and look for the line `invalid gothic path`. In this case OpenGothic fails to find your Gothic installation and you have to explicitly specify its location via `-g` parameter. Either you create a shortcut to `Gothic2Notr.exe` and change the target line in Properties to e.g.

   `Gothic2Notr.exe -g "C:\Program Files (x86)\Steam\steamapps\common\Gothic II"`

   or you can edit `Gothic2Notr.bat` and run this file instead.

### Linux
1. If not already done, install Gothic 2 via Wine/Proton or copy the game files from a Windows installation.
2. Install OpenGothic using one of these options:
   * **Debian/Ubuntu**: Download the `.deb` package from [Releases](https://github.com/Try/opengothic/releases/latest) and install with `sudo dpkg -i opengothic_*.deb`
   * **Arch**: Install from [AUR](https://aur.archlinux.org/packages/opengothic)
   * **Other distros**: Download the portable build from [CI](https://github.com/Try/OpenGothic/actions?query=build.yml?query=branch%3Amaster) or build manually

3. Run `Gothic2Notr -g "~/PlayOnLinux's virtual drives/Gothic2_gog/drive_c/Gothic II"` (example path, use the path to your Gothic 2 installation instead)

   For the portable build, you can edit `Gothic2Notr.sh` and change the line `exec "$DIR/Gothic2Notr" "$@"` to

   `exec "$DIR/Gothic2Notr" "$@" -g "~/PlayOnLinux's virtual drives/Gothic2_gog/drive_c/Gothic II"`

   to not have to enter the path manually every time. Then run `Gothic2Notr.sh` without arguments to start.

### MacOS
1. If not already done, install Gothic 2. Instructions on how to obtain the game files can be found [here](https://macsourceports.com/faq#getgamedata). OpenGothic comes with automatic path detection if your Gothic files are in `"~/Library/Application Support/OpenGothic"`.
2. Download a build from [Mac Source Ports](https://macsourceports.com/game/gothic2) and follow the installation instructions given there.
   Alternatively, recent test builds are available from [CI](https://github.com/Try/OpenGothic/actions?query=build.yml?query=branch%3Amaster) and can be extracted into a folder of your choice. You can compile a fresh build as well.
3. Run `Gothic2Notr.sh`

   If OpenGothic fails to find your Gothic 2 files, you have to explicitly specify its location via `-g` parameter.
   Change the line `exec "$DIR/Gothic2Notr" "$@"` to reflect your Gothic 2 path e.g.

   `exec "$DIR/Gothic2Notr" "$@" -g "~/PlayOnLinux's virtual drives/Gothic2_gog/drive_c/Gothic II"`

---
### Modifications
Mods can be installed as usual. Provide the `modfile.ini` to OpenGothic via the `-game:` parameter to play. Example:

`Gothic2Notr.exe -game:Karibik.ini`

#### What's working?
Content mods (retexture/reworld/animations) that only rely on regular scripting and do not use memory hacking.

#### What's not?
- Ikarus/LeGo

There are ongoing efforts to support parts of it to make at least some popular mods, like `Chronicles of Myrtana`, playable. Progress can be tracked in the corresponding [issue](https://github.com/Try/OpenGothic/issues/231). An explanantion how Ikarus works is given [here](https://github.com/Try/OpenGothic/discussions/396#discussioncomment-4823499).
- Union (32 bit and Windows only, [not possible](https://github.com/Try/OpenGothic/issues/195))
- DX11 Renderer - same as Union, but don't worry - OpenGothic has nice graphics out of the box
- AST sdk
- Ninja

## Build Instructions
### Linux
Install dependencies:
* Ubuntu 20.04/22.04 and their derived distros
```bash
source <(cat /etc/os-release | grep UBUNTU_CODENAME)
# latest Vulkan SDK provided externally as Ubuntu packages are usually older
wget -qO - http://packages.lunarg.com/lunarg-signing-key-pub.asc | sudo apt-key add -
sudo wget -qO /etc/apt/sources.list.d/lunarg-vulkan-${UBUNTU_CODENAME}.list http://packages.lunarg.com/vulkan/lunarg-vulkan-${UBUNTU_CODENAME}.list
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

The game menu in Gothic 2 is done by scripting. While the original graphics settings do not apply to current-gen graphics, the engine redefines meaning for some tweakables:

* `Settings` -> `Extended configuration`
  *  `Cloud Shadows` mapped to SSAO.
  *  `Radial Fog` mapped to "Volumetric fog"
  *  `Reflections` reused, for screen space reflections
* `Settings` -> `Video settings`
  *  Internal rendering resolution, for 3D, can be altered here. UI and text is always rendered at full resolution.
* Application command line:
  *  Ray tracing: setting affects only capable hardware, off by default for igpu's, add `-rt 1` to enable

Rendering distance is not customizable.

## Command line arguments
| Argument(s)            | Description                                                      |
| ---------------------- | -------                                                          |
| `-g`                   | specify path containing Gothic game data                         |
| `-game:<modfile.ini>`  | specify game modification manifest (GothicStarter compatibility) |
| `-nomenu`              | skip main menu                                                   |
| `-devmode`             | enable marvin-mode at start of the game                          |
| `-w <worldname.zen>`   | startup world; newworld.zen is default                           |
| `-save q`              | load the quick save on start                                     |
| `-save <number>`       | load a specified save-game slot on start                         |
| `-v -validation`       | enable validation layers for graphics api                        |
| `-dx12`                | force DirectX 12 renderer instead of Vulkan (Windows only)       |
| `-g1`                  | assume a Gothic 1 installation                                   |
| `-g2c`                 | assume a Gothic 2 classic installation                           |
| `-g2`                  | assume a Gothic 2 night of the raven installation                |
| `-rt <boolean>`        | explicitly enable or disable ray-query                           |
| `-gi <boolean>`        | explicitly enable or disable ray-traced global illumination      |
| `-ms <boolean>`        | explicitly enable or disable meshlets                            |
| `-aa <number>`         | enable anti-aliasing (number = 1-2, 2 = most expensive AA)       |
| `-window`              | windowed debugging mode (not to be used for playing)             |

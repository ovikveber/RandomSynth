# OV RandomSynth

**语言 / Language:** [中文](#zh) | [English](#en)

---

<a id="zh"></a>

## 中文

一个**粒子碰撞驱动的生成式音序器**。

小球在封闭场地内运动，与墙壁和其他小球碰撞。每次碰撞都会交换音高、包络和效果属性，生成波、场和火花，并产生音符/触发事件。最终得到的是持续变化、随机但不会跑调的旋律与节奏。

本仓库的主要目标是 **VCV Rack 模块**。`Others/` 里是 JUCE 独立版、Python 视觉原型和本地构建脚本。

### 功能

- 默认 12 个小球（最多 32 个），在封闭场地内运动并碰撞
- 三类小球，按比例分布：**音高**、**ADSR**、**效果**
- 碰撞会交换属性，并生成波、场和火花
- 音高范围 **0.5–8 八度**，带 `FINE` 开关；关闭 FINE 时按 **0.5 八度**步进
- 音阶量化：
  - `FREE`：完全随机音高，可输出微分音
  - 常见调式：Major、Minor、Dorian、Phrygian、Lydian、Mixolydian、Locrian、Harmonic Minor、Melodic Minor
  - 五声音阶、Blues、Whole Tone、Diminished、Chromatic
  - 5-limit 纯律：Just Major / Just Minor
  - 微分音 EDO：24、31、48、72 EDO
- 场地形状：圆形、矩形、椭圆、六边形
- 场地缩放、小球大小、小球速度、水波速度、水波范围
- 附近小球之间会共享速度
- Booster 加速器：碰到小球会将其弹开
- VCV 输出：**V/OCT**、**GATE**、**TRIG**、**FX**

### 直接安装（Windows x64，免编译）

下载仓库中的：

`VCV-Module/RandomSynth-2.0.0-win-x64.vcvplugin`

把这个 `.vcvplugin` 文件直接拖进 VCV Rack 2.x 窗口即可加载。

注意：**源码文件夹不能直接拖进 Rack**，Rack 只识别编译好的 `.vcvplugin`，或包含 `plugin.dll` + `plugin.json` 的插件文件夹。

### 仓库结构

```
VCV-Module/              VCV Rack 模块（开源主体，编译这个）
  Makefile
  plugin.json
  src/
  RandomSynth-2.0.0-win-x64.vcvplugin   预编译包
Others/                  其他版本（原型、构建脚本、本地产物）
  CMakeLists.txt
  build.ps1
  src/                   JUCE 独立版原型
  visual_prototype.py    Python 视觉原型
  *.bat                  本地运行脚本
```

只有 `VCV-Module/` 是构建 VCV 插件所必需的。`Others/` 放的是其他版本和本地产物。

### 从源码编译 VCV 模块

需要：

- [VCV Rack SDK](https://vcvrack.com/manual/Building) 2.x
- `make`
- 支持 C++17 的编译器（Windows 下推荐 MSYS2 MinGW-w64）

```sh
cd VCV-Module

# 编译 plugin.dll
make RACK_DIR=/path/to/Rack-SDK

# 打包 .vcvplugin
make dist RACK_DIR=/path/to/Rack-SDK
```

Windows + MSYS2 下 `RACK_DIR` 通常类似：

```sh
make RACK_DIR=/d/Rack-SDK/Rack-SDK
```

生成的 `.vcvplugin` 可以通过 Rack 菜单安装，或复制到 Rack 用户插件目录。

### 编译 JUCE 独立版（可选）

JUCE 路径在 `Others/CMakeLists.txt` 顶部：

```cmake
set(JUCE_DIR "/path/to/JUCE" CACHE PATH "Path to a JUCE checkout")
```

改成你本地的 JUCE 路径后，进入 `Others/`：

```sh
cd Others
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Windows 下也可以使用 `Others/build.ps1`，但脚本里有硬编码的 Visual Studio / Ninja 路径，需要按你的环境修改。

### 许可证

GPL-3.0-or-later，见 [LICENSE](LICENSE)。

本项目是独立的开源模块，与 VCV 官方无关。

---

<a id="en"></a>

## English

A **particle-collision generative sequencer**.

Balls move inside a bounded field and collide with each other and with the
walls. Every collision transfers pitch, envelope and effect properties, spawns
waves/fields/sparks, and produces musical events. The result is an endless,
unpredictable but tonal stream of notes and triggers.

The primary target of this repository is the **VCV Rack module**. `Others/`
contains the JUCE standalone prototype, the Python visual prototype and local
build scripts.

### Features

- 12 balls by default (up to 32), moving and colliding inside a closed field
- Three ball types with weighted ratios: **pitch**, **ADSR**, **effect**
- Collisions exchange properties and create waves, fields and sparks
- Pitch range from **0.5 to 8 octaves**, with a `FINE` toggle; without fine mode
  the knob steps in **0.5 octave** increments
- Scale quantizer with:
  - `FREE` mode: continuous random pitch, including microtones
  - Common modes: Major, Minor, Dorian, Phrygian, Lydian, Mixolydian, Locrian,
    Harmonic Minor, Melodic Minor
  - Pentatonic, Blues, Whole Tone, Diminished, Chromatic
  - 5-limit Just Major / Just Minor
  - Microtonal EDO grids: 24, 31, 48 and 72 EDO
- Field shapes: circle, rectangle, ellipse, hexagon
- Field scale, ball size, ball speed, wave speed, wave range
- Speed sharing between nearby balls
- Booster objects that kick balls on contact
- VCV outputs: **V/OCT**, **GATE**, **TRIG**, **FX**

### Install without building (Windows x64)

Download:

`VCV-Module/RandomSynth-2.0.0-win-x64.vcvplugin`

Drag the `.vcvplugin` file into the VCV Rack 2.x window.

Note: the source folder itself cannot be dragged into Rack — only the built
`.vcvplugin` file (or a plugin folder containing `plugin.dll` and
`plugin.json`) can be loaded.

### Repository layout

```
VCV-Module/              VCV Rack module (the open-source target; build this)
  Makefile
  plugin.json
  src/
  RandomSynth-2.0.0-win-x64.vcvplugin   Prebuilt package
Others/                  Everything else (prototypes, build scripts, local outputs)
  CMakeLists.txt
  build.ps1
  src/                   JUCE standalone prototype
  visual_prototype.py    Early Python visual prototype
  *.bat                  Local run scripts
```

Only `VCV-Module/` is needed to build the VCV Rack plugin. `Others/` contains
the in-progress standalone/prototype versions and local build outputs.

### Build the VCV Rack module from source

Requirements:

- [VCV Rack SDK](https://vcvrack.com/manual/Building) 2.x
- `make`
- A C++17 compiler (MSYS2 MinGW-w64 on Windows)

```sh
cd VCV-Module

# Build plugin.dll
make RACK_DIR=/path/to/Rack-SDK

# Package a distributable .vcvplugin
make dist RACK_DIR=/path/to/Rack-SDK
```

On Windows with MSYS2, a typical `RACK_DIR` looks like:

```sh
make RACK_DIR=/d/Rack-SDK/Rack-SDK
```

Install the resulting `.vcvplugin` through Rack's menu, or copy the built plugin
folder into your Rack user plugin directory.

### Build the JUCE standalone (optional)

The JUCE path is configured at the top of `Others/CMakeLists.txt`:

```cmake
set(JUCE_DIR "/path/to/JUCE" CACHE PATH "Path to a JUCE checkout")
```

Change it to your local JUCE checkout, then run from `Others/`:

```sh
cd Others
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

On Windows you can also use `Others/build.ps1`; it contains hard-coded Visual
Studio / Ninja paths, so adjust them for your machine.

### License

GPL-3.0-or-later. See [LICENSE](LICENSE).

This project is an independent open-source module and is not affiliated with
VCV.

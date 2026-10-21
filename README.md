# OV RandomSynth

A **particle-collision generative sequencer** — not a granular synthesizer.

Balls move inside a bounded field and collide with each other and with the
walls. Every collision transfers pitch, envelope and effect properties, spawns
waves/fields/sparks, and produces musical events. The result is an endless,
unpredictable but tonal stream of notes and triggers.

The primary target of this repository is the **VCV Rack module**. A JUCE
standalone prototype is included as well.

## Features

- 12 balls by default (up to 32), moving and colliding inside a closed field
- Three ball types with weighted ratios: **pitch**, **ADSR**, **effect**
- Collisions exchange properties, create waves, fields and sparks
- Pitch range from **0.5 to 8 octaves**, with a `FINE` toggle for continuous
  adjustment; without fine mode the knob steps in **0.5 octave** increments
- Scale quantizer with:
  - `FREE` mode: continuous random pitch, including microtones
  - Common modes: Major, Minor, Dorian, Phrygian, Lydian, Mixolydian,
    Locrian, Harmonic Minor, Melodic Minor
  - Pentatonic, Blues, Whole Tone, Diminished, Chromatic
  - 5-limit Just Major / Just Minor
  - Microtonal EDO grids: 24, 31, 48 and 72 EDO
- Field shapes: circle, rectangle, ellipse, hexagon
- Field scale, ball size, ball speed, wave speed, wave range
- Speed sharing between nearby balls
- Booster objects that kick balls on contact
- VCV outputs: **V/OCT**, **GATE**, **TRIG**, **FX**

## Repository layout

```
VCV-Module/              VCV Rack module (the open-source target; build this)
  Makefile
  plugin.json
  src/
Others/                  Everything else (prototypes, build scripts, local outputs)
  CMakeLists.txt
  build.ps1
  src/                   JUCE standalone prototype
  visual_prototype.py    Early Python visual prototype
  *.bat                  Local run scripts
```

Only `VCV-Module/` is needed to build the VCV Rack plugin. `Others/` contains
the in-progress standalone/prototype versions and local build outputs.

## Build the VCV Rack module

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

Install the resulting `.vcvplugin` through Rack's `Library > Install plugin`
menu, or copy the built plugin folder into your Rack user plugin directory.

## Build the JUCE standalone (optional)

The JUCE path is configured at the top of `Others/CMakeLists.txt`:

```cmake
set(JUCE_DIR "/path/to/JUCE" CACHE PATH "Path to a JUCE checkout")
```

Change it to your local JUCE checkout, then run from `Others/` and use
`build.ps1` on Windows, or build with CMake directly:

```sh
cd Others
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

`Others/build.ps1` also contains hard-coded Visual Studio / Ninja paths;
adjust them for your machine.

## License

GPL-3.0-or-later. See [LICENSE](LICENSE).

This project is an independent open-source module and is not affiliated with
VCV.

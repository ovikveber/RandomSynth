# RandomSynth MIDI (VST3) 交接说明

这是一个**纯 MIDI 发生器**，不产生音频。它和桌面音频版共用同一个
`World` 粒子碰撞引擎，但把碰撞事件转换成 MIDI Note On / Note Off。

- 唯一 VST3：`Others/dist/RandomSynth MIDI.vst3`
- 安装包：`Others/dist/RandomSynth-MIDI-1.0.0-win-x64.exe`
- 工程位置：`Others/`
- 安装包只包含这一个 VST3；MIDI 通道在插件本体的 `MIDI CH` 下拉框选择，
  不需要额外的 VST 实例
- VCV 模块不受影响，`RandomSynth-VCV` 未改动

## VST3 行为

- 插件类型：Instrument（带 MIDI 输出），音频输出为静音
- MIDI 通道：可在 `MIDI CH` 下拉框选择 1–16
- 带粒子碰撞图形界面，实时显示小球、加速器、场、波、连线和墙体属性颜色
- 小球按真实碰撞半径绘制，视觉大小与碰撞箱一致
- 墙体被碰撞后会按音高/ADSR/效果球属性变色，和 VCV 版本一致
- `RUN` / `STOP` 控制粒子引擎，`RESET` 重置碰撞场
- `BALLS` / `加速器` 控制小球数量和加速器数量
- `FINE` 是开关，不再是旋钮
- 图形区域外有独立状态栏，显示 `OUT / GATE / TRIG / FX / PAN`，
  不再遮挡碰撞场
- 只输出 Note On / Note Off；默认不输出 CC
- 切换 `MIDI CH` 时，已经在旧通道上发声的音符仍会在旧通道正确 Note Off
- 音符长度由 `SoundEvent::gateSeconds` 决定
- 同一个音高再次触发时，会先发旧音符的 Note Off，避免切掉新音符
- 宿主调用 `reset()` 时会发送 All Notes Off 并清空内部活动音符

## 统一事件接口（给下游合成器）

`World.h` 中的 `SoundEvent` 就是交接边界。下游合成器可以直接读取这些字段，
把它们映射成自己的振荡器/包络/混响宏，不需要解析 MIDI CC。

```cpp
struct SoundEvent
{
    float note = 60.0f;       // MIDI note number
    float pan = 0.0f;         // -1..1
    float send = 0.0f;        // FX / reverb send CV, 0..1（混响宏接口）
    float velocity = 0.5f;    // 0..1
    float gateSeconds = 0.08f;// 音符长度，可被 ADSR 球改写
    float waveform = 0.0f;    // 波形 CV, 0..1（0 = sine, 1 = square）
    float attack = 0.005f;    // ADSR 接口
    float decay = 0.15f;
    float sustain = 0.6f;
    float release = 0.3f;
};
```

说明：

- `send` 是 FX / 混响发送接口，`waveform` 是波形 CV 接口。
  这两个字段**只保留在代码里**，当前没有面板旋钮；后续下游合成器可以直接读取，
  或在代码里调用 `World::setBaseWaveform()` / `World::setBaseReverb()` 设置基础值。
- `attack/decay/sustain/release` 保留为 ADSR 接口；ADSR 球碰撞时会改变
  `decay/release`，并给其他球分配随机的 Gate 长度。
- 音阶、音域和 FINE 完全按 VCV 逻辑：`RANGE` 内部是 1–16 半八度，
  显示值乘以 0.5；FINE 关闭时吸附到整数（0.5 八度步进），FINE 打开时连续；
  参数读取按实际索引/实际值处理。
- 如果下游需要 MIDI CC，可以把 `MidiPluginProcessor.h` 中的
  `outputMacroCc` 改成 `true`，默认映射为：
  - CC10：PAN
  - CC74：波形 CV
  - CC91：混响 CV
- 宏的具体映射由下游合成器决定，本项目只负责提供 CV 值。

## Gate 规则

- `GATE`：默认音符长度（秒）
- `GATE MIN` / `GATE MAX`：ADSR 球随机给其他球分配 Gate 时的上下限
- ADSR 球与其他球碰撞时，会随机生成一个位于 `[GATE MIN, GATE MAX]` 的长度，
  赋给对方球；该球之后触发的音符都使用这个长度
- 没有被 ADSR 球改写的球使用 `GATE` 默认值

## 参数 ID

| ID | 面板名称 | 范围 / 默认 |
| --- | --- | --- |
| `field` | 场地大小 | 0.5–1.0 / 1.0 |
| `ball` | 小球大小 | 0.3–1.6 / 0.55 |
| `speed` | 小球速度 | 0.1–3.0 / 1.0 |
| `speedShare` | 调速范围 | 0.4–2.5 / 1.0 |
| `strength` | 调速强度 | 0.1–3.0 / 1.0 |
| `waveSpeed` | 水波速度 | 0.2–2.0 / 0.65 |
| `waveRange` | 水波范围 | 0.2–1.5 / 0.5 |
| `ballCount` | 小球数量 | 1–32 / 12 |
| `boosterCount` | 加速器数量 | 0–5 / 2 |
| `shape` | 场地形状 | Circle / Rect / Ellipse / Hex |
| `scale` | 音阶 | FREE、调式、纯律、24/31/48/72 EDO |
| `range` | 音域 | 1–16 半八度（显示 0.5–8 八度）/ 8（4 八度） |
| `fine` | 微调音域 | OFF：0.5 八度步进；ON：连续 |
| `ratioPitch` | 音高比例 | 0–100% / 50% |
| `ratioAdsr` | ADSR 比例 | 0–100% / 25% |
| `ratioEffect` | 效果比例 | 0–100% / 25% |
| `gate` | GATE | 0.01–2.0 s / 0.08 s |
| `gateMin` | GATE MIN | 0.01–2.0 s / 0.03 s |
| `gateMax` | GATE MAX | 0.01–2.0 s / 0.50 s |
| `midiChannel` | MIDI CH | 1–16 / 1 |

## 在下游代码里复用

```cpp
#include "World.h"

World world;
world.setScale ((int) ScaleMode::Major);
world.setPitchRange (4.0f);
world.setDefaultGate (0.08f);
world.setGateRange (0.03f, 0.50f);
world.setBaseWaveform (0.0f);
world.setBaseReverb (0.25f);

const double dt = blockSize / sampleRate;
world.update ((float) dt);

for (const auto& e : world.getEvents())
{
    // e.note, e.velocity, e.gateSeconds,
    // e.pan, e.send, e.waveform,
    // e.attack, e.decay, e.sustain, e.release
}
```

`World` 只依赖 `juce_core` / `juce_graphics`，不依赖 VST3 或音频设备；
下游可以把 `World.h/.cpp` 直接接进自己的合成器工程。

## FL Studio / DAW 路由（类似 FL MIDI Out）

VST3 插件本身不能直接“指定另一个插件”，MIDI 输出由宿主负责路由。
`MIDI CH` 下拉框决定插件把音符发到哪个 MIDI 通道。

FL Studio 示例：

1. 把 `RandomSynth MIDI` 加载到一个通道（例如 A）
2. 打开该插件的包装器设置，在 MIDI 设置里启用 MIDI 输出，
   并设置一个 MIDI Out Port（例如 Port 1）
3. 目标合成器所在通道（例如 B）的 MIDI 输入 Port 设为同一个 Port 1
4. 在 `RandomSynth MIDI` 面板上选择 `MIDI CH`（1–16），
   并让目标合成器接收同一个通道
5. 播放后，A 的 Note On/Off 会通过宿主送到 B

其他 DAW：

- 启用插件的 MIDI 输出
- 把该插件的 MIDI 输出路由到目标合成器轨道
- 用 `MIDI CH` 选择目标通道

通道切换直接在插件本体的 `MIDI CH` 下拉框上完成，不需要加载多个 VST。

## 构建 VST3

```powershell
& "D:\dekstop mainstage\RandomSynth\Others\build.ps1"
```

脚本会：

1. 构建桌面音频版 `RandomSynth.exe`
2. 构建 MIDI 版 Standalone `RandomSynth MIDI.exe`
3. 构建 `RandomSynth MIDI.vst3`
4. 把产物复制到 `Others/dist/`

安装 VST3 时，把 `RandomSynth MIDI.vst3` 复制到
`C:\Program Files\Common Files\VST3`（或你的 VST3 目录），然后在 DAW 里
作为一个 Instrument 加载，并把它的 MIDI 输出路由到目标合成器。

## 制作安装包

```powershell
& "D:\dekstop mainstage\RandomSynth\Others\build-installer.ps1"
```

输出：

`Others/dist/RandomSynth-MIDI-1.0.0-win-x64.exe`

安装包只包含**一个** VST3，安装到
`C:\Program Files\Common Files\VST3\RandomSynth MIDI.vst3`。
MIDI 通道在插件本体的 `MIDI CH` 下拉框选择，不需要安装多个 VST。

## 注意

- 它不是 MIDI Effect，而是带 MIDI 输出的 Instrument；这样大多数 DAW 都能
  直接路由 MIDI。
- 当前没有音频 DSP、没有内置振荡器和混响；波形/混响只是 `SoundEvent` 里的
  代码接口，面板上不提供对应旋钮。
- 粒子图形界面使用处理器发布的 `Snapshot`，不参与音频渲染。
- 原来的 `RandomSynth.exe` 桌面音频版仍然保留，构建脚本会一起构建。

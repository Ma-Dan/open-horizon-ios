# Open Horizon — iOS Port

Open-world arcade flight game based on [open-horizon](https://github.com/undefined-darkness/open-horizon), an open-source re-implementation of *Ace Combat: Assault Horizon*'s engine (C++ / nya-engine). This repository contains the **iOS port** — touch controls, GLES2/ES3 rendering pipeline, on-device asset decoding, and all the platform glue needed to run the game natively on iPhone.

基于 [open-horizon](https://github.com/undefined-darkness/open-horizon)（开源 *Ace Combat: Assault Horizon* 引擎复刻，C++ / nya-engine）移植到 iOS 的工程。包含触摸操控、GLES2/ES3 渲染管线、设备端资源解码，以及所有让游戏在 iPhone 上原生运行所需的平台适配代码。

## Acknowledgements / 致谢

This project is a port of [**undefined-darkness/open-horizon**](https://github.com/undefined-darkness/open-horizon). The original engine, game logic, physics, and asset formats are their work. This repository adds:

本工程移植自 [**undefined-darkness/open-horizon**](https://github.com/undefined-darkness/open-horizon)。原始引擎、游戏逻辑、物理系统及资源格式均为原作者的工作。本仓库新增内容：

- iOS platform layer (touch input, GLES context, scene lifecycle) — iOS 平台层（触摸输入、GLES 上下文、Scene 生命周期）
- ASTC CPU decode pipeline — ASTC 纹理 CPU 解码管线（Apple GLES 驱动硬件上传 ASTC 会渲染乱码）
- Logarithmic depth buffer — 对数深度缓冲（顶点着色器注入，消除 30km 视距下的远处 z-fighting）
- Mipmap double-buffer fix — mipmap 双缓冲修复（`MANUAL_MIPMAP_GENERATION` 的 CPU 降采样）
- Virtual joystick / button overlay for touch devices — 触摸虚拟摇杆/按键
- Self-contained iOS build with vendored dependencies — 自包含 iOS 构建，依赖全部内嵌

## Repository Structure / 目录结构

```
open-horizon-ios/
├── demo/          游戏入口（demo.cpp）、autopilot、触摸控制
├── game/          游戏状态、任务、机体选择
├── gui/           菜单系统
├── phys/          飞行物理
├── renderer/      场景渲染、FHM 网格、天空、地形、机体
├── sound/         音频（OpenAL）
├── util/          平台抽象（platform_ios.cpp）
├── containers/    STL-like 容器
├── deps/
│   ├── nya-engine/   核心引擎（渲染、场景、数学、格式、系统）
│   ├── pugixml-1.4/  XML 解析器
│   ├── miso/         网络（iOS 上 stub 掉）
│   ├── glfw-bin/     GLFW 头文件（仅非 iOS 平台 #include，不链接）
│   ├── lua-5.2.4/    Lua 头文件（静态库预编译在 ios/build/）
│   └── astcenc/      ASTC 解码器（头文件 + 预编译 iOS 静态库 + 源码）
└── ios/
    ├── project.yml          XcodeGen 工程描述
    ├── Info.plist
    ├── ios_render_metal.mm  Metal 后端桥接
    ├── build_lua.sh         重新编译 Lua iOS 静态库
    ├── build_astcenc.sh     重新编译 astcenc iOS 静态库
    ├── build/
    │   ├── liblua52-ios.a         Lua（真机 arm64）
    │   ├── liblua52-sim.a         Lua（模拟器 arm64）
    │   └── openal_include/AL/     OpenAL 头文件（weak-link）
    └── pack/                游戏资源（不在本仓库中，需自行提供）
```

## Disclaimer

This project is an unofficial, non-commercial fan effort. It is not affiliated with, endorsed by, or associated with Bandai Namco Entertainment, Project Aces, or Microsoft. *Ace Combat: Assault Horizon* and all related assets, characters, and trademarks are the property of their respective owners.

This repository contains **source code only**. It does not contain, and will never contain, any game data. To use this software you must supply your own legally obtained copy of the game. No game files, disc images, DLC packages, title updates, or console keys are distributed here, and none may be committed to this repository.

The software is provided as-is, without warranty of any kind. It is experimental: expect bugs, crashes, and visual or audio differences from the original console release. Do not redistribute builds bundled with game data.

## Prerequisites / 环境要求

| Tool / 工具 | Version / 版本 | Purpose / 用途 |
|------|---------|---------|
| macOS | 14+ | 编译宿主机 |
| Xcode | 16+ | iOS SDK、工具链 |
| [XcodeGen](https://github.com/yonaskolb/XcodeGen) | 2.x+ | 从 project.yml 生成 .xcodeproj |
| iPhone | iOS 15+ | 目标设备（已在 iPhone 14 Pro 上测试） |

Game assets (`ios/pack/`) are **not included** in this repository. You must obtain them separately from your own legally owned copy of the game.

游戏资源（`ios/pack/`）**不包含在仓库中**，需自行从合法拥有的游戏副本中提取。

## Building / 编译

### 1. Generate the Xcode project / 生成 Xcode 工程

```bash
cd ios
xcodegen generate
```

This reads `project.yml` and creates `OpenHorizon.xcodeproj`.

读取 `project.yml` 生成 `OpenHorizon.xcodeproj`。

### 2. Build for device / 编译真机版本

```bash
xcodebuild -project OpenHorizon.xcodeproj \
           -scheme OpenHorizon \
           -sdk iphoneos \
           -configuration Debug \
           -destination 'generic/platform=iOS' \
           ARCHS=arm64 \
           CODE_SIGNING_ALLOWED=NO \
           build
```

Build output is in Xcode's DerivedData:

编译产物在 Xcode 的 DerivedData 目录：

```
~/Library/Developer/Xcode/DerivedData/OpenHorizon-*/Build/Products/Debug-iphoneos/OpenHorizon.app
```

### 3. (Optional) Rebuild static libraries /（可选）重新编译静态库

Pre-built `liblua52-ios.a` / `liblua52-sim.a` and `libastcenc-ios-*.a` are included. To rebuild from source:

已包含预编译的 `liblua52-ios.a` / `liblua52-sim.a` 和 `libastcenc-ios-*.a`。如需从源码重新编译：

```bash
# Lua（需要 lua-5.2.4 源码，如未下载请先获取）
cd ios && ./build_lua.sh

# ASTC 编码器（源码已内嵌在 deps/astcenc/src）
cd ios && ./build_astcenc.sh
```

## Running on Device / 真机运行

Code signing is done manually (no Apple Developer account login required). You need a `.mobileprovision` and a signing identity registered to your device.

代码签名为手动完成（无需登录 Apple 开发者账号）。需要一个 `.mobileprovision` 描述文件和一个已注册到设备的签名身份。

### Sign the app / 签名

```bash
APP="~/Library/Developer/Xcode/DerivedData/OpenHorizon-*/Build/Products/Debug-iphoneos/OpenHorizon.app"
IDENTITY="你的签名身份 SHA1"
PROVISION="你的 .mobileprovision 路径"
ENTITLEMENTS="entitlements.plist 路径"

# 修改 Bundle ID 匹配你的描述文件
/usr/libexec/PlistBuddy -c "Set :CFBundleIdentifier com.your.bundleid" "$APP/Info.plist"
cp "$PROVISION" "$APP/embedded.mobileprovision"
rm -f "$APP/__preview.dylib"

# 先签 dylib，再签主二进制，最后签 .app
codesign -f -s "$IDENTITY" --entitlements "$ENTITLEMENTS" "$APP/OpenHorizon.debug.dylib"
codesign -f -s "$IDENTITY" --entitlements "$ENTITLEMENTS" "$APP/OpenHorizon"
codesign -f -s "$IDENTITY" --entitlements "$ENTITLEMENTS" "$APP"
```

### Install and launch / 安装并启动

```bash
xcrun devicectl device install app --device <设备UDID> "$APP"
xcrun devicectl device process launch --device <设备UDID> com.your.bundleid
```

首次启动需在设备上信任开发者证书：**设置 → 通用 → VPN与设备管理**。

## Touch Controls / 触摸操控

| Zone / 区域 | Gesture / 手势 | Action / 操作 |
|------|---------|--------|
| 屏幕左半侧 | 拖动 | 虚拟摇杆（俯仰 / 横滚） |
| 右侧边缘 | 拖动 | 油门滑块 |
| 中右 | 点击 | 机炮（MGUN） |
| 右下 | 点击 | 导弹（MSL） |
| 右上 | 点击 | 切换目标（TGT） |
| 中上 | 点击 | 武器选择（WPN） |
| 左上 | 点击 | 返回（BACK） |

- 推杆**向下** = 俯冲（机头下），推杆**向上** = 爬升
- 菜单中手指**上滑** = 选中项下移（标准触屏列表手感）

## Simulator Testing (Autopilot) / 模拟器测试（自动驾驶）

The build includes an autopilot mode for headless simulator testing:

构建内置 autopilot 模式，用于模拟器无人值守测试：

```bash
# 启动模拟器
xcrun simctl boot "iPhone 17 Pro"

# 安装并以 autopilot 模式启动
SIMCTL_CHILD_OH_FF_AUTO=1 xcrun simctl launch --console <模拟器UDID> <BUNDLE_ID>
```

The autopilot auto-navigates the menu (Free Flight → Paris ms30) and enters the scene. Useful for verifying shader compilation and rendering without manual input.

Autopilot 自动导航菜单（Free Flight → Paris ms30）并进入场景。用于在无人工操作的情况下验证 shader 编译和渲染。

## Key Technical Decisions / 关键技术决策

- **GLES2/ES3 context / GLES2/ES3 上下文**：App requests ES3 first (higher uniform/varying limits, GLSL ES 1.00 compatible), falls back to ES2. — 优先请求 ES3（更高的 uniform/varying 上限，兼容 GLSL ES 1.00 shader），ES2 兜底。
- **ASTC CPU decode / ASTC CPU 解码**：Apple's GLES driver accepts ASTC hardware upload but renders garbage. All ASTC textures are decoded on CPU via `astcenc` (`OH_ENABLE_ASTC_CPU_DECODE`). — Apple GLES 驱动接受 ASTC 硬件上传但渲染乱码。所有 ASTC 纹理通过 `astcenc` 在 CPU 解码。
- **Logarithmic depth buffer / 对数深度缓冲**：Vertex shaders are auto-injected with `gl_Position.z = log2(w+1) * Fcoef` to distribute depth precision geometrically. Eliminates z-fighting on distant buildings at 5 m – 30 km view range with 24-bit depth. — 顶点着色器自动注入对数深度公式，将深度精度按几何级数分布。消除 5m–30km 视距下 24 位深度的远处 z-fighting。
- **FHM mesh float conversion / FHM 网格浮点转换**：Apple GLES rejects half-float vertex attributes; FHM meshes convert to float32 on upload. — Apple GLES 拒绝 half-float 顶点属性；FHM 网格在上传时转为 float32。
- **Mipmap double-buffer / mipmap 双缓冲**：`MANUAL_MIPMAP_GENERATION` CPU downsample uses alternating buffers to prevent in-place corruption of mip levels 2+. — `MANUAL_MIPMAP_GENERATION` 的 CPU 降采样使用交替双缓冲，防止第 2 级 mip 起的 in-place 写覆盖损坏。

## License / 许可证

This project inherits the license of the upstream [open-horizon](https://github.com/undefined-darkness/open-horizon) repository. No game data is distributed; all trademarks and copyrights belong to their respective owners.

本工程继承上游 [open-horizon](https://github.com/undefined-darkness/open-horizon) 仓库的许可证。本仓库不分发任何游戏数据，所有商标及版权归各自所有者所有。

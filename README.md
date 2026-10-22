# DA Flash (Dopamine Flash)

⚡ **DA Flash** 是专为 **Rime 小狼毫输入法 (Weasel)** 打造的高性能、全场景、多显示器支持的硬件加速输入视觉特效引擎。

取名 **DA** 源于 **多巴胺 (Dopamine)**，旨在为每次指尖敲击与汉字上屏带来即时、灵动、畅快的高帧率视觉反馈。

---

## 🌟 核心特性

- **全局光标 100% 像素级精准定位**：
  - 深度挂钩小狼毫底层管道通信 (`WEASEL_IPC_UPDATE_INPUT_POS`)，直接截获系统高精度 Text Services Framework (TSF) 候选位置。
  - 在 **Chrome/Edge 浏览器、微信、PowerShell/Terminal、Notepad++、VS Code、Word** 等全场景无死角精准跟随。
- **真实汉字反重力升腾**：
  - 上屏汉字从光标处破空而起，具备粒子物理膨胀、微光阴影、角速度旋转与正弦风阻漂移效果。
- **全多屏全域无缝支持**：
  - 完美兼容负坐标副屏（如顶部副屏）与混合 DPI 缩放，锁定 96 DPI 物理坐标系，无拉伸、无虚拟化偏移。
- **纯硬件加速与极低资源消耗**：
  - Direct2D + DirectWrite 60~120 FPS 硬件管线渲染，黑色色键穿透，鼠标绝对穿透 (`WS_EX_TRANSPARENT`)。
  - 智能节能机制：无活跃粒子时自动休眠，日常待机 **0% CPU 占用**。
- **连击暴击与屏幕微震 (Combo & Shake)**：
  - 实时计算打字速率 (WPM)，速率超过阈值时触发暴击模式，双倍粒子爆破并伴随屏幕微震反馈。
- **个性化系统托盘**：
  - 任务栏常驻青紫色霓虹光环图标，支持右键切换 4 套粒子主题（赛博朋克 / 璀璨流金 / 极地冷光 / 彩虹炫彩）与震动开关。

---

## 🏗️ 架构组成

```text
DAFlash/
├── include/
│   ├── Common.h          # 基础数学工具、颜色混合、动态日志
│   ├── Config.h          # 引擎配置结构与主题定义
│   └── IpcProtocol.h     # 进程间通信协议与共享内存定义
├── src/
│   ├── app/              # 主引擎进程 (DAFlash.exe)
│   │   ├── main.cpp            # 主渲染循环、系统托盘、自动补注
│   │   ├── OverlayWindow.cpp   # 全屏透明穿透画布
│   │   ├── D2DRenderer.cpp     # Direct2D 硬件渲染管线
│   │   ├── ParticleManager.cpp # 粒子物理系统与发射器
│   │   ├── InputMonitor.cpp    # 键盘监听、IPC 轮询与多重兜底光标
│   │   └── Injector.cpp        # WeaselServer 自动注入器
│   └── hook/             # 小狼毫注入模块 (DAFlashHook.dll)
│       ├── HookMain.cpp        # DLL 入口与生命周期
│       └── WeaselHook.cpp      # MinHook 截获 Rime 选词与管道输入坐标
├── third_party/
│   └── minhook/          # 极简轻量 x64 API Hook 库
├── CMakeLists.txt        # CMake 工程配置
├── config.yaml           # 外部配置文件
├── restart.bat           # 一键重启小狼毫与特效引擎
└── run.bat               # 日常静默启动脚本
```

---

## 🛠️ 编译指南

### 依赖环境
- **编译器**：MinGW64 GCC 15+ (x86_64-w64-mingw32)
- **构建工具**：CMake 3.15+ 与 Ninja

### 编译步骤
```powershell
cd D:\DEV\DAFlash
mkdir build
cd build
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ..
ninja
```
编译产物：
- `DAFlash.exe` (独立主程序，已静态链接 libgcc 与 libstdc++)
- `DAFlashHook.dll` (注入模块)

---

## 🚀 启动与使用

1. 双击运行根目录下的 `run.bat` 或 `restart.bat`。
2. 启动后，主屏幕中央将浮现 `✦ DA Flash (Dopamine) ✦` 欢迎动画。
3. 任务栏右下角托盘区（或 `^` 溢出区）会出现 **DA Flash** 图标，右键可选择特效主题。
4. 在任意应用中输入中文，即刻体验流畅指尖多巴胺反馈！

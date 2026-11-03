# 钢铁的咆哮 3（Kurogane no Houkou 3）中文化补丁

这是面向中文用户的钢铁的咆哮 3（Kurogane no Houkou 3，WSC3，工程目录名为 `KH3`）运行时汉化补丁基准工程。

## 项目说明

本仓库保存 Kurogane no Houkou 3（WSC3）运行时汉化补丁的基准源码。

`dist/version.dll` 是现有运行版产物；`dist/version-debug.dll` 是带 Debug UI、来源
追踪开关和 Win11 排版调参能力的完整版调试产物，需配合 `dist/debug.ini` 使用。

补丁保留 SafeDiscLoader2 的 `version.dll` 代理和兼容逻辑，并加入：

- CP932（日文 Shift-JIS）原始字节识别；
- 中文翻译表查找；
- `DrawTextA` / `DrawTextW` 文字拦截；
- 中文宽字符绘制；
- 254、260、328、362 宽度的段落重排；
- 旧版段落重排回退逻辑；
- WSC3 UI Debug 调试窗口和翻译保存功能；
- 游戏资源来源追踪与来源缓存清理；
- XP 参考字体度量和 Win11 排版调参窗口；
- 模板占位符的可选二次比对；

完整复现说明请阅读：[运行时汉化-完整复现与维护说明.md](运行时汉化-完整复现与维护说明.md)。

## 快速使用

将构建后的 `version.dll` 放到游戏 EXE 所在目录。当前测试目录为：

    E:\\KH3

翻译表放在：

    E:\\KH3\\chinese\\translations.json

仓库提供的翻译表位于 `game-files\\chinese\\translations.json`。下载后应将它复制为：

    <游戏根目录>\\chinese\\translations.json

本次发布的修改版图片资源 `cg.bb3` 必须复制到：

    <游戏根目录>\\data\\stobj\\cg.bb3

仓库文件位于 `game-files\\data\\stobj\\cg.bb3`。完整目录结构见
[游戏目录部署说明.md](游戏目录部署说明.md)。

游戏运行版默认启用汉化、CP932 兼容和段落修正，无需 `version.json`；翻译内容仍从 `chinese\translations.json` 读取。此版本不包含翻译调试窗口或日志输出。

补丁默认限制 `EndScene` 调用频率为 60 次/秒。可在游戏目录的 `debug.ini` 中
设置 `[Common]` 下的 `FrameRate=30` 或 `FrameRate=60`；`FrameRate=0` 关闭限速。
支持 15～240，未设置或数值超出范围时默认 60。该数值控制的是 `EndScene`
调用频率，实际画面帧率和游戏速度仍需在游戏中验证。
完整版 Debug DLL 也从同一个 `debug.ini` 读取调试开关：

    WSC3DebugUI=1
    WSC3SourceTracking=0
    WSC3LayoutTuner=0

这些 `[Common]` 项优先于 `version.json` 中对应的 UI 设置。将
`WSC3DebugUI=0` 后，翻译、CP932 兼容、普通段落修补和帧率限制仍然运行，
但不启动 Debug UI，也不安装来源追踪、调试记录和排版调参功能；这样不会
因为调试窗口或来源扫描增加游戏运行负担。仓库中的 `debug.ini` 是可直接复制
到游戏目录的模板。

现代中文 Windows 上，进程内 CP932 兼容层在完整版 Debug DLL 中默认启用。
实现细节和维护步骤见 [帧率限制-实现与维护说明.md](帧率限制-实现与维护说明.md)。

现代中文 Windows 上，进程内 CP932 兼容层在游戏运行版中默认启用。

SafeDisc 3 及以上版本通常会自动加载同目录下的 `version.dll`。部分 SafeDisc 2 游戏需要使用 `VersionInjector.exe` 注入；可以双击运行，也可以将游戏 EXE 拖到该程序上。

## 运行逻辑

补丁不修改游戏 EXE，也不修改原始游戏文本文件：

    游戏文字调用
        ↓
    DrawTextA / DrawTextW Hook
        ↓
    用原始 CP932 字节识别文本
        ↓
    查找 chinese\\translations.json
        ↓
    有翻译：用中文宽字符绘制
    无翻译：显示原文
        ↓
    对完整段落进行重排并抑制重复日文行

原始文本身份仍然是 CP932。CP936 和宽字符只负责显示中文，不能代替原始文本身份，否则会导致翻译键无法稳定匹配。

## 系统区域兼容修复

游戏原版按日文非 Unicode 程序环境运行，依赖 CP932。中文系统通常使用 CP936，可能导致部分资源或舰船数据加载失败，最终在游戏内部出现空表项并闪退。

`uiForceCp932=true` 只对游戏进程模拟 CP932：让 `GetACP()`、`GetOEMCP()` 返回 932，并将 `CP_ACP` 转换按 CP932 处理。它不修改游戏 EXE、原始资源或翻译表，也不改变中文显示链路。

该兼容层已经在中文系统区域下验证可修复完成舰购买闪退；如果改用日文系统区域，可关闭该选项进行对照测试。

## 翻译表

翻译表路径：

    E:\\KH3\\chinese\\translations.json

普通静态文本示例：

    {
      "hash": "DE23B78B",
      "text": "战舰",
      "source": ""
    }

只有确实存在变量时才使用 `{0}`、`{1}` 等模板占位符。不能把所有数字自动视为变量，因为玩家输入的数字也可能是名称。

普通记录的 `source` 保持为空，只有模板记录保存 `source`，以避免翻译表无意义地膨胀。

模板记录可以额外设置 `translate_captures: true`。启用后，运行时先按完整 `source`
模板匹配，再对捕获到的 `{0}`、`{1}` 等具体值按普通 CP932 哈希查找翻译；不启用时，
占位符原样写回。该字段只影响模板记录，不改变普通翻译的查找逻辑。

## 段落补丁

当前支持的历史文字框宽度为：

    254
    260
    328
    362

游戏通常先以宽度 0 测量完整段落，再以窄框逐行绘制。补丁缓存完整段落，优先进行历史调用回放；严格路径不匹配时，继续使用旧版通用段落重排逻辑。

因此，即使没有任何中文翻译，日文原文也应经过同样的段落修正。

## XP 与 Win11 排版调试

XP 日文环境用于采集原始游戏的参考数据，不作为最终补丁环境。监测记录包括
`extentWidth`、`rectWidth`、`lfWidth`、`avgWidth`、`maxWidth` 和字体高度，
以确认原游戏实际使用的测量区域与字体度量。

Win11 使用完整版 Debug DLL 的 `WSC3LayoutTuner=1` 开启排版调参窗口，按字体
高度分别调整 `lfWidth`、`avgWidth`、`maxWidth`。当前人工确认的 XP 参考值为：

    12px: lfWidth=5,  avgWidth=5,  maxWidth=11
    16px: lfWidth=0,  avgWidth=7,  maxWidth=32
    28px: lfWidth=15, avgWidth=15, maxWidth=34
    38px: lfWidth=0,  avgWidth=20, maxWidth=39（关闭）
    76px: lfWidth=0,  avgWidth=39, maxWidth=77（关闭）

`lfWidth` 只影响之后创建的字体，点击 Apply 后需要重新进入对应游戏界面。
调参窗口只用于寻找 Win11 对齐 XP 的参数，不负责翻译表编辑；翻译与段落修补
仍由完整版 Debug UI 完成。

## 从源码构建

要求：Windows、Visual Studio 2022 或 Build Tools、Win32/x86 C++ 工具链。

在 `SafeDiscLoader2` 目录执行：

    .\\build-release.ps1

脚本会调用 Win32/v143 MSBuild，完成重建并输出 DLL 的 SHA-256。也可以按脚本中的命令手动构建。

输出文件：

    Release\\version.dll

运行全部源码契约测试：

    Get-ChildItem .\\tests\\*.ps1 | ForEach-Object { & $_.FullName }

Release 构建定义 `WSC3_RELEASE_NO_LOGGING` 和 `WSC3_GAME_RELEASE`，移除日志与翻译调试 UI 的运行入口。构建和部署前请保存原 DLL。
不要同时加载多个版本的 `version.dll` 或 `kh3_gbk.dll`。

## 重要文件

| 文件 | 用途 |
|---|---|
| `src/UiDiag.cpp` | 编码转换、翻译表、文字 Hook、段落重排 |
| `src/version.cpp` | version.dll 代理、SafeDisc 初始化、配置读取和 Hook 安装 |
| `tests/` | 段落补丁和 Hook ABI 契约测试 |
| `运行时汉化-完整复现与维护说明.md` | 完整复现、维护和故障排查 |
| `基准构建记录.md` | 基准 DLL、哈希和部署记录 |
| `更新说明-CP932区域兼容修复.md` | 本次中文系统区域闪退修复说明 |
| `version.capture.json` | 已关闭日志的可部署配置示例 |
| `debug.ini` | 帧率、Debug UI、来源追踪和排版调参开关模板 |
| `game-files\\chinese\\translations.json` | 可直接部署到游戏目录的翻译表 |
| `游戏目录部署说明.md` | 翻译表和修改版 `cg.bb3` 的目标路径说明 |
| `build-release.ps1` | 一键重建 Release DLL 并输出 SHA-256 |
| `build-full-debug.ps1` | 一键重建完整版 Debug DLL 并输出 SHA-256 |
| `更新说明-2026-10-25-干净版与调试UI.md` | 本版本改动、构建产物和回退信息 |
| `更新说明-2026-11-03-debug.ini与XP-Win11排版.md` | 本次 debug.ini、XP 基线和 Win11 排版调参说明 |
| `更新说明-2026-11-01-帧率限制与日常版.md` | 历史版本更新记录 |

## 安全和许可证

本项目面向拥有正版游戏的用户，用于个人兼容性、维护和合理使用。仓库不包含游戏本体、游戏资源、运行日志、API 密钥、内存镜像或旧版 DLL 备份。`dist/version.dll` 是由本仓库源码构建的当前发布产物，不是游戏文件。

某些杀毒软件可能会把 DLL Hook 或代码注入误报为风险行为，请先确认源码和构建过程，再根据自己的安全策略决定是否运行。

本仓库保留原 SafeDiscLoader2 的 GPL-3.0 许可证。

<details>
<summary>原 SafeDiscLoader2 英文资料（展开查看）</summary>

## Kurogane no Houkou 3 Chinese patch baseline

This repository contains the WSC3/KH3 runtime translation patch. It preserves the SafeDiscLoader2 `version.dll` proxy and adds runtime CP932 text identity, Chinese translation lookup, `DrawTextA/W` interception, and the historical paragraph reflow compatibility path.

The complete reproduction and maintenance record is in [运行时汉化-完整复现与维护说明.md](运行时汉化-完整复现与维护说明.md).

The repository intentionally excludes game files, runtime logs, API keys, memory captures, old DLL backups, and Visual Studio build output. Build the DLL from source using the Win32 instructions in the reproduction document.

## Download
[Releases](https://github.com/nckstwrt/SafeDiscLoader2/releases)

## Usage (For SafeDisc version 3 or higher)
SafeDiscLoader2 is primarily just "version.dll". For any game using SafeDisc 3 or higher you can just place version.dll in the same directory as the game's main executable and the game can then be run normally. It will then automatically use version.dll without any further changes required to bypass SafeDisc.

## Usage (For SafeDisc version 2)
Old games do not automatically load version.dll - so it needs to be injected first using VersionInjector.exe

### Just Double-Click VersionInjector.exe
From here a window will appear allowing you to select your game's SafeDisc .exe file
### Drag your SafeDisc exe onto VersionInjector.exe
That is equivalent to running VersionInjector.exe with the dragged SafeDisc .exe file as the first argument. VersionInjector will attempt to pass on any additional arguments provided. i.e. `Sims2.exe -w` will load The Sims 2 in windowed mode
### Create a SafeDiscLoader.ini file
If a SafeDiscLoader.ini file is found in the same directory as SafeDiscLoader.exe it will use its contents as if it were commandline arguments passed. i.e. Create a text file called SafeDiscLoader.ini with just `Sims2.exe -w` to load The Sims 2
### Using it with non-SafeDisc executables
You can also use VersionInjector.exe to make SafeDisc utilities work on modern Windows. e.g. Safedisc2Cleaner which can unwrap Safedisc executables below version 2.7.

## Configuration
SafeDiscLoader2 can be configured to bypass additional CD checks and log lots of debug information.  
Find out more here: [Example Config Files](https://github.com/nckstwrt/SafeDiscLoader2/tree/main/Example%20Config%20Files)

## Credits
*  Reloaded for their Universal SafeDisc Loader which had great logging to help me reverse engineer what it was doing
*  SafeDiscCleaner (bOOls eYe, r!sc, etc) nice clean assembly to read for how to patch SafeDisc v2.0 - v2.6
*  [RibShark](https://twitter.com/RibShark) for his secdrv.sys [SafeDiscShim emulation code](https://github.com/RibShark/SafeDiscShim)
*  [version-proxy](https://github.com/BitCrackers/version-proxy) from BitCrackers for proxying the Windows version.dll code and approach
*  [MinHooks](https://github.com/TsudaKageyu/minhook) for API Hooking
*  SafeDisc 2.7 + 2.8 Loader code written by me

## Background
[My initial version](https://github.com/nckstwrt/SafeDiscLoader) used Reloaded's SDLoader.dll to bypass SafeDisc. Sadly this was picked up by many virus checkers as being suspicious purely because reloaded's code is so obfuscated. So this version is a fully reverse engineered version of SDLoader.dll and SafeDiscCleaner to support all versions of SafeDisc 2.0 and higher.

## Tested Games
* Call of Duty (Original): 3.10.20
* Call of Duty (Patched - 1.4): 3.15.11 
* Call of Duty - United Offensive: 3.20.22
* Call of Duty - Modern Warfare: 4.81.00
* The Sims 2: 3.20.20 (has two safedisc cd checks!)
* Call of Duty - World at War: 4.90.10
* Need for Speed: Underground: 2.90.40 (Needs VersionInjector)
* Football Manager 2005 (Original - 5.0): 3.20.22
* Football Manager 2005 (Patched - 5.0.5): 4.00.02
* Harry Potter and the Prisoner of Azkaban: 3.20.22
* Harry Potter and the Chamber of Secrets: 2.80.10 (Needs VersionInjector)
* The Battle for Middle-earth II: 4.60.00 ? Normal version string missing. Need to use VersionInjector on the Launcher.
* The Battle for Middle-earth: 4.00.01 ? Normal version string missing. Need to use VersionInjector on the Launcher. (On Win 11 needs special Options.ini to work)
* Football Manager 2006 (6.0.2): 4.60.00
* Football Manager 2006 (6.2.3): 4.60.00
* Football Manager 2007 (7.0.2): 4.60.00
* Need For Speed Underground 2: 3.05.10 Normal version string missing. Still needs CD 2 (but not a Safedisc check) - looks for bin.dat from the disc
* Command & Conquer: Renegade: 2.51.21 (Needs VersionInjector - still crashes due to getting system info. Issue with game on modern systems)
* Operation Flashpoint: Cold War Crisis: 2.40.10 (original) 1.46 (2.50.20) - Even it's patchers need safedisc (and therefore VersionInjector) to work.
* Doom 3: 3.20.22
* Far Cry: 3.15.10
* Max Payne (1.0): 2.30.33
* Settlers IV: 2.10.30
* Delta Force - Land Warrior: 2.05.30 (cd check for volume name and CDFS filesystem)
* Eurofighter Typhoon: 2.10.30
* Battlefield 1942: 2.60.52 - checks for the existence of bfdist.vlu on the CDROM
* Kohan: 2.60.52 - Checks the CDROM's Volumename is KOHAN_IS
* Battlefield 2: 4.00.01

 ## Notices
> [!IMPORTANT]
> This is not intended for Piracy but for users to exercise their Fair Use rights for the games they actually own and can no longer play due to draconian and unsupported copy protection

> [!CAUTION]
> Antivirus software that blocks code injection could block this dll or loader, so make sure to greenlist the included exe and dll or disable your antivirus software if required.

</details>

# Valkyria Chronicles 3 中文化补丁

这是面向中文用户的 Valkyria Chronicles 3（WSC3，工程目录名为 `KH3`）运行时汉化补丁基准工程。

## 项目说明

本仓库保存 Valkyria Chronicles 3（WSC3）方案一运行时汉化补丁的基准源码。

补丁保留 SafeDiscLoader2 的 `version.dll` 代理和兼容逻辑，并加入：

- CP932（日文 Shift-JIS）原始字节识别；
- 中文翻译表查找；
- `DrawTextA` / `DrawTextW` 文字拦截；
- 中文宽字符绘制；
- 254、260、328 宽度的段落重排；
- 旧版段落重排回退逻辑；
- WSC3 UI Debug 调试窗口和翻译保存功能。

完整复现说明请阅读：[方案一-完整复现与维护说明.md](方案一-完整复现与维护说明.md)。

## 快速使用

将构建后的 `version.dll` 放到游戏 EXE 所在目录。当前测试目录为：

    E:\\KH3

翻译表放在：

    E:\\KH3\\chinese\\translations.json

`version.json` 也放在游戏目录中，用于控制日志、汉化补丁和 Debug UI。

SafeDisc 3 及以上版本通常会自动加载同目录下的 `version.dll`。部分 SafeDisc 2 游戏需要使用 `VersionInjector.exe` 注入；可以双击运行，也可以将游戏 EXE 拖到该程序上。

## 方案一运行逻辑

方案一不修改游戏 EXE，也不修改原始游戏文本文件：

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

## 段落补丁

当前支持的历史文字框宽度为：

    254
    260
    328

游戏通常先以宽度 0 测量完整段落，再以窄框逐行绘制。补丁缓存完整段落，优先进行历史调用回放；严格路径不匹配时，继续使用旧版通用段落重排逻辑。

因此，即使没有任何中文翻译，日文原文也应经过同样的段落修正。

## 从源码构建

要求：Windows、Visual Studio 2022 或 Build Tools、Win32/x86 C++ 工具链。

在 `SafeDiscLoader2` 目录执行：

    MSBuild.exe .\\version-proxy.vcxproj /t:Rebuild /p:Configuration=Release /p:Platform=Win32 /p:PlatformToolset=v143 /p:WholeProgramOptimization=false /m:1 /v:minimal

输出文件：

    Release\\version.dll

构建和部署前请保存原 DLL。不要同时加载多个版本的 `version.dll` 或 `kh3_gbk.dll`。

## 重要文件

| 文件 | 用途 |
|---|---|
| `src/UiDiag.cpp` | 编码转换、翻译表、Debug UI、文字 Hook、段落重排 |
| `src/version.cpp` | version.dll 代理、SafeDisc 初始化、配置读取和 Hook 安装 |
| `tests/` | 段落补丁和 Hook ABI 契约测试 |
| `方案一-完整复现与维护说明.md` | 完整复现、维护和故障排查 |
| `基准构建记录.md` | 基准 DLL、哈希和部署记录 |

## 安全和许可证

本项目面向拥有正版游戏的用户，用于个人兼容性、维护和合理使用。仓库不包含游戏本体、游戏资源、运行日志、API 密钥、内存镜像或旧版 DLL 备份。

某些杀毒软件可能会把 DLL Hook 或代码注入误报为风险行为，请先确认源码和构建过程，再根据自己的安全策略决定是否运行。

本仓库保留原 SafeDiscLoader2 的 GPL-3.0 许可证。

<details>
<summary>原 SafeDiscLoader2 英文资料（展开查看）</summary>

## Valkyria Chronicles 3 Chinese patch baseline

This repository contains the current scheme-one baseline for the WSC3/KH3 runtime translation patch. It preserves the SafeDiscLoader2 `version.dll` proxy and adds runtime CP932 text identity, Chinese translation lookup, `DrawTextA/W` interception, and the historical paragraph reflow compatibility path.

The complete reproduction and maintenance record is in [方案一-完整复现与维护说明.md](方案一-完整复现与维护说明.md). It is the authoritative description of the current scheme-one behavior. The two `方案二` documents are retained as historical design notes and do not describe the final runtime path.

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

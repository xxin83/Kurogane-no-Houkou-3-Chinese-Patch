# WSC3 Win11 排版调参测试版

这是 Win11 专用的排版实验 DLL，只用于寻找 XP 参考排版对应的参数。

## 使用

1. 将 `version.dll` 复制到游戏目录，与 `wsc3.exe` 放在同一目录。该 DLL 已经在编译时固定为调参模式，不依赖 `version.json` 才能启动调参窗口；目录中的 `version.json` 仅作为参数说明和备用配置。
2. 启动游戏后会出现 `WSC3 排版调参测试` 窗口。
3. 修改 `lfWidth`、`avgWidth`、`maxWidth`，点击 `Apply`。
4. 根据游戏画面手动记录有效参数。
5. `lfWidth` 只对之后创建的字体生效；修改后需要重新进入对应界面，或让游戏重新创建字体。
6. `Reset` 恢复当前人工确认的默认调参值，`Close` 只关闭调参窗口。

调参窗口只在主游戏进程 `wsc3.exe` 中启动；SafeDisc 辅助进程不会创建调参窗口。

## 当前默认值

```text
12/13px: lfWidth=5,  avgWidth=5,  maxWidth=11
16px:    lfWidth=0,  avgWidth=7,  maxWidth=32
28px:    lfWidth=15, avgWidth=15, maxWidth=34
38px:    lfWidth=0,  avgWidth=20, maxWidth=39 (disabled)
76px:    lfWidth=0,  avgWidth=39, maxWidth=77 (disabled)
```

默认强制 CP932；默认不启用“段落测量修正”。

此版本不启用翻译、翻译 Debug UI、来源追踪、舰船数据抓取、原有段落重排或日志记录。它是独立的 `LayoutTuner` 构建，不是完整版 Debug DLL，也不要把它作为日常游戏汉化 DLL 使用。

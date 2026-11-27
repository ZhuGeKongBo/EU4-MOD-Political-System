# 政治体系框架 · Political Framework for EU4

**Public Beta 0.1** · 框架 Runtime 0.9.2 · 编辑器 Editor 1.0.11 · Windows x64 · Steam EU4 1.37.5 · Anvil 1.24.0

[中文](#中文) · [English](#english)

> [!WARNING]
> **注意！Beta v0.1 仅供 MOD 开发参考。** 本版本还没有在游戏中加入完整的玩法机制与帮助说明。安装后可以进游戏打开面板查看，但没有必要开局正式游玩（测试除外）。
>
> - 教廷、原住民联盟两种预设，以及从零自建政治体系的可靠性**尚未测试**。
> - 室町幕府与蒙古帝国两套示例中，许多文本与接口**仍有问题**。
>
> **NOTICE: Beta v0.1 is a reference build for mod developers.** It does not yet add full gameplay mechanics or in-game help. You can open the panels in game, but there is no need to start a real campaign (except for testing).
>
> - The Papacy and Native Federation presets, and systems built from scratch, have **not been tested** for reliability.
> - The Ashikaga Shogunate and Mongol Empire examples still have **many problems** with their texts and interfaces.

---

## 中文

### 这是什么

本项目旨在为《欧陆风云 4》增加更多类似神圣罗马帝国、中华帝国、教廷、原住民联盟的跨 TAG 组织玩法，在本项目中统称为“政治体系”。

它是**面向 MOD 作者的框架**。框架以 [Anvil](https://github.com/happy-dwarf-team/anvil) 插件的形式在运行时扩展游戏，MOD 作者只需编写脚本、GUI 和本地化文件，就能定义自己的政治体系，不必重新编译 DLL。项目主要借助 OpenAI Codex（AI 编程助手）开发。

### 0.9.2 有什么新东西

- **原版脚本接入：** 改革、决议、诏令、事变的条件和效果，以及入口、成员筛选、职位资格等条件块，现在交给游戏自己的解析器读取。可以直接写原版 trigger / effect、你自己的 scripted trigger / effect，也可以切到 `capital_scope` 等作用域。
- **持续修正：** 改革和诏令可以给受益方、领袖、成员或体系领土附加修正。
- **通知：** 新增 `political_send_message`，可以向指定国家发送带按钮的弹窗，或只记入消息日志。海岸宪章示例里有现成按钮。
- **示例补全：** 两个可编辑示例都带上了外层 `.mod` 文件，复制即可在启动器里启用。

### 当前状态

| 内容 | 状态 |
| --- | --- |
| 室町幕府、蒙古帝国（Workshop 示例） | 上一版 0.9.1 曾在战役中确认能与原版神罗、天朝同时打开；本版精简内容通过预检，**尚未在战役中实测**；文本与接口仍有许多问题 |
| 原版 trigger / effect 接入 | 预检能检查文件结构并给出行号；实际运行结果**需进游戏验证** |
| 通知弹窗 `political_send_message` | 离线测试通过，**尚未在战役中实测** |
| 神罗式、天朝式预设 | 随 Workshop 提供，被上面两套示例使用 |
| 教廷式、原住民联盟式预设 | 只在编辑器模板中提供，**未测试** |
| 从零自建体系（`examples/FromScratch`） | 通过预检，**可靠性未测试** |
| 编辑器 1.0.11 | Windows 回归测试通过（新建、编辑、存读、导出） |
| 框架插件 0.9.2 | 15 项离线测试通过；对 Steam EU4 1.37.5 核验了 114 个符号 |

详细记录见 [验证记录](docs/Validation.md)。

### 需要下载什么

| 下载项 | 从哪里获取 | 用途 | 是否必需 |
| --- | --- | --- | --- |
| Anvil 1.24.0（Windows x64） | [上游发布页](https://github.com/happy-dwarf-team/anvil/releases/tag/v1.24.0) | 插件加载器及标准插件 `anvil-game` | 必需 |
| `Political-Framework-Runtime-Beta-0.1.zip` | 本仓库 [Releases](https://github.com/<GitHub用户名>/<仓库名>/releases) | 框架插件 0.9.2 | 必需 |
| Political Framework - Beta 0.1 | Steam 创意工坊：`<Workshop物品链接>` | 幕府、蒙古示例内容 | 想看示例时必需 |
| `EU4-Political-Editor-Beta-0.1.zip` | 本仓库 [Releases](https://github.com/<GitHub用户名>/<仓库名>/releases) | 可视化编辑器 1.0.11（需 .NET Framework 4.8） | 只在做 MOD 时需要 |

### 快速安装

1. 退出游戏。按上游说明把 Anvil 1.24.0 安装到 `eu4.exe` 所在文件夹。
2. 解压 Runtime 压缩包，把 `anvil/plugins/great-khan-native/` 整个文件夹复制到游戏目录的 `anvil/plugins/` 下。
3. 订阅 Workshop 物品，在启动器播放集中启用。
4. 开 1444 年普通单人新局，选足利（ASK）或蒙古（KHA），在小地图旁找到图标。

> [!IMPORTANT]
> - **汉化 DLL 兼容性。** 本项目通过 Anvil 与 Mei 汉化插件配合使用。游戏根目录的 `version.dll` 由 Anvil 提供，Mei 放在 `anvil/plugins/mei/`，负责读取 UTF-8 本地化并解码旧 EU4DLL 格式的汉化文本。框架使用游戏加载后的文本。已实测 Mei 0.9.0 与框架 0.9.1 同时加载，0.9.2 尚待复验。如果旧汉化补丁也要求把自己的 `version.dll` 放进游戏根目录，它会与 Anvil 的同名文件互相覆盖。更换前请先备份。
> - 本 MOD 替换了 `interface/messagetypes.txt`。其他替换这个文件的 MOD 会互相覆盖，需要手动合并消息注册项。

每一步的检查方法和出错处理见 **[安装教程](docs/Install-Guide.zh-CN.md)**。

### 给 MOD 作者：从哪里开始

推荐顺序：

1. 按 **[Modder 教程](docs/Modder-Tutorial.zh-CN.md)** 第 3 节，用“继承预设”的方式做出第一个自己的体系（大约 5 个文件，不碰 GUI）。
2. 需要某个功能时，查教程第 4 节的“常见改法”，或 [`examples/DefaultType/Default-Fields.zh-CN.txt`](examples/DefaultType/Default-Fields.zh-CN.txt)（带中文逐行注释的全功能字段参考），复制需要的段落即可。
3. 想在原版事件里读写体系，或者想知道每个条件块里 `ROOT`、`FROM` 指谁，看 [原版脚本接入说明](docs/Native-Scripting.zh-CN.md)。
4. 想要自己的窗口时，再看教程第 5 节“从零自建”（Beta 0.1 中可靠性未测试）。
5. 需要拖拽摆放控件、预览面板、检查错误时，用编辑器，见 [编辑器手册](docs/Editor-Manual.zh-CN.md)。

### 仓库里有什么

| 目录 | 内容 |
| --- | --- |
| `docs/` | 安装教程、Modder 教程、原版脚本接入说明、编辑器手册、验证记录、发行说明，以及安装手册 PDF |
| `examples/` | 字段参考 `DefaultType`、继承预设示例 `ReuseTemplate/coastal_charter`、从零自建示例 `FromScratch/harbor_moot` |
| `editor/` | 编辑器源码与内置的预设模板（含教廷、原住民联盟） |
| `anvil/` | 框架插件源码、符号声明、导出的脚本接口清单 `api_manifest.json` |
| `licenses/` | 第三方许可证（Anvil） |

从源码构建见 [BUILD.md](BUILD.md)。

### 维护状态

作者在开发阶段结束后进入怠惰期，**不保证后续更新与答疑**。诸位可以在本版本上继续修改和优化，遵守 MIT 许可证、保留原版权声明即可。欢迎 fork、提交 Issue 和 Pull Request，但回复可能很慢，详见 [CONTRIBUTING.md](CONTRIBUTING.md)。如果你维护着持续更新的分支，欢迎告知，会在这里附上链接。

### 许可证与声明

- 本项目自研的编辑器与框架代码以 [MIT 许可证](LICENSE) 发布，版权 © 2026 Political Framework contributors。再分发代码或由其构建的程序时，请保留版权声明与许可证文本。
- Anvil、其 SDK 与 `anvil-game` 由 Happy Dwarf Team 开发，适用其自己的 [MIT 许可证](licenses/Anvil-MIT.txt)。
- 本项目为非官方作品，与 Paradox Interactive 无关，也未获其认可，需要正版游戏。Workshop 内容中的 `interface/messagetypes.txt` 是 EU4 1.37.5 原版消息设置表的完整副本，只在末尾追加了 5 项框架注册，原表版权归 Paradox；除此之外不含游戏原版文件，其余游戏素材都通过引用本机已安装的游戏资源使用。
- 本项目会在运行时修改游戏进程，使用风险自负，作者不提供任何担保。

详见 [NOTICE.md](NOTICE.md)。

**祝 EU4 健康，常胜！**

---

## English

### What is this?

This project aims to bring more cross-tag organisations like the Holy Roman Empire, the Empire of China, the Papacy and the Native Federations to Europa Universalis IV. In this project they are called "political systems".

It is a **framework for mod authors**. It extends the game at runtime as an [Anvil](https://github.com/happy-dwarf-team/anvil) plugin, so mod authors can define their own political systems with script, GUI and localisation files, without recompiling the DLL. It was developed mainly with OpenAI Codex (an AI coding assistant).

### What's new in 0.9.2

- **Vanilla script support:** the conditions and effects of reforms, decisions, decrees and incidents, as well as condition blocks such as entries, member filters and office eligibility, are now read by the game's own parser. You can write vanilla triggers and effects, your own scripted triggers and effects, and switch to scopes such as `capital_scope`.
- **Continuous modifiers:** reforms and decrees can apply modifiers to their beneficiaries, the leader, the members or the system's territory.
- **Notices:** the new `political_send_message` sends a popup with buttons, or just a message-log entry, to chosen countries. The Coastal Charter example has a ready-made button.
- **Complete examples:** both editable examples now include their outer `.mod` file, so you can copy them and enable them in the launcher.

### Current status

| Item | Status |
| --- | --- |
| Ashikaga Shogunate and Mongol Empire (Workshop examples) | The previous build, 0.9.1, was confirmed in a campaign to open alongside the original HRE and Empire of China. This release's trimmed content passes preflight but is **not yet tested in a campaign**. Many text and interface problems remain |
| Vanilla trigger/effect support | Preflight checks file structure and reports line numbers; actual behaviour **must be checked in game** |
| Notice popups (`political_send_message`) | Pass offline tests, **not yet tested in a campaign** |
| HRE-style and Empire-of-China-style presets | Shipped on the Workshop and used by the two examples |
| Papacy-style and Native-Federation-style presets | Only in the editor templates, **untested** |
| Systems built from scratch (`examples/FromScratch`) | Pass preflight, **reliability untested** |
| Editor 1.0.11 | Windows regression passed (new project, editing, save and reopen, export) |
| Framework plugin 0.9.2 | 15 offline test programs pass; 114 symbols checked against Steam EU4 1.37.5 |

See the [validation record](docs/Validation.md) for details.

### What to download

| Download | Where | Purpose | Required? |
| --- | --- | --- | --- |
| Anvil 1.24.0 (Windows x64) | [Upstream release](https://github.com/happy-dwarf-team/anvil/releases/tag/v1.24.0) | Plugin loader and its standard `anvil-game` plugin | Yes |
| `Political-Framework-Runtime-Beta-0.1.zip` | This repository's [Releases](https://github.com/<GitHub用户名>/<仓库名>/releases) | Framework plugin 0.9.2 | Yes |
| Political Framework - Beta 0.1 | Steam Workshop: `<Workshop物品链接>` | Shogunate and Mongol example content | To see the examples |
| `EU4-Political-Editor-Beta-0.1.zip` | This repository's [Releases](https://github.com/<GitHub用户名>/<仓库名>/releases) | Visual editor 1.0.11 (needs .NET Framework 4.8) | Only for making mods |

### Quick install

1. Close the game. Install Anvil 1.24.0 into the folder that contains `eu4.exe`, following the upstream instructions.
2. Unzip the Runtime package and copy the whole `anvil/plugins/great-khan-native/` folder into `anvil/plugins/` in your game folder.
3. Subscribe to the Workshop item and enable it in your launcher playset.
4. Start an ordinary single-player game in 1444 as Ashikaga (ASK) or the Mongols (KHA) and look for the icon beside the minimap.

> [!IMPORTANT]
> - **Chinese localisation DLL compatibility.** The framework works with the Mei localisation plugin through Anvil. Anvil supplies `version.dll` in the game directory. Mei loads from `anvil/plugins/mei/` and handles UTF-8 localisation and legacy EU4DLL-encoded text. The framework uses the text loaded by the game. Mei 0.9.0 and framework 0.9.1 were verified to load together. Framework 0.9.2 still needs a retest. If an older patch also installs its own `version.dll` in the game directory, it will overwrite the Anvil loader. Back up the existing file before replacing it.
> - This mod replaces `interface/messagetypes.txt`. Other mods that replace the same file will overwrite each other; merge the message entries by hand.

For checks after every step and troubleshooting, see the **[installation guide](docs/Install-Guide.en.md)**.

### For mod authors: where to start

Suggested order:

1. Follow section 3 of the **[modder tutorial](docs/Modder-Tutorial.en.md)** to build your first system by inheriting a preset (about five files, no GUI work).
2. When you need a feature, check the recipes in section 4 of the tutorial, or [`examples/DefaultType/Default-Fields.zh-CN.txt`](examples/DefaultType/Default-Fields.zh-CN.txt), a full field reference (comments in Chinese).
3. To read or change systems from vanilla events, or to see what `ROOT` and `FROM` mean in each block, read section 4.8 of the tutorial. The full scope reference, [Native-Scripting.zh-CN.md](docs/Native-Scripting.zh-CN.md), is currently in Chinese only.
4. When you want your own window, read section 5 of the tutorial, "Building from scratch" (reliability untested in Beta 0.1).
5. To drag controls, preview panels and check for errors, use the editor; see the [editor manual](docs/Editor-Manual.en.md).

### Repository layout

| Folder | Contents |
| --- | --- |
| `docs/` | Installation guide, modder tutorial, vanilla scripting reference, editor manual, validation record, release notes, and the installation guide PDFs |
| `examples/` | Field reference `DefaultType`, preset-inheriting example `ReuseTemplate/coastal_charter`, from-scratch example `FromScratch/harbor_moot` |
| `editor/` | Editor source and its built-in preset templates (including Papacy and Native Federation) |
| `anvil/` | Framework plugin source, symbol declarations and the exported script interface `api_manifest.json` |
| `licenses/` | Third-party licenses (Anvil) |

To build from source, see [BUILD.md](BUILD.md).

### Maintenance

The author has finished the development phase and is now taking a long break, so **updates and support are not guaranteed**. You are welcome to keep modifying and improving this release; just follow the MIT License and keep the original copyright notice. Forks, issues and pull requests are welcome, but replies may be slow; see [CONTRIBUTING.md](CONTRIBUTING.md). If you maintain an actively updated fork, let us know and it will be linked here.

### License and disclaimer

- The original editor and framework code are released under the [MIT License](LICENSE), copyright © 2026 Political Framework contributors. Keep the copyright notice and license text when redistributing the code or programs built from it.
- Anvil, its SDK and `anvil-game` are by Happy Dwarf Team under their own [MIT License](licenses/Anvil-MIT.txt).
- This is an unofficial project, not affiliated with or endorsed by Paradox Interactive, and requires a legitimate copy of the game. The Workshop content's `interface/messagetypes.txt` is a complete copy of the EU4 1.37.5 message settings table with five framework entries appended; the original table belongs to Paradox. Apart from that file, no original game files are included, and other game sprites are used by referencing your locally installed game.
- The framework modifies the game process at runtime. Use it at your own risk; no warranty is provided.

See [NOTICE.md](NOTICE.md).

**Long live EU4, and may your campaigns be ever victorious!**

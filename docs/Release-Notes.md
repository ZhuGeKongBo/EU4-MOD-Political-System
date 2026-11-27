# Public Beta 0.1

> **注意：本版本仅供 MOD 开发参考，游戏中尚未加入完整的玩法机制与帮助说明，没有必要开局正式游玩（测试除外）。**
> **Note: this is a reference build for mod developers. Full gameplay mechanics and in-game help are not added yet, so there is no need to start a real campaign (except for testing).**

## 已知问题 · Known issues

- 教廷、原住民联盟两种预设，以及从零自建政治体系的可靠性尚未测试。
  The Papacy and Native Federation presets, and political systems built from scratch, have not been tested for reliability.
- 室町幕府与蒙古帝国两套示例中，许多文本与接口仍有问题。
  The Ashikaga Shogunate and Mongol Empire examples still have many problems with their texts and interfaces.
- 上一版 0.9.1 曾在战役中确认幕府、蒙古面板能与原版神罗、天朝同时打开；本版精简后的 Workshop 内容和新增的通知弹窗尚未在战役中实测。
  Build 0.9.1 was confirmed in a campaign to open the Shogunate and Mongol panels alongside the original HRE and Empire of China; this release's trimmed Workshop content and the new notice popups have not yet been tested in a campaign.
- 新接入的原版 trigger / effect 只经过游戏外预检，实际运行结果需进游戏验证。
  The newly connected vanilla triggers and effects have only passed preflight outside the game; their behaviour must be checked in game.
- Workshop 内容替换了 `interface/messagetypes.txt`，同样替换这个文件的其他 MOD 需要手动合并消息注册项。
  The Workshop content replaces `interface/messagetypes.txt`; other mods that replace the same file need their message entries merged by hand.
- 汉化补丁（EU4dll 双字节 / UTF-8）与 Anvil 都使用 `version.dll`，两者共存的方法尚未验证。
  The Chinese patches (EU4dll double-byte / UTF-8) and Anvil both use `version.dll`; a way to run both has not been verified.

## 组件 · Components

| 组件 Component | 版本与位置 Version and location |
| --- | --- |
| 框架插件 Framework runtime plugin | 0.9.2，本页附件 · in the assets below |
| 编辑器 Windows editor | 1.0.11，本页附件 · in the assets below |
| Anvil 与 `anvil-game` | 1.24.0，[上游 upstream](https://github.com/happy-dwarf-team/anvil/releases/tag/v1.24.0) |
| 游戏内容 Game content | Beta 0.1，Steam 创意工坊 · Steam Workshop |
| 目标游戏 Target game | Steam EU4 1.37.5，Windows x64，普通单人 · ordinary single-player |

## 本版变化 · What's new

- **原版脚本接入。** 改革、决议、诏令、事变的条件与效果，以及入口、成员筛选、职位资格、提名、防御义务、资源月变等条件块，改由游戏自己的解析器读取，可写原版 trigger / effect、自定义 scripted trigger / effect，并切换到首都等合法作用域。此前“条件只能用少数几种、按钮效果只有威望”的限制已取消。
  **Vanilla script support.** The conditions and effects of reforms, decisions, decrees and incidents, and condition blocks such as entries, member filters, office eligibility, nomination, defensive obligations and monthly resource terms, are now read by the game's own parser. They accept vanilla triggers and effects, scripted triggers and effects, and legal scope switches such as the capital. The earlier limits (a few conditions only, prestige-only button effects) are gone.
- **改革与诏令。** 改革新增 `on_effect` / `off_effect`，诏令新增 `removed_effect`；两者都可附 `modifier`、`emperor`、`member`、`province` 四种持续修正；`ai_will_do` 按原版权重求值。
  **Reforms and decrees.** Reforms gain `on_effect` / `off_effect` and decrees gain `removed_effect`. Both accept the continuous modifiers `modifier`, `emperor`, `member` and `province`. `ai_will_do` is evaluated as a vanilla weight.
- **通知。** 新增 `messages` 定义和 `political_send_message` 效果：标题、横幅、1–4 段正文、日志文字、一到两个按钮，可选择弹窗或只记日志。海岸宪章示例附带现成按钮。
  **Notices.** New `messages` definitions and the `political_send_message` effect: title, banner, 1–4 body paragraphs, log text and one or two buttons, shown as a popup or as a log entry only. The Coastal Charter example has a ready-made button.
- **内容与示例。** 幕府成员随游戏中的大名附属关系变化；蒙古帝国有了自己的布局。两个可编辑示例都附上了外层 `.mod` 文件。
  **Content and examples.** Shogunate membership follows the daimyo relations in game, and the Mongol Empire has its own layout. Both editable examples now include their outer `.mod` file.
- **文档。** 安装教程与 Modder 教程（中英）按 0.9.2 更新；新增中文的原版脚本接入说明。
  **Documentation.** The installation guide and modder tutorial (Chinese and English) are updated for 0.9.2, and a Chinese vanilla scripting reference has been added.

## 安装 · Installation

见 [安装教程](Install-Guide.zh-CN.md) · See the [installation guide](Install-Guide.en.md).

做 MOD 见 [Modder 教程](Modder-Tutorial.zh-CN.md) · For modding, see the [modder tutorial](Modder-Tutorial.en.md).

## 校验值 · Checksums (SHA-256)

```text
00413420af984f0a94fc505605495a8e84e7cb34cd73020febc7dd669f16adbe  EU4-Political-Editor-Beta-0.1.zip
82d554c478baac209eed791fad7e58fe0854fc4abd46148f8397f2a29b0216bb  Political-Framework-Runtime-Beta-0.1.zip
```

## 许可 · License

自研代码采用 MIT，版权 © 2026 Political Framework contributors。作者已进入怠惰期，欢迎在此基础上继续开发。
Original project code is MIT, copyright © 2026 Political Framework contributors. The author is taking a long break; you are welcome to continue development from here.

[License](../LICENSE) · [Notices](../NOTICE.md) · [Contributing](../CONTRIBUTING.md) · [Validation](Validation.md) · [Script API](../anvil/api_manifest.json)

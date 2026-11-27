# Public Beta 0.1

Steam EU4 1.37.5 x64 for Windows. Runtime **0.9.2**, editor **1.0.11**, Anvil **1.24.0** with `anvil-game`. Install the plugin from GitHub Releases and enable the Workshop content in the launcher.

## Release contents

- Ashikaga uses the Celestial-style window. Its member list follows live daimyo relations.
- The Mongol Empire uses an HRE-style window with its own authored layout, religion-based membership and six dated Tumen appointments. The two systems have separate resources and history.
- Supported condition and effect blocks accept vanilla or authored scripted clauses in the applicable scope. Actions can use native modifiers and the original country, province and nested scopes. The native parser checks the clauses when the game loads.
- `political_send_message` supports a localised title, ordered body, log text and one or two dismiss buttons. Script filters decide who is addressed. The local player may receive the notice as a popup or a log entry. Coastal Charter has an example action.
- The Workshop uses the complete EU4 1.37.5 `interface/messagetypes.txt` with five appended registration entries. Mods that use the same file need to merge their registration entries into the loaded table.

[Installation](Install-Guide.en.md) · [Validation](Validation.md) · [Script fields](../anvil/api_manifest.json)

## 中文变更

| 旧版说法 | 本包实际行为 |
| --- | --- |
| 条件块只能写少数几种国家条件 | 已接入的字段可由原版解析器读取合法作用域下的原版 trigger 和自定义 scripted trigger。 |
| 行动效果只有威望 | 改革、决议、诏令、事变等已接入的效果可以调用原版 effect、自定义 scripted effect，并从国家切到首都等合法作用域。 |
| 邀请只能执行固定加入语句 | 加入类邀请仍需名单可以增减的成员来源。接受后可在受邀国作用域执行原版效果。 |
| 原版消息只有框架内置通知 | 0.9.2 新增 `political_send_message`、`messages` 和 `popup`。海岸宪章示例附按钮、两种通知和动态本地化。 |
| 编辑器适配 0.8.9 | 本包编辑器 1.0.11 搭载 0.9.2 预检。 |

教廷和原住民联盟预设仍是编辑器内的创作参考。真战役中的原版新消息渲染待验。Workshop 首发只启用幕府和蒙古。 [中文安装](Install-Guide.zh-CN.md) · [原版接口与作用域](Native-Scripting.zh-CN.md) · [带注释的字段参考](../examples/DefaultType/Default-Fields.zh-CN.txt)


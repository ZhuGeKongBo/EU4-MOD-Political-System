# 示例 · Content examples

[中文](#中文) · [English](#english)

## 中文

| 示例 | 是什么 | 怎么用 |
| --- | --- | --- |
| `DefaultType/Default-Fields.zh-CN.txt` | 带中文逐行注释的全功能字段参考，几乎包含所有可启用的功能 | 当“字典”查：需要哪个功能，就把那一段复制到你自己的类型里 |
| `DefaultType/default.example.txt` | 一个最简的目录级 `default` 母版 | 只有当你希望所有未写 `copy_from` 的类型都继承它时，才把改好的副本放进 `common/political_systems/types/`。同一套启用内容里只能有一个 `default` |
| `ReuseTemplate/coastal_charter/` | 继承 Beta 天朝式预设的小型 MOD，带原版效果的决议和通知按钮 | **新手从这里开始。** 需要和 Beta 0.1 同时启用。逐行讲解见 [Modder 教程第 3 节](../docs/Modder-Tutorial.zh-CN.md#3-教程一继承预设做第一个自己的体系)，按钮和通知见第 4.5、4.7 节 |
| `FromScratch/harbor_moot/` | 不依赖预设、自带窗口和入口的独立 MOD | 想做自己的窗口时参考。Beta 0.1 中可靠性未测试。讲解见 [Modder 教程第 5 节](../docs/Modder-Tutorial.zh-CN.md#5-教程二从零自建含自己的窗口) |

### 把示例装进游戏

每个示例文件夹旁边都附有外层 `.mod` 文件（`coastal_charter.mod`、`harbor_moot.mod`）：

1. 把示例文件夹和旁边的 `.mod` **一起**复制到 `文档\Paradox Interactive\Europa Universalis IV\mod\`。
2. 在启动器里启用它。`coastal_charter` 还要同时启用 Political Framework - Beta 0.1。
3. 所有示例都需要框架插件 0.9.2。

也可以用编辑器的「预检与导出…」导出完整内容包。

预检通过只说明文件写法正确，游戏里的实际表现仍需单独测试。

## English

| Example | What it is | How to use it |
| --- | --- | --- |
| `DefaultType/Default-Fields.zh-CN.txt` | A full field reference covering almost every feature (comments in Chinese) | Use it as a dictionary: copy the section for the feature you need into your own type |
| `DefaultType/default.example.txt` | A minimal catalogue-wide `default` | Move an edited copy into `common/political_systems/types/` only if you want every type without `copy_from` to inherit it. Only one `default` may exist in the enabled content |
| `ReuseTemplate/coastal_charter/` | A small mod that inherits Beta's Empire-of-China-style preset, with a vanilla-effect decision and a notice button | **Start here.** Enable it together with Beta 0.1. Explained line by line in [section 3 of the modder tutorial](../docs/Modder-Tutorial.en.md#3-tutorial-1-inherit-a-preset-to-make-your-first-system); buttons and notices in sections 4.5 and 4.7 |
| `FromScratch/harbor_moot/` | A standalone mod with its own window and entry, no presets | Reference for building your own window. Reliability untested in Beta 0.1. See [section 5 of the modder tutorial](../docs/Modder-Tutorial.en.md#5-tutorial-2-build-from-scratch-with-your-own-window) |

### Installing an example

Each example folder comes with its outer `.mod` file next to it (`coastal_charter.mod`, `harbor_moot.mod`):

1. Copy the example folder and its `.mod` **together** into `Documents\Paradox Interactive\Europa Universalis IV\mod\`.
2. Enable it in the launcher. `coastal_charter` also needs Political Framework - Beta 0.1 enabled.
3. All examples need framework plugin 0.9.2.

Alternatively, use the editor's **Preflight and export** to export a full package.

Passing preflight only means the files are written correctly. Behaviour in game still needs its own test.

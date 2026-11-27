# Political Framework 安装手册

**公开 Beta 0.1 · 框架 0.9.2 · 编辑器 1.0.11**

适用 Windows x64 上的 Steam EU4 1.37.5。第一次进游戏请选择 1444 年普通非铁人单人战役。

## 1. 先备齐三样

| 文件 | 去哪里找 | 用处 |
| --- | --- | --- |
| Anvil 1.24.0 Windows x64 | [Anvil 上游发行页](https://github.com/happy-dwarf-team/anvil/releases/tag/v1.24.0) | 加载器和标准插件 `anvil-game` |
| 框架运行时 ZIP | 本项目 GitHub Releases | 自研框架插件 0.9.2 |
| **Political Framework - Beta 0.1** | Steam Workshop，或本地内容 ZIP | 幕府、蒙古、面板及本地化 |

编辑器是可选的。想自己改 Mod，另下载 `EU4-Political-Editor-Beta-0.1.zip`，解压后双击 `EU4PoliticalEditor.exe`。

### 找到两个文件夹

- **游戏目录**里有 `eu4.exe`。在 Steam 库右键 EU4，选择「管理」→「浏览本地文件」。Anvil 和框架插件放这里。
- **玩家目录**通常在「文档 → Paradox Interactive → Europa Universalis IV」。本地 Mod 放这里的 `mod`，存档也在玩家目录下。文档有重定向时，以游戏实际用的目录为准。 [Paradox 的 Windows 路径说明](https://support.paradoxplaza.com/hc/en-us/articles/203089358-Europa-Universalis-IV-file-locations)

## 2. 装加载器和框架插件

1. 退出 EU4。按 [Anvil 1.24.0 安装说明](https://github.com/happy-dwarf-team/anvil/releases/tag/v1.24.0)把 Windows x64 发行档放到游戏目录。检查上游 `version.dll` 和 `anvil-game` 插件。如果游戏目录已有旧翻译工具的同名 DLL，先备份并核对它是否支持当前 Anvil 加载方式。
2. 解压本项目的 **Runtime** ZIP。把其中 `anvil/plugins/great-khan-native` 整个文件夹复制到游戏目录下相同位置。
3. 打开框架的 `plugin.toml`，应看到 `version = "0.9.2"`。升级时把 DLL、两个 TOML 和锁文件配套更新。旧版本备份放在 `anvil/plugins` 外面。

~~~text
Europa Universalis IV/
  eu4.exe                              # 找对这一层
  version.dll                          # 来自上游 Anvil
  anvil/
    plugins/
      anvil-game/                      # 来自上游 Anvil
      great-khan-native/               # 复制本项目框架目录到这里
        great_khan_native.dll
        plugin.toml
        symbols.toml
        symbols.lock
~~~

## 3. 启用游戏内容，确认能开面板

- **订阅安装。** 在 Steam Workshop 订阅 **Political Framework - Beta 0.1**，然后在 Paradox 启动器的游玩集启用。
- **手动安装。** 解压 Workshop ZIP，把 `political_framework_beta_01` 文件夹和旁边的 `political_framework_beta_01.mod` 一起放到玩家目录的 `mod`。内容目录的第一层能看到 `descriptor.mod`。随后在启动器启用。

同一版内容任选一种安装方式。首次验收可先停用旧 QA Mod，避免同名定义覆盖。用 Steam 正常启动 1444 年普通单人战役，选足利。小地图旁悬停入口，找 **Open the Ashikaga Shogunate**。打开、关掉再重开，核对领袖和大名盾徽。蒙古可在 1444 年选蒙古国，找 **Open the Mongol Empire**。

想试新消息时，启用源码包里的海岸宪章示例和 Beta 内容，开 1444 法兰西，点骑兵图标，再点 **Send Council Bulletin**。预期弹窗在面板上层，按 **Understood** 后面板仍打开。此项是进游戏的验收步骤。 [本版验证范围](Validation.md)

## 4. 更新或找不到入口时

| 看见什么 | 先检查什么 |
| --- | --- |
| Anvil 日志没更新 | 从 Steam 再定位实际启动的游戏目录，核对 Anvil 文件和 `anvil/anvil.log`。 |
| 日志说插件被跳过 | 核对游戏版本、Anvil 标准插件和本项目四个配套文件。升级前停用旧的冲突插件，例如 `great-khan-history`。 |
| 启动器有 Mod，游戏中没有面板图标 | 查游玩集是否启用该版内容，再用 1444 年足利检查。自定义体系还要查起始日期、领袖首都是否已发现，以及入口条件。 |
| 点图标闪退 | 用编辑器预检，先修最早的 GUI 或绑定错误。对照 `anvil/anvil.log` 和崩溃时间。 |
| 面板正常，只有通知不出现 | 查当前国家是否是收件人、原版消息设置，以及同路径的 `interface/messagetypes.txt` 是否被另一 Mod 覆盖。 |

更新和回退前备份存档及插件。回退时配套恢复之前的内容、插件和存档。报告问题可以附版本、最短操作步骤和删去个人路径的日志片段。

### 编辑器从哪里打开

双击 `EU4PoliticalEditor.exe`，点「打开 Mod…」，选直接包含 `descriptor.mod` 的目录。「路径…」可指定游戏目录和依赖 Mod。先保存工作工程，再把预检通过的内容导出到新目录。 [编辑器手册](Editor-Manual.zh-CN.md)


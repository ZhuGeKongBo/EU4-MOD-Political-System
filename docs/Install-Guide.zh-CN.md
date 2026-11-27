# 安装教程（Beta 0.1）

[English](Install-Guide.en.md)

**公开 Beta 0.1 · 框架 0.9.2 · 编辑器 1.0.11**

> **适用环境：** Windows、Steam 版 EU4 **1.37.5**、普通单人游戏。其他版本、其他商店、铁人与联机均未测试。
>
> 按第 1 步到第 5 步的顺序做。每一步末尾都有“检查一下”，对得上再往下走。

## 需要下载的东西

| 序号 | 下载什么 | 去哪下载 | 用来做什么 | 必需？ |
| --- | --- | --- | --- | --- |
| ① | Anvil 1.24.0（Windows x64） | [Anvil 上游发布页](https://github.com/happy-dwarf-team/anvil/releases/tag/v1.24.0) | 让游戏能加载插件的“加载器” | 必需 |
| ② | `Political-Framework-Runtime-Beta-0.1.zip` | 本项目 [Releases](https://github.com/<GitHub用户名>/<仓库名>/releases) | 本框架插件 0.9.2（DLL） | 必需 |
| ③ | Political Framework - Beta 0.1 | Steam 创意工坊 | 幕府、蒙古两套示例内容 | 想看示例时必需 |
| ④ | `EU4-Political-Editor-Beta-0.1.zip` | 本项目 [Releases](https://github.com/<GitHub用户名>/<仓库名>/releases) | 可视化编辑器 1.0.11 | 只在做 MOD 时需要 |

## 先认识两个文件夹

这是最容易搞混的地方，后面会反复用到：

| 名称 | 在哪里 | 怎么打开 | 里面放什么 |
| --- | --- | --- | --- |
| **游戏目录** | 有 `eu4.exe` 的文件夹 | Steam 库 → 右键 Europa Universalis IV → 管理 → 浏览本地文件 | Anvil 和本框架插件装在这里 |
| **文档目录** | `文档\Paradox Interactive\Europa Universalis IV\` | 资源管理器打开“文档” | `mod`、`save games`、`logs`；手动装 MOD 用这里 |

“文档”被重定向到其他盘时，以游戏实际使用的目录为准。可参考 [Paradox 的文件位置说明](https://support.paradoxplaza.com/hc/en-us/articles/203089358-Europa-Universalis-IV-file-locations)。

## 第 1 步：备份（装过汉化补丁的一定要看）

打开**游戏目录**，看看有没有 `version.dll`。

- **没有：** 直接进入第 2 步。
- **有：** 它多半来自双字节补丁（EU4dll）或 UTF-8 汉化补丁。Anvil 也使用 `version.dll`，安装 Anvil 会把它覆盖掉，汉化补丁就会失效；反过来重装汉化补丁，Anvil 也会失效。**两者共存的方法本项目尚未验证。**

  先把原来的 `version.dll` 复制到游戏目录以外的地方备份，再决定是否继续。

另外建议把 `文档\...\save games` 里重要的存档也备份一份。

## 第 2 步：安装 Anvil

1. 完全退出游戏。
2. 打开 [Anvil 1.24.0 发布页](https://github.com/happy-dwarf-team/anvil/releases/tag/v1.24.0)，下载 Windows x64 安装包。
3. 按上游页面的说明，安装到**游戏目录**（有 `eu4.exe` 的那个文件夹）。

**检查一下：** 游戏目录里应该出现 `version.dll`，以及 `anvil\plugins\anvil-game\` 文件夹。

## 第 3 步：放入本框架插件

1. 解压 `Political-Framework-Runtime-Beta-0.1.zip`。
2. 在解压出来的文件里找到 `anvil\plugins\great-khan-native\` 这个文件夹。
3. 把**整个** `great-khan-native` 文件夹复制到游戏目录的 `anvil\plugins\` 里面。

装好后，游戏目录应该是这样：

```text
Europa Universalis IV\          ← 有 eu4.exe 的文件夹
├─ eu4.exe
├─ version.dll                  ← Anvil 提供
└─ anvil\
   └─ plugins\
      ├─ anvil-game\            ← Anvil 提供
      └─ great-khan-native\     ← 本项目插件
         ├─ great_khan_native.dll
         ├─ plugin.toml
         ├─ symbols.toml
         └─ symbols.lock
```

**检查一下：** 用记事本打开 `plugin.toml`，能看到 `version = "0.9.2"`。

> - 这四个文件是一套，必须来自同一个版本。以后升级时四个一起换，不要混用。
> - 从旧版升级时，把旧版插件文件夹整个移到 `anvil\plugins` **外面**备份，不要留在里面。
> - 如果以前装过测试用的 `great-khan-history` 插件，也把它移出 `anvil\plugins`。它和本插件冲突，不能同时启用。

## 第 4 步：启用示例内容（两种方式选一种）

### 方式 A：订阅创意工坊（推荐）

1. 订阅 **Political Framework - Beta 0.1**。
2. 打开 Paradox 启动器 → 播放集 → 启用这个 MOD。

### 方式 B：手动安装

1. 把 `political_framework_beta_01` 文件夹和旁边的 `political_framework_beta_01.mod` 一起复制到**文档目录**的 `mod` 文件夹。
2. 用记事本打开 `political_framework_beta_01.mod`，确认内容是：

   ```text
   name = "Political Framework - Beta 0.1"
   version = "0.1"
   supported_version = "1.37.5"
   tags = { "Gameplay" }
   path = "mod/political_framework_beta_01"
   ```

3. 打开启动器，在播放集中启用。

> A 和 B 不要同时用。第一次测试时，先在播放集里关掉旧的测试版 MOD，以及其他会修改界面或 `interface/messagetypes.txt` 的 MOD。

## 第 5 步：进游戏检查

1. 从启动器启动游戏，开新局，用默认的 **1444.11.11** 剧本。
2. 选**足利（ASK，日本）**查看室町幕府；或者另开一局选**蒙古（KHA）**查看蒙古帝国。
3. 小地图旁应该出现一个图标。鼠标悬停，提示应为 **Open the Ashikaga Shogunate**（蒙古为 **Open the Mongol Empire**）。点击打开面板，核对领袖和大名盾徽，然后关闭、再打开一次。
4. 查看日志：用记事本打开游戏目录里的 `anvil\anvil.log`，搜索 `great-khan-native`，确认插件已被加载。

都正常就说明安装成功。Beta 0.1 的面板主要用来查看，不必继续游玩。

### 可选：试一试新的通知弹窗

0.9.2 新增了通知功能，海岸宪章示例里有一个现成按钮。这一项尚未在战役中实测，欢迎帮忙验证：

1. 把源码包里的 `examples/ReuseTemplate/coastal_charter` 文件夹和旁边的 `coastal_charter.mod` 一起复制到文档目录的 `mod` 文件夹。
2. 在播放集中同时启用 Political Framework - Beta 0.1 和 Coastal Charter。
3. 开 1444 新局，选**法兰西**，点小地图旁的骑兵图标，再点面板里的 **Send Council Bulletin**。
4. 预期结果：弹窗出现在面板上层；点 **Understood** 后弹窗关闭，政治面板仍然开着；消息日志里多一条记录。

结果和预期不同的话，欢迎提交 Issue。

## 卸载 / 回退

1. 退出游戏，在启动器里停用 MOD。
2. 把游戏目录里的 `anvil\plugins\great-khan-native\` 删除或移出游戏目录。
3. 如果不再使用 Anvil，按上游说明卸载，再把第 1 步备份的 `version.dll` 放回去。

> 用本 MOD 创建的存档，在不装本 MOD 时能否正常读取，尚未确认。请保留安装前的存档备份。

## 出问题怎么办

| 现象 | 先这样检查 |
| --- | --- |
| 游戏里没有图标 | ① 是不是 1444 新局，选的是不是 ASK 或 KHA；② 启动器里 MOD 有没有启用；③ `anvil.log` 里有没有 `great-khan-native`；④ 有没有别的 MOD 改了同一批界面文件。自己做的体系，还要查历史日期是否早于开局、入口的 `visible` 条件是否满足 |
| 没有 `anvil.log`，或日志时间没更新 | Anvil 没有生效。确认装在 `eu4.exe` 所在的文件夹；检查 `version.dll` 是不是又被汉化补丁覆盖了 |
| 日志显示插件被跳过 | 多半是游戏版本不是 1.37.5，或插件文件混用了不同版本，或旧的 `great-khan-history` 插件还在。重新放一整套四个文件，并移走冲突插件 |
| 点图标后闪退 | 记下时间，查看游戏目录的 `anvil\crashes\` 和 `anvil.log` 中的第一条错误。自己做的 MOD 先用编辑器预检，从最早的一条 GUI 或绑定错误改起 |
| 面板正常，但通知不出现 | 当前国家是不是收件人；游戏的消息设置有没有关掉这类消息；`interface/messagetypes.txt` 有没有被别的 MOD 覆盖 |
| 面板文字是英文 | Beta 0.1 的示例文本以英文为主 |
| 面板上显示的是代码名（如 `shogun_title`） | 本地化没有加载。确认 MOD 已启用；自己改过本地化文件的，见 Modder 教程的“常见错误” |
| 杀毒软件报毒 | 运行时注入的 DLL 常被误报。只从本项目 Releases 下载，并按下一节核对校验值 |

## 核对下载的文件

在压缩包所在的文件夹里，按住 Shift 右键 → “在此处打开 PowerShell 窗口”，运行：

```powershell
Get-FileHash .\Political-Framework-Runtime-Beta-0.1.zip -Algorithm SHA256
```

把输出的长串和 Releases 页面上 `SHA256SUMS.txt` 里的值对比，完全一致才说明文件没被改动。

## 提交问题时请附上

EU4、Anvil、框架插件和编辑器的版本号，最短的复现步骤，以及删掉个人路径和用户名后的日志片段。详见 [CONTRIBUTING.md](../CONTRIBUTING.md)。

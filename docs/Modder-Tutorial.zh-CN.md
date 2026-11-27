# Modder 教程（Beta 0.1 · 框架 0.9.2）

[English](Modder-Tutorial.en.md)

写给会改 EU4 脚本（事件、决议、国策）、但不写 C/C++ 的 MOD 作者。全程只改文本文件，不需要编译任何东西。

> [!WARNING]
> Beta 0.1 中，教廷、原住民联盟两种预设以及从零自建体系的可靠性**尚未测试**；室町幕府与蒙古帝国示例的文本与接口**仍有许多问题**。0.9.2 新接入的原版 trigger / effect 和通知弹窗只经过游戏外的预检。本教程的每一步改完后，都请进游戏实测。

## 目录

0. [开始前的准备](#0-开始前的准备)
1. [四个概念](#1-四个概念)
2. [文件放在哪](#2-文件放在哪)
3. [教程一：继承预设，做第一个自己的体系](#3-教程一继承预设做第一个自己的体系)
4. [常见改法](#4-常见改法)
5. [教程二：从零自建（含自己的窗口）](#5-教程二从零自建含自己的窗口)
6. [Beta 0.1 的限制](#6-beta-01-的限制必读)
7. [常见错误](#7-常见错误)
8. [用编辑器省事](#8-用编辑器省事)

---

## 0. 开始前的准备

- 已按 [安装教程](Install-Guide.zh-CN.md) 装好 Anvil 和框架插件 0.9.2，能在游戏里看到幕府或蒙古的图标。
- 一个能指定编码的文本编辑器，推荐 VS Code 或 Notepad++。本地化文件要存成 **UTF-8 带 BOM**。
- 可选：编辑器 `EU4PoliticalEditor.exe` 1.0.11，可以预览面板、检查错误（第 8 节）。

## 1. 四个概念

| 名称 | 是什么 | 打个比方 |
| --- | --- | --- |
| **类型**（type） | 一套规则：谁是成员、有哪些职位、资源、按钮，用哪个面板 | “神圣罗马帝国”这套制度本身 |
| **实例**（instance） | 按某个类型建立起来的一个具体组织，写在历史文件里 | 1444 年实际存在的那一个神罗 |
| **布局**（layout） | 面板用哪个 GUI 窗口，每个控件显示什么 | 神罗窗口长什么样 |
| **入口**（entry） | 主界面上打开面板的图标 | 小地图旁那个神罗图标 |

几点要记住：

- 一个类型可以建多个实例，例如两个互不相干的联盟，它们互不干扰。点入口时，如果有多个实例可用，会弹出选择窗口。
- **预设**是写了 `abstract = yes` 的类型，只供继承，不会自己出现在游戏里。Beta 0.1 的 Workshop 内容带有两个预设：`hre_style`（神罗式）和 `celestial_style`（天朝式）。
- 子类型用 `copy_from = 预设名` 继承预设。没写的字段沿用父级，写了的覆盖父级。
- 原版的 `is_emperor`、`is_emperor_of_china` 这类条件仍然只针对原版的神罗和天朝。你的体系要用框架提供的 `political_*` 语句（第 4.8 节）。

## 2. 文件放在哪

路径都相对于你的 MOD 根目录：

| 路径 | 写什么 | 必需？ |
| --- | --- | --- |
| `descriptor.mod` | MOD 名称、版本、支持的游戏版本 | 必需 |
| `common/political_systems/types/*.txt` | 类型规则 | 必需 |
| `history/political_systems/*.txt` | 实例：领袖、成员、职位、开局日期 | 必需 |
| `localisation/*_l_english.yml` | 玩家看到的文字 | 必需 |
| `common/political_systems/localisation/*.txt` | 提示文字里的动态参数 | 可选 |
| `common/political_system_layouts/*.txt` | 布局与入口区域 | 自建窗口时才需要 |
| `interface/*.gui`、`interface/*.gfx` | 窗口、控件、图标 | 自建窗口时才需要 |

继承预设时，布局和 GUI 用 Beta 自带的，所以只需要前四类文件。

---

## 3. 教程一：继承预设，做第一个自己的体系

**目标：** 做一个以法国为领袖、套用天朝式面板的“海岸宪章”（Coastal Charter）。

这就是仓库里的 `examples/ReuseTemplate/coastal_charter/`。下面先做出最小可用的版本，第 4 节再逐步加上示例里的按钮和通知。

### 3.1 建 MOD 文件夹

在**文档目录**的 `mod` 文件夹里放好这些文件：

```text
mod\
├─ coastal_charter.mod                ← 外层描述文件，给启动器看
└─ coastal_charter\
   ├─ descriptor.mod                  ← 内层描述文件
   ├─ common\political_systems\types\coastal_charter.txt
   ├─ common\political_systems\localisation\coastal_terms.txt
   ├─ history\political_systems\coastal_charter.txt
   └─ localisation\coastal_charter_l_english.yml
```

> 仓库示例已经带好了外层 `coastal_charter.mod`。直接复制的话，把文件夹和旁边的 `.mod` **一起**复制过去即可。

外层 `coastal_charter.mod` 的内容：

```text
name = "Coastal Charter - Framework Beta Example"
version = "0.1"
supported_version = "1.37.5"
tags = { "Gameplay" }
path = "mod/coastal_charter"
```

内层 `descriptor.mod` 的内容相同，只是**没有最后一行 `path`**。

### 3.2 写类型

`common/political_systems/types/coastal_charter.txt`：

```text
coastal_charter = {                       # 类型 ID：全局唯一，用小写英文和下划线
    copy_from = celestial_style           # 继承天朝式预设。必须写在第一行
    title = coastal_title                 # 类型名称（本地化键）
    members = {
        source = explicit                 # 成员 = 历史文件里写明的名单
        subject_type = clear              # 清掉父级的“朝贡国”设定
        title = coastal_members           # 面板上成员区的标题
    }
    resources = {
        mandate = { tooltip = coastal_reserve_tip }   # 父级自带的资源，这里只换提示文字
    }
    interface = {
        title_key = coastal_title         # 面板标题
        entry_button = {
            icon = GFX_pf075_expanded_icon    # 入口图标（骑兵，Beta 已登记）
            order = 30                        # 同一区域内的排序，数字小的靠前
            tooltip = coastal_open            # 鼠标悬停提示
        }
    }
}
```

没写的部分，包括领袖、改革、诏令、面板布局和入口位置，全部沿用 `celestial_style`。

为什么要写 `subject_type = clear`：天朝式预设的成员来源是“领袖的朝贡国”，带着 `subject_type = tributary_state`。这里改成手写名单（`explicit`），就要把父级不再适用的字段清掉，否则加载会报错。

### 3.3 写实例（历史）

`history/political_systems/coastal_charter.txt`：

```text
coastal_charter_1444 = {          # 实例 ID：全局唯一
    type = coastal_charter        # 用哪个类型
    1444.1.1 = {                  # 从这一天起生效，写在开局日期之前
        leader = FRA              # 领袖国
        members = { FRA SAV }     # 成员名单（因为 source = explicit）
    }
}
```

### 3.4 写本地化

`localisation/coastal_charter_l_english.yml`，**存成 UTF-8 带 BOM**，第一行必须是 `l_english:`：

```yaml
l_english:
 coastal_title:0 "Coastal Charter"
 coastal_members:0 "Charter Members"
 coastal_open:0 "Open the Coastal Charter"
 coastal_reserve_tip:0 "$POLITY$: $BALANCE|2$ / $LIMIT|2$\n$DETAIL$"
```

最后一行用到了 `$参数$`。这些参数要在 `common/political_systems/localisation/coastal_terms.txt` 里声明它们取什么值：

```text
coastal_reserve_tip = {
    parameters = { POLITY = system BALANCE = value LIMIT = maximum DETAIL = breakdown }
}
```

| 参数 | 取值 |
| --- | --- |
| `system` | 当前体系的名称 |
| `value` | 资源当前数值 |
| `maximum` | 资源上限 |
| `breakdown` | 来源与计算明细 |

`$BALANCE|2$` 中的 `|2` 表示显示两位小数。改参数名时，文本和声明要一起改。

### 3.5 进游戏测试

1. 在启动器的播放集里，**同时启用** Political Framework - Beta 0.1 和你的 MOD。你的 MOD 用到了 Beta 里的预设、布局和图标，少了 Beta 就无法运行。
2. 开 1444 新局，选法国。
3. 小地图旁应该多出一个骑兵图标，悬停显示 “Open the Coastal Charter”。打开面板，查看领袖和成员。

没出现的话，先看 [第 7 节 常见错误](#7-常见错误)。

---

## 4. 常见改法

> **0.9.2 起，条件块和效果块可以直接写原版脚本。** 改革、决议、诏令、事变的条件与效果，以及入口、成员筛选、职位资格等条件块，都交给游戏自己的解析器读取。你在原版决议、事件里会写的 trigger / effect、自己的 scripted trigger / scripted effect、`capital_scope = { ... }` 这样的作用域切换，基本都能照写。每个块里 `ROOT`、`FROM` 指谁，见 [第 4.8 节](#48-在原版事件决议里读写体系以及-root--from-指谁)。

下面的代码片段都可以直接复制到第 3 节的文件里修改。完整字段见 [`examples/DefaultType/Default-Fields.zh-CN.txt`](../examples/DefaultType/Default-Fields.zh-CN.txt)。

### 4.1 改领袖、改成员，写以后的变化

在历史文件里，日期块可以写多个。后面的日期记录改任、加入和退出：

```text
coastal_charter_1444 = {
    type = coastal_charter
    1444.1.1 = { leader = FRA members = { FRA SAV } }
    1460.1.1 = { add_members = { POR } }        # 1460 年葡萄牙加入
    1480.1.1 = { remove_members = { SAV } }     # 1480 年萨伏依退出
}
```

日期块里可以用的字段：`leader`、`members`、`add_members`、`remove_members`、`roles`、`resources`、`territory`、`add_provinces`、`remove_provinces`。

修改历史后，要开**新局**才能看到效果。旧存档会按存档里记录的状态恢复。

### 4.2 成员从哪里来（五种方式）

```text
# ① 手写名单：在历史文件里写 members = { ... }
members = { source = explicit }

# ② 领袖的附属国（幕府示例的用法）：subject_type 填原版 subject_types 里的 ID
members = { source = leader_subjects subject_type = daimyo_vassal }

# ③ 按条件查询（蒙古示例的用法）：所有满足条件的国家都是成员
members = { source = query source_trigger = { religion = tengri_pagan_reformed } }

# ④ 首都在体系疆域内的国家（神罗式预设的用法）
members = { source = capital_territory }

# ⑤ 混合：按条件查询，再用历史名单增减
members = { source = mixed source_trigger = { religion = catholic is_subject = no } }
```

`source_trigger` 里可以写原版条件。`ROOT` 是正在被判断的国家，`FROM` 是体系领袖。

### 4.3 加一个职位

照抄幕府示例的“管领”。职位写在类型的 `special_members` 里：

```text
special_members = {
    admiral = {                          # 职位 ID
        acquisition = appointed          # 任命制
        max_holders = 3                  # 最多 3 个席位（1 到 32）
        require_member = yes             # 持有人必须是成员
        can_hold_with_leader = no        # 领袖不能兼任
        candidate_trigger = { num_of_ports = 1 }   # 原版条件：至少有一个港口
        retain_trigger = { always = yes }
        title_key = coastal_admiral      # 职位名称（本地化键）
        tooltip = coastal_admiral_tip    # 悬停提示
        # history_key = admiral          # 可选：允许从原版国家历史的 yes/no 字段引入持有人，一般不用写
    }
}
```

在体系历史文件的日期块里，用**职位 ID** 任命：

```text
1444.1.1 = {
    leader = FRA
    members = { FRA SAV POR }
    roles = { admiral = { SAV = yes POR = yes } }
}
```

以后想撤掉某人，就在后面的日期块里写 `roles = { admiral = { SAV = no } }`。

记得在本地化文件里加上 `coastal_admiral` 和 `coastal_admiral_tip`。

### 4.4 加一个资源，或者让它每月增长

照抄幕府示例的“幕府权威”：

```text
resources = {
    mandate = clear                      # 删掉父级的“天命”（不想删就不写这行）
    harbor_dues = {                      # 资源 ID
        initial = 55                     # 初始值
        precision = 2                    # 小数位数（0 到 3）
        name_key = coastal_dues          # 面板上的资源名
        tooltip = coastal_dues_tip       # 悬停提示
        monthly_growth = {
            value = 1                    # 每月 +1
        }
    }
}
```

天朝式预设的改革 `charter` 原本扣的是 `mandate`。删掉 `mandate` 后，要把这项改革的费用改成新资源，否则会报错。幕府示例就是这样做的：

```text
reforms = {
    charter = {
        cost = { resource = harbor_dues payer = instance amount = 20 }
    }
}
```

想让月增长随条件变化，或者读取原版修正，见字段参考里 `monthly_growth` 一段的 `growth_modifier` 和带 `trigger` 的月变项。

### 4.5 加按钮：决议、改革、诏令

三种按钮都显示在面板的行动区（`ui = { region = actions_region }`）：

| 类型 | 字段 | 特点 |
| --- | --- | --- |
| 决议 | `decisions` | 可设为能重复执行 |
| 改革 | `reforms` | 通过一次后生效，可设前置与互斥，可附持续修正 |
| 诏令 | `decrees` | 有期限，到期自动结束，可附持续修正 |

**决议**（海岸宪章示例里的“港口节庆”）：

```text
decisions = {
    port_festival = {
        name = coastal_festival                  # 按钮文字
        tooltip = coastal_festival_tip           # 悬停说明
        actor_rule = leader                      # 只有领袖能点
        trigger = { is_at_war = no }             # 原版条件：和平时才能点
        effect = {                               # 原版效果
            add_treasury = 15
            capital_scope = { set_province_flag = coastal_port_festival }
        }
        cooldown_days = 365                      # 冷却 365 天
        repeatable = yes
        ui = { region = actions_region }
    }
}
```

**改革**：`on_effect` 在通过时执行一次，`off_effect` 在撤销时执行一次。

```text
reforms = {
    harbor_law = {
        name = coastal_harbor_law
        actor_rule = leader
        cost = { resource = mandate payer = instance amount = 20 }
        on_effect = { add_prestige = 5 }         # 通过时执行
        off_effect = { add_prestige = -5 }       # 撤销时执行
        member = { global_trade_power = 0.1 }    # 通过后，所有成员持续获得的修正
        ui = { region = actions_region }
    }
}
```

**诏令**：`effect` 在颁布时执行，`removed_effect` 在到期时执行。

```text
decrees = {
    open_ports = {
        name = coastal_open_ports
        actor_rule = leader
        term = fixed
        duration_days = 180                      # 持续 180 天
        effect = { add_mercantilism = 1 }        # 颁布时执行
        removed_effect = { add_prestige = 1 }    # 到期时执行
        emperor = { trade_efficiency = 0.05 }    # 生效期间，领袖获得的修正
        ui = { region = actions_region }
    }
}
```

改革和诏令可以用的四种持续修正：

| 字段 | 修正给谁 |
| --- | --- |
| `modifier` | 按 `beneficiaries` 设定的受益方 |
| `emperor` | 现任领袖 |
| `member` | 所有当前成员 |
| `province` | 体系领土上的省份（领土要在历史或脚本里声明） |

`actor_rule` 可选：`any`（任何人）、`leader`（领袖）、`member`（成员）、`permission`（拥有某项权限的人，要配合 `actor_permission`）。

想让 AI 也会用这个按钮，加上原版写法的权重：

```text
ai_will_do = { factor = 1 modifier = { factor = 1.25 is_at_war = no } }
```

`ai_will_do` 只决定权重；AI 什么时候尝试、在哪些按钮里挑，仍由框架的规则决定。

### 4.6 改入口：显示条件、可用条件

```text
interface = {
    entry_button = {
        visible = { capital_scope = { continent = europe } }   # 首都在欧洲的国家才能看到图标
        enabled = { is_at_war = no }                           # 和平时才能点开
        tooltip = coastal_open                                 # 正常时的提示
        disabled_tooltip = coastal_locked                      # 置灰时的提示
        toggle = yes                                           # 再点一次图标就关闭面板
    }
}
```

这里的 `ROOT` 是玩家当前操控的国家，`FROM` 是体系领袖。

**可用图标：** Beta 0.1 已登记的入口图标有 `GFX_pf075_hre_icon`（神罗）、`GFX_pf075_celestial_icon`（天命）、`GFX_pf075_expanded_icon`（骑兵）。想用新图标，建议用编辑器「添加组件」向导中的「入口按钮」，它会自动完成登记；手动登记需要改 Beta 的界面文件，容易和 Beta 冲突。

### 4.7 发通知：弹窗或消息日志（0.9.2 新增）

这是海岸宪章示例里的 “Send Council Bulletin” 按钮。分两步：

**第一步：在类型里定义通知长什么样**

```text
messages = {
    council_notice = {                                   # 通知 ID，自己起名
        title = coastal_notice_title                     # 弹窗标题
        subtitle = coastal_notice_subtitle               # 横幅文字（可省略）
        body = { coastal_notice_body coastal_notice_balance }   # 正文，1 到 4 段
        log = coastal_notice_log                         # 消息日志里的文字（可省略，默认用第一段正文）
        options = {
            close = coastal_notice_read                  # 主按钮
            secondary = coastal_notice_file              # 第二个按钮（可省略）
        }
    }
    council_record = {
        title = coastal_record_title
        body = coastal_record_body
        popup = no                                       # 只记入消息日志，不弹窗
    }
}
```

**第二步：在按钮的效果里发送**

```text
decisions = {
    send_council_notice = {
        name = coastal_notice_button
        tooltip = coastal_notice_tip
        actor_rule = leader
        repeatable = yes
        trigger = { ai = no }                            # 只给玩家用
        effect = {
            every_country = {                            # 原版写法：挑出收件国
                limit = { OR = { tag = FRA tag = SAV } }
                political_send_message = {
                    instance = INSTANCE                  # 当前体系
                    message = council_notice
                    actor = ROOT                         # 发送者：点按钮的国家
                }
            }
            political_send_message = {                   # 再给自己记一条日志
                instance = INSTANCE
                message = council_record
                target = ROOT
            }
        }
        ui = { region = actions_region }
    }
}
```

没写 `target` 时，收件国就是当前作用域里的国家，所以放在 `every_country` 里会逐个发送。

本地化里，`$SYSTEM$`（体系名称）、`$SENDER$`（发送国）、`$RECIPIENT$`（收件国）由框架自动填入。自己的数值要在参数文件里声明，例如在正文里显示共用储备：

```yaml
 coastal_notice_body:0 "$SENDER$ sends greetings to $RECIPIENT$."
 coastal_notice_balance:0 "The common reserve is $RESERVE|2Y$."
```

```text
coastal_notice_balance = {
    parameters = { RESERVE = { source = balance id = mandate } }
}
```

通知只发给本地玩家（单人游戏）。弹窗在战役中的显示效果尚未实测。

### 4.8 在原版事件、决议里读写体系，以及 ROOT / FROM 指谁

框架给原版脚本加了一批 `political_*` 语句，可以写进原版事件、决议的条件块和效果块：

```text
# 条件（trigger）
political_is_member = { instance = coastal_charter_1444 country = SAV }
political_has_role = { instance = mongol_system role = tumen country = OIR }

# 效果（effect）
political_change_resource = { instance = mongol_system resource = mongol_resource amount = 5 }
```

`instance` 的写法：

- 在框架自己执行的块里（按钮效果、入口条件等），写 `instance = INSTANCE` 表示“当前这个体系”。
- 在原版事件、决议里，要写**实例 ID**（历史文件最外层的名字，如 `coastal_charter_1444`），或者写 `type = 类型ID`（只在该类型恰好有一个活动实例时可用）。
- `country` 等国家参数可以写国家代码，也可以写 `ROOT`、`THIS`、`FROM`、`PREV`。

常用语句（全部语句及参数见 [`anvil/api_manifest.json`](../anvil/api_manifest.json) 的 `operations`）：

| 语句 | 类型 | 作用 |
| --- | --- | --- |
| `political_is_member` | 条件 | 是否为成员 |
| `political_is_leader` | 条件 | 是否为领袖 |
| `political_has_role` | 条件 | 是否担任某职位 |
| `political_compare_resource` | 条件 | 比较资源数值 |
| `political_member_count` | 条件 | 比较成员数量 |
| `political_has_reform` | 条件 | 是否已通过某项改革 |
| `political_change_resource` | 效果 | 增减资源 |
| `political_set_member` | 效果 | 加入或移出成员（成员来源须为 `explicit` 或 `mixed`） |
| `political_set_role` | 效果 | 任免职位 |
| `political_set_leader` | 效果 | 更换领袖 |
| `political_send_message` | 效果 | 发送通知（第 4.7 节） |

**各个块里 ROOT / FROM 指谁**（完整版见 [原版脚本接入说明](Native-Scripting.zh-CN.md)）：

| 写在哪里 | `ROOT` | `FROM` |
| --- | --- | --- |
| 面板查看、入口 `visible` / `enabled` | 玩家当前国家 | 体系领袖 |
| 成员筛选、加入、退出、保留条件 | 正在被判断的国家 | 体系领袖 |
| 职位与领袖资格 | 正在被判断的国家 | 体系领袖 |
| 按钮的 `potential`、`trigger`、`actor_trigger`、`ai_will_do` | 点按钮的国家 | 目标国（有目标时） |
| 按钮的 `target_trigger` | 目标国 | 点按钮的国家 |
| 按钮的效果 | 按 `beneficiaries` 决定的受益国 | 目标或发起国 |
| 邀请被接受后的效果 | 受邀并加入的国家（已先登记为成员） | 邀请国 |
| 防御义务 | 被攻击的成员 | 攻击方 |

---

## 5. 教程二：从零自建（含自己的窗口）

> [!CAUTION]
> Beta 0.1 中，从零自建体系的可靠性尚未测试。

仓库里的 `examples/FromScratch/harbor_moot/` 是一个完整的独立示例：自己的类型、实例、518×438 的窗口、控件和入口，不依赖 Beta 的预设。它旁边已经带好外层 `harbor_moot.mod`。

### 5.1 六个文件怎么连起来

难点在于名字要一一对上。下面每一行，左边写的名字必须和右边文件里的名字完全一致：

```text
类型 harbor_moot.txt
  interface.layout = harbor_chart ─────────→ 布局文件里的 harbor_chart = { ... }
  entry_button.region = port_buttons ──────→ 布局文件里的 hud_regions = { port_buttons = ... }
  entry_button.icon = GFX_harbor_entry ────→ .gfx 里的 spriteType name

历史 western_harbor.txt
  type = harbor_moot ──────────────────────→ 类型 ID

布局 harbor_chart.txt
  window = western_chart ──────────────────→ .gui 里的 windowType name
  close_control = leave_chart ─────────────→ .gui 里的关闭按钮 name
  leader_region.control = captain ─────────→ .gui 里的领袖盾徽按钮 name
  members_region.control = port ───────────→ .gui 里的 port_00（前缀 + 两位编号）
  special_members_region.control = chair ──→ .gui 里的 chair_00、chair_01

本地化 harbor_chart_l_english.yml
  harbor_title、harbor_seats、harbor_tip ──→ 类型里引用的本地化键
```

规律：布局里写**前缀**，GUI 里的控件名是**前缀 + 下划线 + 两位编号**，例如 `chair` 对应 `chair_00`、`chair_01`。

### 5.2 改造步骤

1. 把 `harbor_moot` 文件夹和旁边的 `harbor_moot.mod` 一起复制到 `mod` 文件夹。
2. **先不改任何内容**，进游戏或用编辑器预检，确认原样能跑。
3. 把类型 ID、实例 ID、窗口名换成你自己的名字。每换一处，就按 5.1 把对应的另一头也换掉；外层 `.mod` 里的 `name` 和 `path` 也要改。
4. 改控件位置时，只改 `.gui` 里的 `position` 和 `size`，不要改控件类型和名字。
5. 滚动条内部的子控件名（`membership_thumb`、`offices_up` 等）照抄示例，不要改。
6. 示例里的 `ps_instance_selector.gui`、`great_khan_history.gui` 和 `ps_interface_l_english.yml` 是框架共享的支持文件（实例选择窗、投票窗等），保留原样。

---

## 6. Beta 0.1 的限制（必读）

| 限制 | 说明 |
| --- | --- |
| 原版脚本块只经过预检 | 离线预检只能检查结构、花括号和行号，看不出原版语句写得对不对、运行结果如何。游戏读取时的报错会写进日志，并指出你的文件和行号 |
| 原版神罗的投票流程未移植 | 原版事变用 `event`、`default_option` 和编号权重组织投票，这套流程还不能用在自定义体系上。自定义事变用框架的选项写法，可在选项效果里调用 `country_event` |
| AI 只读权重 | `ai_will_do` 只影响权重，AI 什么时候尝试、在哪些按钮里挑，由框架决定 |
| 通知 | 只发给本地玩家；弹窗在战役中的显示效果未实测；另一个替换 `interface/messagetypes.txt` 的 MOD 会让通知失效，需要手动合并注册项 |
| 部分功能未经实战验证 | 自定义修正键（`common/political_modifiers/`）、提示文字的动态参数、面板背景覆盖；原版悬停框是否逐条列出自定义修正的来源也待核对 |
| 尚未开放 | 进攻义务（`offensive` 只能为 `no`）、外部保护（`protection` 只能为 `no`）、重新加入冷却（`rejoin_cooldown_days` 只能为 0） |
| 按钮位置固定 | 行动按钮只能放进 `actions_region` |
| 只有一个 default | 同一套启用的内容里，只能有一个名为 `default` 的类型 |
| 文本 | 示例内容以英文为主 |

## 7. 常见错误

| 现象 | 原因与解决 |
| --- | --- |
| 面板上显示代码名（如 `coastal_title`） | 本地化没加载：文件不是 UTF-8 带 BOM；文件名不是 `_l_english.yml` 结尾；第一行不是 `l_english:`；键名大小写不一致 |
| 继承后加载报错 | `copy_from` 不在第一行；同一个类型里写了两次 `interface`；换了成员来源却没清掉父级的字段（如 `subject_type = clear`）；删了父级资源却没改引用它的费用 |
| 原版条件或效果不生效 | 查 `anvil\anvil.log` 和游戏日志，报错会指出文件和行号。再对照 4.8 节确认 `ROOT`、`FROM` 是不是你以为的那个国家 |
| 图标完全不出现 | MOD 没启用；Beta 没一起启用；开局日期早于历史日期块；`visible` 条件不满足；图标没有在入口区域登记 |
| 图标置灰点不开 | `enabled` 条件不满足；当前国家没有查看资格（`view_trigger`） |
| 点图标后没有面板或闪退 | GUI 的 `tooltip` 字段填了本地化键。`tooltip` 应指向原生文本样式；悬停文字的键要写在 `tooltipText` 里，见下方示例 |
| 通知不出现 | 当前国家不是收件人；`messages` 里拼错了字段或正文超过 4 段（预检会报出文件和行号）；`messagetypes.txt` 被别的 MOD 覆盖 |
| 盾徽悬停显示的国家不对 | 控件编号没有对上前缀，例如布局写 `chair`，GUI 却写成 `chair_1` |
| 邀请功能报错 | 成员来源不是 `explicit` 或 `mixed` |
| 在原版事件里找不到体系 | 原版事件里不能用 `INSTANCE`，要写实例 ID，或只有一个活动实例时写 `type` |
| 两个体系互相覆盖 | 类型 ID、实例 ID、窗口名或 sprite 名重复了。所有 ID 都要全局唯一，建议加上自己的前缀 |

正确的 GUI 悬停写法：

```text
guiButtonType = {
    name = "inspect_seat"
    position = { x = 240 y = 180 }
    tooltipText = "coastal_seat_tip"    # 悬停文字的本地化键写在这里
    tooltip = ""                        # 这一项保持原样，不要填本地化键
}
```

遇到问题时，先看游戏目录里的 `anvil\anvil.log`，框架会在这里写下加载和报错信息；闪退时再看 `anvil\crashes\`。

## 8. 用编辑器省事

编辑器是一个 Windows 程序，在游戏外运行，不会改动游戏文件：

1. 解压编辑器压缩包，双击 `EU4PoliticalEditor.exe`。
2. 点「打开 Mod…」，选择含 `descriptor.mod` 的文件夹。想先练习，可以点「打开幕府练习」。
3. 点「路径…」，指定游戏目录。如果你的 MOD 继承了 Beta 的预设，把 Beta 的文件夹加为依赖。
4. 在画布上拖动控件；按 `F5` 进入离线试玩，看提示和点击效果。
5. 点「预检与导出…」检查错误，导出到一个**新的空文件夹**。交给游戏读取的原版块，预检会标出行号并提示“需进游戏验证”。

离线试玩只模拟界面，实际效果仍要进游戏验证。完整说明见 [编辑器手册](Editor-Manual.zh-CN.md)。

---

有问题或改进，欢迎在 GitHub 提交 Issue 或 Pull Request，见 [CONTRIBUTING.md](../CONTRIBUTING.md)。

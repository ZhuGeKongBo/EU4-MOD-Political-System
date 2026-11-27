# Modder tutorial (Beta 0.1 · framework 0.9.2)

[简体中文](Modder-Tutorial.zh-CN.md)

For mod authors who can edit EU4 script (events, decisions, missions) but do not write C/C++. You only edit text files; nothing needs to be compiled.

> [!WARNING]
> In Beta 0.1, the Papacy and Native Federation presets and systems built from scratch have **not been tested** for reliability, and the Ashikaga Shogunate and Mongol Empire examples still have **many problems** with their texts and interfaces. The vanilla trigger/effect support and notice popups added in 0.9.2 have only passed preflight outside the game. Test every change in game.

## Contents

0. [Before you start](#0-before-you-start)
1. [Four concepts](#1-four-concepts)
2. [Where files go](#2-where-files-go)
3. [Tutorial 1: inherit a preset to make your first system](#3-tutorial-1-inherit-a-preset-to-make-your-first-system)
4. [Common recipes](#4-common-recipes)
5. [Tutorial 2: build from scratch with your own window](#5-tutorial-2-build-from-scratch-with-your-own-window)
6. [Beta 0.1 limitations](#6-beta-01-limitations-read-this)
7. [Common errors](#7-common-errors)
8. [Let the editor do the work](#8-let-the-editor-do-the-work)

---

## 0. Before you start

- Anvil and framework plugin 0.9.2 are installed as in the [installation guide](Install-Guide.en.md), and you can see the Shogunate or Mongol icon in game.
- A text editor that lets you choose the encoding, such as VS Code or Notepad++. Localisation files must be saved as **UTF-8 with BOM**.
- Optional: the editor `EU4PoliticalEditor.exe` 1.0.11, which previews panels and checks for errors (section 8).

## 1. Four concepts

| Name | What it is | Think of it as |
| --- | --- | --- |
| **Type** | A set of rules: who the members are, which offices, resources and buttons exist, which panel is used | The Holy Roman Empire as an institution |
| **Instance** | One concrete organisation built from a type, written in a history file | The particular HRE that exists in 1444 |
| **Layout** | Which GUI window the panel uses and what each control shows | What the HRE window looks like |
| **Entry** | The main-interface icon that opens the panel | The HRE icon beside the minimap |

Keep in mind:

- One type can have several instances, for example two unrelated federations, and they do not interfere with each other. If more than one instance is available when you click an entry, a selection window appears.
- A **preset** is a type with `abstract = yes`. It exists only to be inherited and never appears in game on its own. The Beta 0.1 Workshop content has two presets: `hre_style` and `celestial_style`.
- A child type inherits a preset with `copy_from = preset_name`. Fields you leave out come from the parent; fields you write replace the parent's.
- Vanilla conditions such as `is_emperor` and `is_emperor_of_china` still refer only to the original HRE and Empire of China. For your own systems, use the framework's `political_*` statements (section 4.8).

## 2. Where files go

Paths are relative to your mod's root folder:

| Path | Contents | Required? |
| --- | --- | --- |
| `descriptor.mod` | Mod name, version, supported game version | Yes |
| `common/political_systems/types/*.txt` | Type rules | Yes |
| `history/political_systems/*.txt` | Instances: leader, members, offices, start date | Yes |
| `localisation/*_l_english.yml` | Text players see | Yes |
| `common/political_systems/localisation/*.txt` | Dynamic parameters used in tooltip text | Optional |
| `common/political_system_layouts/*.txt` | Layouts and entry regions | Only for your own window |
| `interface/*.gui`, `interface/*.gfx` | Windows, controls, icons | Only for your own window |

When you inherit a preset, the layout and GUI come from Beta, so you only need the first four kinds of file.

---

## 3. Tutorial 1: inherit a preset to make your first system

**Goal:** a "Coastal Charter" led by France that uses the Empire-of-China-style panel.

This is `examples/ReuseTemplate/coastal_charter/` in the repository. This section builds the smallest working version; section 4 then adds the example's buttons and notices step by step.

### 3.1 Create the mod folder

Put these files in the `mod` folder of your **Documents folder**:

```text
mod\
├─ coastal_charter.mod                ← outer descriptor, read by the launcher
└─ coastal_charter\
   ├─ descriptor.mod                  ← inner descriptor
   ├─ common\political_systems\types\coastal_charter.txt
   ├─ common\political_systems\localisation\coastal_terms.txt
   ├─ history\political_systems\coastal_charter.txt
   └─ localisation\coastal_charter_l_english.yml
```

> The repository example already includes the outer `coastal_charter.mod`. If you copy it, copy the folder and the `.mod` next to it **together**.

Contents of the outer `coastal_charter.mod`:

```text
name = "Coastal Charter - Framework Beta Example"
version = "0.1"
supported_version = "1.37.5"
tags = { "Gameplay" }
path = "mod/coastal_charter"
```

The inner `descriptor.mod` has the same contents **without the last `path` line**.

### 3.2 Write the type

`common/political_systems/types/coastal_charter.txt`:

```text
coastal_charter = {                       # Type ID: globally unique, lowercase letters and underscores
    copy_from = celestial_style           # Inherit the Empire-of-China-style preset. Must be the first line
    title = coastal_title                 # Type name (localisation key)
    members = {
        source = explicit                 # Members = the list written in the history file
        subject_type = clear              # Remove the parent's tributary setting
        title = coastal_members           # Heading of the members area in the panel
    }
    resources = {
        mandate = { tooltip = coastal_reserve_tip }   # The parent's own resource; only its tooltip changes here
    }
    interface = {
        title_key = coastal_title         # Panel title
        entry_button = {
            icon = GFX_pf075_expanded_icon    # Entry icon (cavalry, already registered by Beta)
            order = 30                        # Order within the region; lower numbers come first
            tooltip = coastal_open            # Hover text
        }
    }
}
```

Everything you did not write, including the leader, reforms, decrees, panel layout and entry position, comes from `celestial_style`.

Why `subject_type = clear`: the Empire-of-China-style preset takes its members from the leader's tributaries, with `subject_type = tributary_state`. Since this type switches to a written list (`explicit`), the parent field that no longer applies has to be cleared, otherwise loading fails.

### 3.3 Write the instance (history)

`history/political_systems/coastal_charter.txt`:

```text
coastal_charter_1444 = {          # Instance ID: globally unique
    type = coastal_charter        # Which type to use
    1444.1.1 = {                  # Takes effect from this date; use a date before the start date
        leader = FRA              # Leading country
        members = { FRA SAV }     # Member list (because source = explicit)
    }
}
```

### 3.4 Write the localisation

`localisation/coastal_charter_l_english.yml`, **saved as UTF-8 with BOM**, with `l_english:` as the first line:

```yaml
l_english:
 coastal_title:0 "Coastal Charter"
 coastal_members:0 "Charter Members"
 coastal_open:0 "Open the Coastal Charter"
 coastal_reserve_tip:0 "$POLITY$: $BALANCE|2$ / $LIMIT|2$\n$DETAIL$"
```

The last line uses `$parameters$`. Declare what each one stands for in `common/political_systems/localisation/coastal_terms.txt`:

```text
coastal_reserve_tip = {
    parameters = { POLITY = system BALANCE = value LIMIT = maximum DETAIL = breakdown }
}
```

| Parameter | Value |
| --- | --- |
| `system` | Name of the current political system |
| `value` | Current value of the resource |
| `maximum` | Maximum of the resource |
| `breakdown` | Sources and calculation |

The `|2` in `$BALANCE|2$` shows two decimal places. If you rename a parameter, change it in both the text and the declaration.

### 3.5 Test in game

1. In your launcher playset, **enable both** Political Framework - Beta 0.1 and your mod. Your mod uses Beta's preset, layout and icon, so it cannot work without Beta.
2. Start a new 1444 game as France.
3. A cavalry icon should appear beside the minimap, with the tooltip "Open the Coastal Charter". Open the panel and check the leader and members.

If it does not appear, see [section 7, Common errors](#7-common-errors).

---

## 4. Common recipes

> **From 0.9.2, condition and effect blocks accept vanilla script.** The conditions and effects of reforms, decisions, decrees and incidents, and condition blocks such as entries, member filters and office eligibility, are read by the game's own parser. The triggers and effects you would write in vanilla decisions and events, your own scripted triggers and scripted effects, and scope switches such as `capital_scope = { ... }` mostly work as written. For what `ROOT` and `FROM` mean in each block, see [section 4.8](#48-read-and-change-systems-from-vanilla-events-and-what-root--from-mean).

All snippets below can be pasted into the files from section 3 and edited. The full field reference is [`examples/DefaultType/Default-Fields.zh-CN.txt`](../examples/DefaultType/Default-Fields.zh-CN.txt) (comments in Chinese).

### 4.1 Change the leader and members, and record later changes

A history file can have several date blocks. Later dates record appointments, joining and leaving:

```text
coastal_charter_1444 = {
    type = coastal_charter
    1444.1.1 = { leader = FRA members = { FRA SAV } }
    1460.1.1 = { add_members = { POR } }        # Portugal joins in 1460
    1480.1.1 = { remove_members = { SAV } }     # Savoy leaves in 1480
}
```

Fields allowed in a date block: `leader`, `members`, `add_members`, `remove_members`, `roles`, `resources`, `territory`, `add_provinces`, `remove_provinces`.

After editing history, start a **new game** to see the result. Existing saves restore the state recorded in the save.

### 4.2 Where members come from (five sources)

```text
# ① Written list: members = { ... } in the history file
members = { source = explicit }

# ② The leader's subjects (used by the Shogunate example); subject_type is an ID from vanilla subject_types
members = { source = leader_subjects subject_type = daimyo_vassal }

# ③ Query (used by the Mongol example): every country matching the condition is a member
members = { source = query source_trigger = { religion = tengri_pagan_reformed } }

# ④ Countries whose capital is in the system's territory (used by the HRE-style preset)
members = { source = capital_territory }

# ⑤ Mixed: a query, plus additions and removals from the history list
members = { source = mixed source_trigger = { religion = catholic is_subject = no } }
```

`source_trigger` accepts vanilla conditions. `ROOT` is the country being checked and `FROM` is the system's leader.

### 4.3 Add an office

Copy the Shogunate example's Kanrei. Offices go in the type's `special_members`:

```text
special_members = {
    admiral = {                          # Office ID
        acquisition = appointed          # Appointed
        max_holders = 3                  # Up to 3 seats (1 to 32)
        require_member = yes             # Holders must be members
        can_hold_with_leader = no        # The leader cannot hold it as well
        candidate_trigger = { num_of_ports = 1 }   # Vanilla condition: at least one port
        retain_trigger = { always = yes }
        title_key = coastal_admiral      # Office name (localisation key)
        tooltip = coastal_admiral_tip    # Hover text
        # history_key = admiral          # Optional: take holders from a yes/no field in vanilla country history; usually not needed
    }
}
```

Appoint holders by **office ID** in a date block of the system's history file:

```text
1444.1.1 = {
    leader = FRA
    members = { FRA SAV POR }
    roles = { admiral = { SAV = yes POR = yes } }
}
```

To remove someone later, write `roles = { admiral = { SAV = no } }` in a later date block.

Remember to add `coastal_admiral` and `coastal_admiral_tip` to your localisation file.

### 4.4 Add a resource, or make it grow every month

Copy the Shogunate example's Shogunal Authority:

```text
resources = {
    mandate = clear                      # Remove the parent's Mandate (leave this line out to keep it)
    harbor_dues = {                      # Resource ID
        initial = 55                     # Starting value
        precision = 2                    # Decimal places (0 to 3)
        name_key = coastal_dues          # Resource name in the panel
        tooltip = coastal_dues_tip       # Hover text
        monthly_growth = {
            value = 1                    # +1 per month
        }
    }
}
```

The preset's `charter` reform pays with `mandate`. If you remove `mandate`, change that reform's cost to your new resource, otherwise loading fails. The Shogunate example does exactly this:

```text
reforms = {
    charter = {
        cost = { resource = harbor_dues payer = instance amount = 20 }
    }
}
```

For monthly growth that depends on conditions or reads vanilla modifiers, see `growth_modifier` and the conditional terms under `monthly_growth` in the field reference.

### 4.5 Add buttons: decisions, reforms, decrees

All three appear in the panel's action area (`ui = { region = actions_region }`):

| Kind | Field | Behaviour |
| --- | --- | --- |
| Decision | `decisions` | Can be made repeatable |
| Reform | `reforms` | Takes effect once passed; can have prerequisites, exclusions and continuous modifiers |
| Decree | `decrees` | Lasts for a set time and then ends; can have continuous modifiers |

**Decision** (the "Port Festival" from the Coastal Charter example):

```text
decisions = {
    port_festival = {
        name = coastal_festival                  # Button text
        tooltip = coastal_festival_tip           # Hover text
        actor_rule = leader                      # Only the leader can use it
        trigger = { is_at_war = no }             # Vanilla condition: only at peace
        effect = {                               # Vanilla effects
            add_treasury = 15
            capital_scope = { set_province_flag = coastal_port_festival }
        }
        cooldown_days = 365                      # 365-day cooldown
        repeatable = yes
        ui = { region = actions_region }
    }
}
```

**Reform**: `on_effect` runs once when it passes, `off_effect` once when it is revoked.

```text
reforms = {
    harbor_law = {
        name = coastal_harbor_law
        actor_rule = leader
        cost = { resource = mandate payer = instance amount = 20 }
        on_effect = { add_prestige = 5 }         # Runs when passed
        off_effect = { add_prestige = -5 }       # Runs when revoked
        member = { global_trade_power = 0.1 }    # Continuous modifier for every member once passed
        ui = { region = actions_region }
    }
}
```

**Decree**: `effect` runs when it is issued, `removed_effect` when it expires.

```text
decrees = {
    open_ports = {
        name = coastal_open_ports
        actor_rule = leader
        term = fixed
        duration_days = 180                      # Lasts 180 days
        effect = { add_mercantilism = 1 }        # Runs when issued
        removed_effect = { add_prestige = 1 }    # Runs when it expires
        emperor = { trade_efficiency = 0.05 }    # Modifier for the leader while active
        ui = { region = actions_region }
    }
}
```

The four kinds of continuous modifier for reforms and decrees:

| Field | Applies to |
| --- | --- |
| `modifier` | The beneficiaries set by `beneficiaries` |
| `emperor` | The current leader |
| `member` | All current members |
| `province` | Provinces in the system's territory (declare the territory in history or script) |

`actor_rule` can be `any`, `leader`, `member`, or `permission` (holders of a given permission, used together with `actor_permission`).

To let the AI use the button, add a weight in vanilla style:

```text
ai_will_do = { factor = 1 modifier = { factor = 1.25 is_at_war = no } }
```

`ai_will_do` only sets the weight. When the AI tries an action and which buttons it picks from are still decided by the framework's rules.

### 4.6 Change the entry: when it shows and when it can be used

```text
interface = {
    entry_button = {
        visible = { capital_scope = { continent = europe } }   # Only countries with a European capital see the icon
        enabled = { is_at_war = no }                           # Can only be opened at peace
        tooltip = coastal_open                                 # Normal hover text
        disabled_tooltip = coastal_locked                      # Hover text when greyed out
        toggle = yes                                           # Clicking the icon again closes the panel
    }
}
```

Here `ROOT` is the country the player controls and `FROM` is the system's leader.

**Available icons:** Beta 0.1 registers `GFX_pf075_hre_icon` (HRE), `GFX_pf075_celestial_icon` (Mandate) and `GFX_pf075_expanded_icon` (cavalry). For a new icon, use the editor's add-component wizard and choose **Entry button**; it registers the icon for you. Registering it by hand means editing Beta's interface files and easily conflicts with Beta.

### 4.7 Send notices: popups or message-log entries (new in 0.9.2)

This is the "Send Council Bulletin" button from the Coastal Charter example. It takes two steps.

**Step 1: define what the notice looks like, in the type**

```text
messages = {
    council_notice = {                                   # Notice ID; name it yourself
        title = coastal_notice_title                     # Popup title
        subtitle = coastal_notice_subtitle               # Banner text (optional)
        body = { coastal_notice_body coastal_notice_balance }   # Body, 1 to 4 paragraphs
        log = coastal_notice_log                         # Message-log text (optional; defaults to the first paragraph)
        options = {
            close = coastal_notice_read                  # Main button
            secondary = coastal_notice_file              # Second button (optional)
        }
    }
    council_record = {
        title = coastal_record_title
        body = coastal_record_body
        popup = no                                       # Log entry only, no popup
    }
}
```

**Step 2: send it from a button's effect**

```text
decisions = {
    send_council_notice = {
        name = coastal_notice_button
        tooltip = coastal_notice_tip
        actor_rule = leader
        repeatable = yes
        trigger = { ai = no }                            # Players only
        effect = {
            every_country = {                            # Vanilla script picks the recipients
                limit = { OR = { tag = FRA tag = SAV } }
                political_send_message = {
                    instance = INSTANCE                  # The current system
                    message = council_notice
                    actor = ROOT                         # Sender: the country that clicked
                }
            }
            political_send_message = {                   # Also log a record for yourself
                instance = INSTANCE
                message = council_record
                target = ROOT
            }
        }
        ui = { region = actions_region }
    }
}
```

Without `target`, the recipient is the country in the current scope, so inside `every_country` the notice goes to each matching country in turn.

In localisation, `$SYSTEM$` (system name), `$SENDER$` (sending country) and `$RECIPIENT$` (receiving country) are filled in by the framework. Your own values must be declared in the parameter file. For example, to show the common reserve in the body:

```yaml
 coastal_notice_body:0 "$SENDER$ sends greetings to $RECIPIENT$."
 coastal_notice_balance:0 "The common reserve is $RESERVE|2Y$."
```

```text
coastal_notice_balance = {
    parameters = { RESERVE = { source = balance id = mandate } }
}
```

Notices go to the local player only (single-player). How popups look in a campaign has not been tested yet.

### 4.8 Read and change systems from vanilla events, and what ROOT / FROM mean

The framework adds `political_*` statements to vanilla script. You can use them in the trigger and effect blocks of vanilla events and decisions:

```text
# Triggers
political_is_member = { instance = coastal_charter_1444 country = SAV }
political_has_role = { instance = mongol_system role = tumen country = OIR }

# Effects
political_change_resource = { instance = mongol_system resource = mongol_resource amount = 5 }
```

How to write `instance`:

- Inside blocks the framework runs itself (button effects, entry conditions and so on), `instance = INSTANCE` means "this system".
- In vanilla events and decisions, write the **instance ID** (the outermost name in the history file, such as `coastal_charter_1444`), or `type = type_id` if that type has exactly one active instance.
- Country parameters such as `country` take a country tag or `ROOT`, `THIS`, `FROM`, `PREV`.

Common statements (all statements and parameters are under `operations` in [`anvil/api_manifest.json`](../anvil/api_manifest.json)):

| Statement | Kind | Purpose |
| --- | --- | --- |
| `political_is_member` | Trigger | Is the country a member? |
| `political_is_leader` | Trigger | Is the country the leader? |
| `political_has_role` | Trigger | Does the country hold an office? |
| `political_compare_resource` | Trigger | Compare a resource value |
| `political_member_count` | Trigger | Compare the number of members |
| `political_has_reform` | Trigger | Has a reform been passed? |
| `political_change_resource` | Effect | Add to or subtract from a resource |
| `political_set_member` | Effect | Add or remove a member (member source must be `explicit` or `mixed`) |
| `political_set_role` | Effect | Appoint to or remove from an office |
| `political_set_leader` | Effect | Change the leader |
| `political_send_message` | Effect | Send a notice (section 4.7) |

**What ROOT and FROM mean in each block** (the full reference, [Native-Scripting.zh-CN.md](Native-Scripting.zh-CN.md), is in Chinese):

| Block | `ROOT` | `FROM` |
| --- | --- | --- |
| Panel access, entry `visible` / `enabled` | The player's current country | The system's leader |
| Member filters, join, leave and retain conditions | The country being checked | The system's leader |
| Office and leader eligibility | The country being checked | The system's leader |
| A button's `potential`, `trigger`, `actor_trigger`, `ai_will_do` | The country clicking the button | The target (if any) |
| A button's `target_trigger` | The target | The country clicking the button |
| A button's effects | The beneficiary chosen by `beneficiaries` | The target or the acting country |
| Effects after an invitation is accepted | The invited country, already registered as a member | The inviting country |
| Defensive obligations | The member being attacked | The attacker |

---

## 5. Tutorial 2: build from scratch with your own window

> [!CAUTION]
> In Beta 0.1, the reliability of systems built from scratch has not been tested.

`examples/FromScratch/harbor_moot/` in the repository is a complete standalone example with its own type, instance, 518×438 window, controls and entry. It does not depend on Beta's presets, and its outer `harbor_moot.mod` is included next to it.

### 5.1 How the six files connect

The hard part is that names must match exactly. In each line below, the name on the left must be identical to the name in the file on the right:

```text
Type harbor_moot.txt
  interface.layout = harbor_chart ─────────→ harbor_chart = { ... } in the layout file
  entry_button.region = port_buttons ──────→ hud_regions = { port_buttons = ... } in the layout file
  entry_button.icon = GFX_harbor_entry ────→ spriteType name in the .gfx

History western_harbor.txt
  type = harbor_moot ──────────────────────→ the type ID

Layout harbor_chart.txt
  window = western_chart ──────────────────→ windowType name in the .gui
  close_control = leave_chart ─────────────→ close button name in the .gui
  leader_region.control = captain ─────────→ leader shield button name in the .gui
  members_region.control = port ───────────→ port_00 in the .gui (prefix + two-digit number)
  special_members_region.control = chair ──→ chair_00 and chair_01 in the .gui

Localisation harbor_chart_l_english.yml
  harbor_title, harbor_seats, harbor_tip ──→ localisation keys used by the type
```

The rule: the layout names a **prefix**, and the GUI control names are **prefix + underscore + two-digit number**. For example, `chair` matches `chair_00` and `chair_01`.

### 5.2 Steps

1. Copy the `harbor_moot` folder and the `harbor_moot.mod` next to it into your `mod` folder.
2. **Change nothing yet.** Run it in game or preflight it in the editor to make sure it works as is.
3. Replace the type ID, instance ID and window name with your own. Each time you rename one end, rename the matching end from 5.1 as well; also change `name` and `path` in the outer `.mod`.
4. To move controls, change only `position` and `size` in the `.gui`, not the control types or names.
5. Keep the scrollbar child names (`membership_thumb`, `offices_up` and so on) exactly as in the example.
6. `ps_instance_selector.gui`, `great_khan_history.gui` and `ps_interface_l_english.yml` in the example are shared framework support files (the instance selector, voting window and so on). Leave them unchanged.

---

## 6. Beta 0.1 limitations (read this)

| Limitation | Details |
| --- | --- |
| Vanilla script blocks only pass preflight | Offline preflight checks structure, braces and line numbers, but cannot tell whether a vanilla statement is correct or how it behaves. Errors found when the game reads your files are logged with your file and line number |
| The vanilla HRE voting flow is not ported | Vanilla incidents use `event`, `default_option` and numbered weights for voting; this flow does not work for custom systems yet. Custom incidents use the framework's option format, and option effects can call `country_event` |
| AI only reads weights | `ai_will_do` only affects the weight; when the AI tries and which buttons it picks are decided by the framework |
| Notices | Local player only; popups not yet tested in a campaign; another mod replacing `interface/messagetypes.txt` breaks notices unless the entries are merged by hand |
| Some features not tested in campaigns | Custom modifier keys (`common/political_modifiers/`), dynamic tooltip parameters, panel background overrides; whether the vanilla tooltip lists the sources of custom modifiers is also still to be checked |
| Not available yet | Offensive obligations (`offensive` must be `no`), external protection (`protection` must be `no`), rejoin cooldown (`rejoin_cooldown_days` must be 0) |
| Fixed button area | Action buttons can only go in `actions_region` |
| One default only | Only one type named `default` may exist in the enabled content |
| Text | Example content is mostly in English |

## 7. Common errors

| Problem | Cause and fix |
| --- | --- |
| The panel shows code names (such as `coastal_title`) | Localisation did not load: the file is not UTF-8 with BOM, the name does not end in `_l_english.yml`, the first line is not `l_english:`, or the key's capitalisation differs |
| Loading fails after inheriting | `copy_from` is not the first line; `interface` is written twice in one type; you changed the member source without clearing parent fields (such as `subject_type = clear`); or you removed a parent resource without changing the costs that use it |
| A vanilla condition or effect does nothing | Check `anvil\anvil.log` and the game logs; errors point to your file and line. Then check section 4.8 to confirm `ROOT` and `FROM` are the countries you expect |
| The icon does not appear at all | The mod is not enabled; Beta is not enabled with it; the game starts before the history date block; the `visible` condition is not met; or the icon is not registered in the entry region |
| The icon is greyed out | The `enabled` condition is not met, or the current country cannot view the system (`view_trigger`) |
| No panel after clicking, or a crash | A localisation key was put in a GUI `tooltip` field. `tooltip` must point to a native text style; put the hover text key in `tooltipText` (see below) |
| No notice appears | The current country is not a recipient; a field in `messages` is misspelled or the body has more than 4 paragraphs (preflight reports the file and line); or another mod overwrote `messagetypes.txt` |
| A shield's hover shows the wrong country | Control numbers do not match the prefix, for example the layout says `chair` but the GUI says `chair_1` |
| The invitation fails | The member source is not `explicit` or `mixed` |
| A vanilla event cannot find the system | `INSTANCE` does not work in vanilla events; write the instance ID, or `type` if there is only one active instance |
| Two systems overwrite each other | A type ID, instance ID, window name or sprite name is duplicated. All IDs must be globally unique; adding your own prefix helps |

Correct GUI hover setup:

```text
guiButtonType = {
    name = "inspect_seat"
    position = { x = 240 y = 180 }
    tooltipText = "coastal_seat_tip"    # The hover text's localisation key goes here
    tooltip = ""                        # Leave this as it is; do not put a localisation key here
}
```

When something goes wrong, check `anvil\anvil.log` in the game folder first; the framework writes loading messages and errors there. After a crash, also check `anvil\crashes\`.

## 8. Let the editor do the work

The editor is a Windows program that runs outside the game and does not change game files:

1. Unzip the editor package and double-click `EU4PoliticalEditor.exe`.
2. Click **Open Mod** and choose the folder that contains `descriptor.mod`. To practise first, click **Open Shogunate practice**.
3. Click **Paths** and set the game folder. If your mod inherits Beta's presets, add Beta's folder as a dependency.
4. Drag controls on the canvas. Press `F5` for offline play mode to check hovers and clicks.
5. Click **Preflight and export** to check for errors, and export into a **new, empty folder**. For vanilla blocks handed to the game, preflight marks the line numbers and notes that they need checking in game.

Offline play only simulates the interface; real effects still have to be checked in game. See the [editor manual](Editor-Manual.en.md).

---

Questions or improvements are welcome as GitHub issues or pull requests; see [CONTRIBUTING.md](../CONTRIBUTING.md).

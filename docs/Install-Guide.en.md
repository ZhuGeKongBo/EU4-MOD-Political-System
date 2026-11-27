# Installation guide (Beta 0.1)

[简体中文](Install-Guide.zh-CN.md)

**Public Beta 0.1 · Framework 0.9.2 · Editor 1.0.11**

> **Supported setup:** Windows, Steam EU4 **1.37.5**, ordinary single-player. Other versions, other stores, Ironman and multiplayer are untested.
>
> Follow steps 1 to 5 in order. Each step ends with a "Check" so you know it worked before moving on.

## What to download

| # | Download | Where | What it does | Required? |
| --- | --- | --- | --- | --- |
| ① | Anvil 1.24.0 (Windows x64) | [Anvil upstream release](https://github.com/happy-dwarf-team/anvil/releases/tag/v1.24.0) | The loader that lets the game load plugins | Yes |
| ② | `Political-Framework-Runtime-Beta-0.1.zip` | This project's [Releases](https://github.com/<GitHub用户名>/<仓库名>/releases) | The framework plugin 0.9.2 (DLL) | Yes |
| ③ | Political Framework - Beta 0.1 | Steam Workshop | The Shogunate and Mongol example content | To see the examples |
| ④ | `EU4-Political-Editor-Beta-0.1.zip` | This project's [Releases](https://github.com/<GitHub用户名>/<仓库名>/releases) | Visual editor 1.0.11 | Only for making mods |

## Two folders you need to know

These are easy to mix up, and both come up again later:

| Name | Where | How to open it | What goes there |
| --- | --- | --- | --- |
| **Game folder** | The folder that contains `eu4.exe` | Steam library → right-click Europa Universalis IV → Manage → Browse local files | Anvil and the framework plugin |
| **Documents folder** | `Documents\Paradox Interactive\Europa Universalis IV\` | Open "Documents" in File Explorer | `mod`, `save games`, `logs`; manual mod installs go here |

If your Documents folder is redirected to another drive, use the folder the game actually uses. See [Paradox's file locations article](https://support.paradoxplaza.com/hc/en-us/articles/203089358-Europa-Universalis-IV-file-locations).

## Step 1: Back up (important if you use a Chinese patch)

Open the **game folder** and look for `version.dll`.

- **Not there:** go to step 2.
- **There:** it most likely comes from the double-byte patch (EU4dll) or the UTF-8 Chinese patch. Anvil also uses `version.dll`, so installing Anvil overwrites it and the patch stops working; reinstalling the patch breaks Anvil in the same way. **A way to run both has not been verified.**

  Copy the existing `version.dll` somewhere outside the game folder first, then decide whether to continue.

It is also a good idea to back up important saves from `Documents\...\save games`.

## Step 2: Install Anvil

1. Close the game completely.
2. Open the [Anvil 1.24.0 release page](https://github.com/happy-dwarf-team/anvil/releases/tag/v1.24.0) and download the Windows x64 package.
3. Install it into the **game folder** (the one with `eu4.exe`), following the upstream instructions.

**Check:** the game folder now contains `version.dll` and an `anvil\plugins\anvil-game\` folder.

## Step 3: Add the framework plugin

1. Unzip `Political-Framework-Runtime-Beta-0.1.zip`.
2. Find the `anvil\plugins\great-khan-native\` folder inside it.
3. Copy the **whole** `great-khan-native` folder into `anvil\plugins\` in your game folder.

Afterwards your game folder should look like this:

```text
Europa Universalis IV\          ← the folder with eu4.exe
├─ eu4.exe
├─ version.dll                  ← from Anvil
└─ anvil\
   └─ plugins\
      ├─ anvil-game\            ← from Anvil
      └─ great-khan-native\     ← this project's plugin
         ├─ great_khan_native.dll
         ├─ plugin.toml
         ├─ symbols.toml
         └─ symbols.lock
```

**Check:** open `plugin.toml` in Notepad and find `version = "0.9.2"`.

> - These four files belong together and must come from the same version. When upgrading, replace all four at once.
> - When upgrading from an older build, move the old plugin folder **outside** `anvil\plugins` as a backup; do not leave it inside.
> - If you ever installed the test plugin `great-khan-history`, move it out of `anvil\plugins` too. It conflicts with this plugin and cannot be enabled at the same time.

## Step 4: Enable the example content (choose one)

### Option A: subscribe on the Workshop (recommended)

1. Subscribe to **Political Framework - Beta 0.1**.
2. Open the Paradox launcher → Playsets → enable the mod.

### Option B: install manually

1. Copy the `political_framework_beta_01` folder and the `political_framework_beta_01.mod` file next to it into the `mod` folder in your **Documents folder**.
2. Open `political_framework_beta_01.mod` in Notepad and make sure it reads:

   ```text
   name = "Political Framework - Beta 0.1"
   version = "0.1"
   supported_version = "1.37.5"
   tags = { "Gameplay" }
   path = "mod/political_framework_beta_01"
   ```

3. Enable it in a playset in the launcher.

> Do not use A and B at the same time. For your first test, disable old test builds of this mod and other mods that change the interface or `interface/messagetypes.txt`.

## Step 5: Check in game

1. Launch the game from the launcher and start a new game on the default **1444.11.11** start date.
2. Play as **Ashikaga (ASK, Japan)** to see the Ashikaga Shogunate, or start another game as the **Mongols (KHA)** to see the Mongol Empire.
3. An icon should appear beside the minimap. Its tooltip should read **Open the Ashikaga Shogunate** (or **Open the Mongol Empire**). Click it, check the leader and the daimyo shields, then close and reopen the panel.
4. Check the log: open `anvil\anvil.log` in the game folder with Notepad and search for `great-khan-native` to confirm the plugin was loaded.

If all of this works, the installation succeeded. In Beta 0.1 the panels are mainly for inspection; there is no need to keep playing.

### Optional: try the new notice popups

Framework 0.9.2 adds notices, and the Coastal Charter example has a ready-made button. This has not yet been tested in a campaign, so your help checking it is welcome:

1. Copy the `examples/ReuseTemplate/coastal_charter` folder from the source package, together with the `coastal_charter.mod` next to it, into the `mod` folder in your Documents folder.
2. Enable both Political Framework - Beta 0.1 and Coastal Charter in your playset.
3. Start a new 1444 game as **France**, click the cavalry icon beside the minimap, then click **Send Council Bulletin** in the panel.
4. Expected result: a popup appears above the panel; after clicking **Understood** the popup closes and the political panel stays open; the message log gains an entry.

If the result differs, please open an issue.

## Uninstall / roll back

1. Close the game and disable the mod in the launcher.
2. Delete `anvil\plugins\great-khan-native\` from the game folder, or move it out of the game folder.
3. If you no longer want Anvil, uninstall it following the upstream instructions, then put back the `version.dll` you backed up in step 1.

> It has not been confirmed whether saves created with this mod load correctly without it. Keep backups of your saves from before installing.

## Troubleshooting

| Problem | Check first |
| --- | --- |
| No icon in game | ① Is it a new 1444 game as ASK or KHA? ② Is the mod enabled in the launcher? ③ Does `anvil.log` mention `great-khan-native`? ④ Does another mod change the same interface files? For your own systems, also check that the history date is before the start date and that the entry's `visible` condition is met |
| No `anvil.log`, or its time does not update | Anvil is not running. Make sure it is installed in the folder with `eu4.exe`, and check whether a Chinese patch has overwritten `version.dll` again |
| The log says the plugin was skipped | Usually the game is not 1.37.5, the plugin files come from different versions, or the old `great-khan-history` plugin is still present. Put back a complete set of all four files and remove the conflicting plugin |
| The game crashes when clicking the icon | Note the time and check `anvil\crashes\` in the game folder and the first error in `anvil.log`. For your own mod, run the editor's preflight first and fix the earliest GUI or binding error |
| The panel works but no notice appears | Is the current country a recipient? Are these messages turned off in the game's message settings? Has another mod overwritten `interface/messagetypes.txt`? |
| Panel text is in English | Beta 0.1 example text is mostly in English |
| The panel shows code names (such as `shogun_title`) | Localisation did not load. Make sure the mod is enabled; if you edited localisation files yourself, see "Common errors" in the modder tutorial |
| Antivirus flags the DLL | Runtime-injected DLLs are often false positives. Only download from this project's Releases and verify the checksum as below |

## Verify your downloads

In the folder with the ZIP, Shift + right-click → "Open PowerShell window here", then run:

```powershell
Get-FileHash .\Political-Framework-Runtime-Beta-0.1.zip -Algorithm SHA256
```

Compare the output with the value in `SHA256SUMS.txt` on the Releases page. They must match exactly.

## When reporting a problem

Include the EU4, Anvil, framework plugin and editor versions, the shortest steps to reproduce, and a log excerpt with personal paths and user names removed. See [CONTRIBUTING.md](../CONTRIBUTING.md).

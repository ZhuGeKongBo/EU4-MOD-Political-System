# Install Political Framework

**Public Beta 0.1 · Runtime 0.9.2 · Editor 1.0.11**

Use Steam EU4 1.37.5 on Windows x64. Start your first check in a regular non-Ironman single-player campaign at the 1444 bookmark.

## 1. Get these three items

| Download | Where to get it | What it does |
| --- | --- | --- |
| Anvil 1.24.0 for Windows x64 | [Upstream release](https://github.com/happy-dwarf-team/anvil/releases/tag/v1.24.0) | Supplies the loader and `anvil-game` |
| Framework runtime ZIP | The project's GitHub Releases | Supplies the 0.9.2 framework plugin |
| **Political Framework - Beta 0.1** | Steam Workshop or the local content ZIP | Supplies the Shogunate, Mongol Empire, panels, and text |

The editor is optional. To edit a mod, extract `EU4-Political-Editor-Beta-0.1.zip` and open `EU4PoliticalEditor.exe`.

### Find the two folders

- **Game folder.** This contains `eu4.exe`. Right-click EU4 in the Steam library and choose **Manage**, then **Browse local files**. Install Anvil and the plugin here.
- **Player folder.** This is usually **Documents → Paradox Interactive → Europa Universalis IV**. Put local mods in its `mod` directory. If Windows redirects Documents, use the folder the game actually reads. [Paradox's Windows file locations](https://support.paradoxplaza.com/hc/en-us/articles/203089358-Europa-Universalis-IV-file-locations)

## 2. Install Anvil and the framework plugin

1. Exit EU4. Follow the [Anvil 1.24.0 instructions](https://github.com/happy-dwarf-team/anvil/releases/tag/v1.24.0) for the Windows x64 distribution. The game folder should contain upstream `version.dll` and the standard `anvil-game` plugin. If an existing translation tool provides a DLL with the same name, back it up and check its loader compatibility first.
2. Extract the project's **Runtime** ZIP. Copy its full `anvil/plugins/great-khan-native` directory to the matching location in the game folder.
3. Check that the project plugin's `plugin.toml` says `version = "0.9.2"`. Update the DLL, both TOML files, and the lockfile together. Keep old backups outside `anvil/plugins`.

~~~text
Europa Universalis IV/
  eu4.exe                              # The folder you want.
  version.dll                          # From upstream Anvil.
  anvil/
    plugins/
      anvil-game/                      # From upstream Anvil.
      great-khan-native/               # Copy this project plugin here.
        great_khan_native.dll
        plugin.toml
        symbols.toml
        symbols.lock
~~~

## 3. Enable the content and open a panel

- **Workshop.** Subscribe to **Political Framework - Beta 0.1** and enable it in your Paradox Launcher playset.
- **Local ZIP.** Copy both the `political_framework_beta_01` directory and the adjacent `political_framework_beta_01.mod` into your player folder's `mod` directory. The content directory should directly contain `descriptor.mod`. Enable the mod in the launcher.

Pick one installation method. For your first check, turn off older QA mods with matching definitions. Launch normally through Steam and start 1444 as Ashikaga. Look near the minimap for **Open the Ashikaga Shogunate**. Open, close, and reopen the panel, checking the leader and daimyo shields. For the second example, start as Mongolia and look for **Open the Mongol Empire**.

To check the new message path, enable the source package's Coastal Charter example with the Beta content. Play France in 1444, open the cavalry entry, and click **Send Council Bulletin**. Expect a popup above the panel and the panel to stay open after **Understood**. [Validation status](Validation.md)

## 4. Update or troubleshoot

| What you see | Check first |
| --- | --- |
| The Anvil log does not update | Find the game folder through Steam again. Check the upstream loader and `anvil/anvil.log` in that folder. |
| Anvil skips the plugin | Check the EU4 build, the standard plugin, and the four matching project files. Disable a conflicting older plugin such as `great-khan-history`. |
| The launcher lists the mod but you cannot see an entry | Confirm the active playset and test 1444 Ashikaga. For your own system, check its start date, discovered capital, and entry conditions. |
| Clicking an entry crashes | Run editor preflight and fix the first GUI or binding error. Use the log and the crash time to narrow it down. |
| The panel works but a message is missing | Check that the played country is a recipient, the vanilla Message Settings allow it, and the expected `interface/messagetypes.txt` is loaded. |

Back up your saves and plugin before updating or rolling back. Restore the matching content, plugin, and save together. A useful issue report includes the versions, short reproduction steps, and a log excerpt with personal paths removed.

### Open the editor

Run `EU4PoliticalEditor.exe`. **Open Mod** accepts the folder that directly contains `descriptor.mod`. Use **Paths** to choose the game and any dependency folders. Save the project and export the preflighted content to a new location. [Editor manual](Editor-Manual.en.md)


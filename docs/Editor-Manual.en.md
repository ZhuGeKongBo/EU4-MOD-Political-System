# EU4 Political Editor manual

**Beta 0.1 · Editor 1.0.11 · Framework validator 0.9.2**

The Windows editor works offline. A project holds a content snapshot, edits and QA records. EU4 reads the exported mod. See the [modder tutorial](Modder-Tutorial.en.md) for content design and scripts, and the [installation guide](Install-Guide.en.md) for setup.

## First project

1. Extract the editor ZIP and run `EU4PoliticalEditor.exe`. Use **Open project** to load an existing `.pfproject.json`.
2. Use **Open Shogunate practice** to explore the built-in example. To work with Beta content or an existing mod, choose **Open Mod** and select the extracted folder containing `descriptor.mod`.
3. In **Paths**, set the game folder containing `eu4.exe` and the EU4 user folder. Add the extracted folders for content that supplies inherited types, layouts or sprites.
4. Select a concrete system and an instance, choose the preview country and date, and save a separate project file.
5. Open **Preflight and export**, fix the reported errors and write the output into a new folder.

In the dependency list, later files win when paths match. The project draft takes precedence. The game folder supplies remaining stock assets. `replace_path` can hide lower-priority paths. Match these paths and dependencies to the playset you intend to test.

## Toolbar

| Command | Use |
| --- | --- |
| New | Start a project with a type and an instance. Give new IDs and files distinct names. |
| From template | Pick a parent type, layout, new type and instance IDs, date and leader. Inherit the parent or copy its expanded configuration. Check any external sprite dependencies. |
| Open Mod | Read an extracted content folder containing `descriptor.mod` into a snapshot. A downloaded Workshop folder can be a source. |
| Open project | Recover a saved project and its settings. Set paths again if the project moves to another machine or folder. |
| Save project | Save the `.pfproject.json` with `Ctrl+S`. Use the recovery command for an automatic backup. |
| Undo / redo | Use `Ctrl+Z`, `Ctrl+Y` or `Ctrl+Shift+Z`. An accepted multi-file edit can be undone together. |
| Add component | Choose the component's purpose, sprite and position. Review the changes before applying. |
| Offline play | Press `F5` to try tooltips, scrolling, simulated clicks and switching instances. In-game effects, triggers and diplomacy need campaign testing. |
| QA record | Start a record for one campaign test, enter results and attach screenshots or log excerpts. |
| Preflight and export | Check syntax, references, layout and source conflicts, then produce a full or incremental content package. |
| Language | Switch the editor UI between Simplified Chinese and English. The choice is saved. |

## Context and canvas

| Control | Use |
| --- | --- |
| System / instance | Choose the rules and the history instance to preview. A concrete type with a valid layout and history supplies the campaign panel. |
| Country / history date | Inspect the selected date and viewer. These settings feed the offline model. |
| Game text | Pick the preview's localisation language independently of the editor's UI language. |
| Paths / syntax reference | Review content roots or open the annotated examples. Check the runtime API and preflight for supported fields. |
| Resolution / UI scale / canvas | Simulate a resolution or UI scale. Zoom the canvas for editing. The stored coordinates are those of the native window. |
| Hit details | See the selected control's extent, the actual hit and its tooltip. Useful for layered regions. |
| Control list / X and Y / Apply | Select an obscured control from the list and set a precise position, or drag it on the canvas. |
| Edit shared layout | Explicitly edit the layout for all listed users. With the default off, changes can be made for the current system. |
| Switch to manual | Keep the current automatic coordinates in the GUI and release individual slots for editing. To stay in automatic mode, move the frame. |
| Discovered / entry state | Simulate leader-capital discovery and a hidden, disabled or enabled entry. Check scripted access in a real campaign. |
| Arrange election area / Tuck scores by shields | Adjust the election area as a whole, or move only scores and their hover surfaces to the lower right of shields. |
| Move whole seat | Drag a large shield together with its score, hover, small shield and seat label. With the option off, the shield moves independently. |
| Delete component / Bindings and repair | Remove a presentation item, rebind a feature, or restore a missing control. |

Manual office controls can overlap and cross rows. The green outline shows the allowed positions inside the panel. Use the control list to choose a lower layer, then inspect the hover or click target in offline play.

## Code tabs

| Tab | What it edits |
| --- | --- |
| Type | Inheritance, leader, membership, offices, resources, actions and entry rules. |
| Layout | Which window to create and which named controls receive each purpose. |
| GUI | Controls, coordinates, sizes, order, input and hit areas. |
| GFX | Sprite name, frame count and relative texture path. |
| History | Instance ID, type, dates, leader, members and office assignments. |
| Localisation | Player-facing labels and tooltips. Use the matching file suffix and language header in UTF-8 with BOM. |

Use the file list at the top to switch files. **Source** tells you which file was read. **Diff** compares the content with the draft. **Find and replace** uses the current file. `Ctrl+F` finds and `Ctrl+H` replaces. While draft syntax is invalid, the canvas may keep the last parseable view. Fix the reported line before inspecting the draft in the canvas again.

## Add a component

1. Choose a purpose. **Icon with tooltip** adds a decorative image and a read-only hit area. **Connect existing action** connects a defined reform, decree or other listed action. **Entry button** sets the HUD sprite.
2. Choose a sprite from the resource list or import a DDS/TGA you have permission to use. The advanced fields specify the sprite ID, GFX file, relative texture path and frames.
3. Pick a parent window and position. Inspect the diff and confirm it. HUD entries use their region's slots. Ordinary panel components use their parent's local coordinates.

Decoration draws an image. A tooltip adds text. Actions use the existing game rule and eligibility. Replacing material changes appearance. Text, read-only surfaces, resource value and monthly change each have their own binding. Confirm any replacement of an existing binding.

## Export and conflicts

- A **full package** contains the visible merged mod content and its descriptors. Review the dependencies, replacements and asset permissions before using it as a release.
- An **incremental package** contains added and changed files for a known baseline. Review the diff and use the matching version.
- **Source differences** checks if files have changed on disk since import. For external edits, merge from a fresh snapshot or choose to use the old snapshot explicitly.
- QA records are for tests you run. Start a round, play the campaign yourself, then read or import the log and attach the evidence.

| Problem | What to check |
| --- | --- |
| Duplicate type or same-name window | Check the source and dependency order. Name types, instances, windows and sprites distinctly. Use one copy of the content in a playset. |
| Another mod masks the file | Compare the actual playset. Check `replace_path`, same-path GUI/GFX and repeated fields such as `interface`. |
| Missing binding or delete blocked | Use **Bindings and repair**. Rebind a required control or remove an optional display first. Check the matching hover surface for a number. |
| Canvas and game differ | Check auto versus manual layout and UI scale. Export the current revision and load it in the game with runtime 0.9.2. |
| Keys display literally | Check UTF-8 BOM, language header, suffix, key spelling and dependency order. Verify dynamic parameter declarations. |
| Entry click crashes | Start with the first preflight error. In GUI, `tooltip` takes a native style and `tooltipText` takes a text key. Read the current Anvil log and crash report. |
| Data or assets are missing | Set the real Steam library and EU4 user folder. Extract content before importing. |
| Export reveals an existing error | Save the draft and fix that file and control before exporting. |

## Shortcuts

`Ctrl+S` saves, `Ctrl+Z` undoes, `Ctrl+Y` redoes, `Ctrl+F` finds, `Ctrl+H` replaces and `F5` switches offline play. `Delete` opens removal when the canvas or control selector has focus and the selected item can be removed.

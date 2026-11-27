# Beta 0.1 release validation

Windows x64, Steam EU4 1.37.5. Framework runtime 0.9.2 and editor 1.0.11. The release DLL is byte-for-byte the DLL from the 0.9.2 merged QA package.

| Check | Observed result |
| --- | --- |
| Game and plugin symbols | Anvil checked 114 symbols against Steam EU4 1.37.5. The copied lockfile matched. |
| Source build | The released source built both the editor and framework. The rebuilt DLL matched the QA DLL after normalising the linker-selected image base and PE checksum. |
| Workshop content | Preflight passed 3 layouts, 1 entry region and 2 instances. The released editor imported both 1444 examples and checked the leaders, entry sprites, layout IDs, opening reserve and Tumen appointments. |
| Editable examples | Coastal Charter with Workshop content, standalone Harbor Moot, and both examples together passed preflight with 0 structural errors. The validator reported the native country conditions and effects as requiring in-game verification. |
| Expected parse failures | Bad layout ID, duplicate interface block, and a localisation key in the native style field were detected. A mistyped message field and a fifth paragraph were rejected with file and line. |
| Desktop editor | The bundled 1.0.11 executable passed its Windows regression covering the clean Workshop samples, new projects, UI editing, a save/reopen round trip, and export. |
| DLL regressions | The identical 0.9.2 DLL's source passed the prior 15-program offline test set. It includes localisation, message targeting, two independent instance texts and the native popup/log paths with simulated game objects. |
| Release content | The Workshop consists of text, GUI and sprite definitions. Its message table preserves the entire stock 1.37.5 section and appends five entries. The package check resolved all 24 of its GUI sprite references against the installed game and content. |

## Campaign results and next check

A previous 0.9.1 QA campaign recorded both the Ashikaga and Mongol panels opening alongside the original HRE and Empire of China. This release uses the exact 0.9.2 runtime. Its pared-down Workshop playset and the new custom popup still need a campaign run. Use a regular 1444 game with the Beta, check the Ashikaga and Mongol panels, and use the Coastal Charter example to check the bulletin, two buttons, a log entry, and a political panel that stays open.

## 中文验收记录

四种内容组合的预检均为 **0 个结构错误**。编辑器实际读入了精简后的幕府和蒙古内容，核对了两套入口、面板、开局领袖与万户任命，并通过工程编辑、存读和导出测试。Anvil 核对了 114 个符号，运行时 DLL 与合并 QA 0.9.2 包一致。此前 0.9.1 战役记录过新旧体系同存。精简发布内容与 0.9.2 新弹窗在战役中的表现仍待验收。


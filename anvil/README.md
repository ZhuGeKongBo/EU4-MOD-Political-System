# Political Framework plugin for Anvil

`src/` contains this project's C sources, including loader integration, game bindings, content validation and offline tests. `plugin/` holds the public plugin manifest, Steam build-specific symbol declarations and their verification lock. `generated/` holds the header generated from the symbol declarations.

The plugin ID is `great-khan-native`. Version 0.9.2 uses Anvil API 2 and the compatible `anvil-game` service. Copy the full release plugin directory to `anvil/plugins/great-khan-native/` beneath the game folder after installing upstream Anvil. The ZIP and symbol metadata target Steam EU4 1.37.5 on Windows x64.

`api_manifest.json` is the exported script interface: every supported trigger and effect with its parameters, member sources, history fields, message fields, and the blocks that are handed to the game's own parser.

The plugin declares a conflict with the older test plugin `great-khan-history`; remove that plugin before installing this one.

This directory contains the project's implementation against Anvil's API. Upstream loader and standard plugin installation is documented separately. [Anvil 1.24.0](https://github.com/happy-dwarf-team/anvil/releases/tag/v1.24.0) · [Building this repository](../BUILD.md)

## 中文说明

`src/` 是本项目的 C 源码，包括加载器集成、游戏绑定、内容校验和离线测试；`plugin/` 是公开的插件清单、对应 Steam 版本的符号声明及校验锁；`generated/` 是由符号声明生成的头文件。

插件 ID 为 `great-khan-native`，0.9.2 版使用 Anvil API 2 和兼容的 `anvil-game` 服务。安装好上游 Anvil 后，把发布包里的整个插件文件夹复制到游戏目录的 `anvil/plugins/great-khan-native/`。压缩包与符号数据针对 Windows x64 的 Steam EU4 1.37.5。

`api_manifest.json` 是导出的脚本接口清单：列出所有支持的 trigger、effect 及其参数，成员来源，历史字段，通知字段，以及哪些块交给游戏自己的解析器读取。MOD 作者查语句时看这个文件。

本插件与旧的测试插件 `great-khan-history` 冲突，安装前请先移除它。

安装步骤见 [安装教程](../docs/Install-Guide.zh-CN.md)，构建见 [BUILD.md](../BUILD.md)。

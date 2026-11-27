# Political Framework plugin for Anvil

`src/` contains this project's C sources, including loader integration, game bindings, content validation and offline tests. `plugin/` holds the public plugin manifest, Steam build-specific symbol declarations and their verification lock. `generated/` holds the header generated from the symbol declarations.

The plugin ID is `great-khan-native`. Version 0.9.2 uses Anvil API 2 and the compatible `anvil-game` service. Copy the full release plugin directory to `anvil/plugins/great-khan-native/` beneath the game folder after installing upstream Anvil. The ZIP and symbol metadata target Steam EU4 1.37.5 on Windows x64.

This directory contains the project's implementation against Anvil's API. Upstream loader and standard plugin installation is documented separately. [Anvil 1.24.0](https://github.com/happy-dwarf-team/anvil/releases/tag/v1.24.0) · [Building this repository](../BUILD.md)

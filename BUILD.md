# Build on Windows

Use PowerShell, an x86-64 MinGW-w64 GCC toolchain and the Windows .NET Framework C# compiler. Obtain the Windows x64 SDK from the pinned [Anvil 1.24.0 release](https://github.com/happy-dwarf-team/anvil/releases/tag/v1.24.0). The SDK is a build dependency.

~~~powershell
.\Build.ps1 -AnvilSdkInclude 'C:\Tools\AnvilSDK\include'
~~~

Pass `-Gcc` for a different GCC path, `-OutputDirectory` for a separate build folder, or `-Target Editor` / `-Target Framework` to build one part. The default build folder is `build/`. The editor embeds the compiled native content validator. The framework build produces a drop-in `anvil/plugins/great-khan-native/` directory. Building uses source and the SDK and leaves game folders alone. Use the binaries and checksums on the release page for the exact tested artifacts.

The exported script interface is `anvil/api_manifest.json`. Framework test sources are under `anvil/src/test_*.c`. Some historical regressions use their own test content or a compatible local EU4 installation. Authoring validation uses the editor's **Preflight and export** with the examples or the Workshop content.

For a custom build, run the SDK's `anvil.exe check` on the built plugin directory against a local Steam 1.37.5 executable. Use the symbol files that accompany the plugin. Publish only the metadata and generated declarations. Keep executable files from the game and raw symbol dump output local.

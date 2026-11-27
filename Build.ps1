param(
    [ValidateSet('All', 'Editor', 'Framework')]
    [string]$Target = 'All',
    [Parameter(Mandatory)]
    [string]$AnvilSdkInclude,
    [string]$Gcc = 'gcc.exe',
    [string]$OutputDirectory = (Join-Path $PSScriptRoot 'build')
)

$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$cc = (Get-Command $Gcc -CommandType Application).Source
$sdk = (Resolve-Path -LiteralPath $AnvilSdkInclude).Path
$output = [IO.Path]::GetFullPath($OutputDirectory)
$anvil = Join-Path $root 'anvil'
$editor = Join-Path $root 'editor/src'
$templates = Join-Path $root 'editor/templates'
$generated = Join-Path $anvil 'generated'
if (!(Test-Path -LiteralPath (Join-Path $sdk 'anvil/anvil_plugin.h'))) {
    throw 'Pass the include folder from the Anvil 1.24.0 Windows x64 SDK.'
}
$null = [IO.Directory]::CreateDirectory($output)
$common = @('-std=c17', '-O2', '-Wall', '-Wextra', "-ffile-prefix-map=$root=.", '-I', $sdk, '-I', $generated, '-I', (Join-Path $anvil 'src'))
$units = @(Get-ChildItem -LiteralPath (Join-Path $anvil 'src') -Filter '*.c' |
    Where-Object { $_.Name -notlike 'test_*' -and $_.Name -notin @('plugin.c','export_script_api.c') } |
    Sort-Object Name | Select-Object -ExpandProperty FullName)

function Invoke-Compiler([string]$Command, [string[]]$Flags) {
    & $Command @Flags
    if ($LASTEXITCODE -ne 0) { throw "Compiler returned $LASTEXITCODE." }
}

$previousPath = $env:Path
$env:Path = (Split-Path -Parent $cc) + ';' + $env:Path
try {
    if ($Target -in @('All','Framework')) {
        $plugin = Join-Path $output 'anvil/plugins/great-khan-native'
        $null = [IO.Directory]::CreateDirectory($plugin)
        $dll = Join-Path $plugin 'great_khan_native.dll'
        Invoke-Compiler $cc ($common + @('-shared','-s','-Wl,--no-insert-timestamp',(Join-Path $anvil 'src/plugin.c')) +
            $units + @('-lcomctl32','-luser32','-o',$dll))
        foreach ($manifest in 'plugin.toml','symbols.toml','symbols.lock') {
            Copy-Item -LiteralPath (Join-Path $anvil "plugin/$manifest") -Destination $plugin
        }
        Get-FileHash -LiteralPath $dll
    }
    if ($Target -in @('All','Editor')) {
        $validator = Join-Path $output 'pf-content-check.exe'
        Invoke-Compiler $cc ($common + @('-s','-static','-Wl,--no-insert-timestamp',(Join-Path $editor 'Validate.c')) +
            $units + @('-lcomctl32','-luser32','-o',$validator))
        $csc = Join-Path $env:WINDIR 'Microsoft.NET/Framework64/v4.0.30319/csc.exe'
        if (!(Test-Path -LiteralPath $csc)) { throw 'The .NET Framework compiler is required.' }
        $exe = Join-Path $output 'EU4PoliticalEditor.exe'
        $flags = @('/nologo','/codepage:65001','/utf8output','/langversion:5','/target:winexe','/platform:x64','/optimize+',
            "/out:$exe","/win32manifest:$editor/Editor.manifest",'/r:System.Windows.Forms.dll','/r:System.Drawing.dll','/r:System.Web.Extensions.dll',
            "/resource:$validator,validator.exe","/resource:$editor/English.json,ui.english","/resource:$editor/Default-reference.txt,starter.reference",
            "/resource:$templates/common/political_systems/types/pf075_styles.txt,starter.styles",
            "/resource:$templates/common/political_system_layouts/pf075_layouts.txt,starter.layout",
            "/resource:$templates/interface/pf075_panels.gui,starter.gui",
            "/resource:$templates/interface/pf075_icons.gfx,starter.gfx",
            "/resource:$templates/interface/ps_instance_selector.gui,starter.selector",
            "/resource:$templates/localisation/pf075_presets_l_english.yml,starter.loc",
            "/resource:$templates/localisation/ps_interface_l_english.yml,starter.runtime_loc")
        $sources = @(Get-ChildItem -LiteralPath $editor -Filter '*.cs' | Sort-Object Name | Select-Object -ExpandProperty FullName)
        Invoke-Compiler $csc ($flags + $sources)
        Get-FileHash -LiteralPath $exe
    }
} finally {
    $env:Path = $previousPath
}

# Isolated PowerShell checks: no real compiler, user installation, or PATH writes.
param([string]$InstallerPath = (Join-Path $PSScriptRoot '../gadgets/install-windows.ps1'))
$ErrorActionPreference = 'Stop'
$tokens = $null
$parseErrors = $null
$null = [Management.Automation.Language.Parser]::ParseFile($InstallerPath, [ref]$tokens, [ref]$parseErrors)
if ($parseErrors.Count) { throw ($parseErrors | Out-String) }
. $InstallerPath
$script:Assertions = 0

function Assert-Equal($Actual, $Expected, [string]$Message) {
    if ($Actual -cne $Expected) { throw "$Message (expected '$Expected', got '$Actual')" }
    $script:Assertions++
}

function Assert-Throws([scriptblock]$Body, [string]$Pattern) {
    try { & $Body | Out-Null } catch {
        if ($_.Exception.Message -notmatch $Pattern) { throw }
        $script:Assertions++
        return
    }
    throw "Expected error matching $Pattern"
}

function Write-Fixture([string]$Root, [string]$Relative, [string]$Value = 'fixture') {
    $path = Join-Path $Root $Relative
    New-Item -ItemType Directory -Path (Split-Path $path -Parent) -Force | Out-Null
    [IO.File]::WriteAllText($path, $Value)
}

$base = Join-Path ([IO.Path]::GetTempPath()) ('batbox-installer-check-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $base | Out-Null
$originalLocation = (Get-Location).Path
$originalPath = $env:Path
$originalConfig = $env:BATBOX_CONFIG_DIR
try {
    Assert-Equal (Add-BatboxPathEntry 'C:\Tools' 'C:\Apps\Batbox\bin') 'C:\Tools;C:\Apps\Batbox\bin' 'PATH append'
    foreach ($existing in @('C:\Tools;c:\apps\batbox\BIN\', 'C:\Tools;"C:\Apps\Batbox\bin"', 'C:\Tools;C:/Apps/Batbox/bin/')) {
        Assert-Equal (Add-BatboxPathEntry $existing 'C:\Apps\Batbox\bin') $existing 'PATH normalization'
    }
    Assert-Equal (Add-BatboxPathEntry '' 'C:\Apps\Batbox\bin') 'C:\Apps\Batbox\bin' 'Empty PATH'

    $script:UserPath = 'C:\Tools'
    $script:ProcessPath = 'C:\Windows;C:\Tools'
    $script:UserWrites = 0
    $script:ProcessWrites = 0
    function Get-BatboxUserPath { return $script:UserPath }
    function Set-BatboxUserPath([string]$Value) { $script:UserPath = $Value; $script:UserWrites++ }
    function Get-BatboxProcessPath { return $script:ProcessPath }
    function Set-BatboxProcessPath([string]$Value) { $script:ProcessPath = $Value; $script:ProcessWrites++ }
    Update-BatboxPath 'C:\Apps\Batbox\bin'
    Update-BatboxPath 'c:\APPS\Batbox\bin\'
    Assert-Equal $script:UserWrites 1 'User PATH changed only once'
    Assert-Equal $script:ProcessWrites 1 'Process PATH changed only once'
    Assert-Equal $script:UserPath 'C:\Tools;C:\Apps\Batbox\bin' 'User PATH keeps unrelated entries'
    Assert-Equal $script:ProcessPath 'C:\Windows;C:\Tools;C:\Apps\Batbox\bin' 'Process PATH keeps unrelated entries'

    $package = Join-Path $base 'package with spaces'
    $destination = Join-Path $base 'installed Batbox'
    foreach ($file in @('bin/batbox.exe', 'bin/runtime.dll', 'etc/ssl/certs/ca-bundle.crt',
                         'LICENSE', 'THIRD_PARTY_NOTICES.md', 'licenses/runtime/LICENSE',
                         'native-client.example.json', 'settings.json')) {
        Write-Fixture $package $file "package $file"
    }
    Write-Fixture $destination 'settings.json' 'keep settings'
    Write-Fixture $destination 'native-client.json' 'keep client'
    Write-Fixture $destination 'unrelated.txt' 'keep unrelated'
    $env:BATBOX_CONFIG_DIR = Join-Path $base 'existing user configuration'
    Write-Fixture $env:BATBOX_CONFIG_DIR 'settings.json' 'keep user configuration'
    Install-BatboxWindows -PackageRoot $package -InstallDirectory $destination -NoPathUpdate
    Install-BatboxWindows -PackageRoot $package -InstallDirectory $destination -NoPathUpdate
    foreach ($file in @('bin/batbox.exe', 'bin/runtime.dll', 'etc/ssl/certs/ca-bundle.crt', 'licenses/runtime/LICENSE')) {
        Assert-Equal ([IO.File]::ReadAllText((Join-Path $destination $file))) "package $file" "Whole layout: $file"
    }
    Assert-Equal ([IO.File]::ReadAllText((Join-Path $destination 'settings.json'))) 'keep settings' 'Existing settings preserved'
    Assert-Equal ([IO.File]::ReadAllText((Join-Path $destination 'native-client.json'))) 'keep client' 'Existing client config preserved'
    Assert-Equal ([IO.File]::ReadAllText((Join-Path $destination 'unrelated.txt'))) 'keep unrelated' 'Unrelated files preserved'
    Assert-Equal ([IO.File]::ReadAllText((Join-Path $env:BATBOX_CONFIG_DIR 'settings.json'))) 'keep user configuration' 'User config untouched'
    Assert-Equal $script:UserWrites 1 'NoPathUpdate skipped user PATH'
    Assert-Equal ((Get-Location).Path) $originalLocation 'Caller working directory preserved'
    Assert-Throws { Copy-BatboxPackage $package (Join-Path $package 'nested-install') } 'cannot be inside'
    Remove-Item -LiteralPath (Join-Path $package 'etc/ssl/certs/ca-bundle.crt')
    Assert-Throws { Copy-BatboxPackage $package $destination } 'missing etc/ssl/certs/ca-bundle.crt'
    Write-Fixture $package 'etc/ssl/certs/ca-bundle.crt'

    $runtime = Join-Path $base 'msys with spaces/ucrt64'
    foreach ($tool in @('cmake.exe', 'python.exe', 'gcc.exe', 'g++.exe', 'ninja.exe', 'pkgconf.exe')) {
        Write-Fixture $runtime "bin/$tool"
    }
    Assert-Equal (Resolve-BatboxRuntimeRoot $runtime) $runtime 'Explicit UCRT64 prefix'
    Assert-Throws { Resolve-BatboxRuntimeRoot (Join-Path $base 'missing') } 'UCRT64 was not found'
    $checkout = Join-Path $base 'source checkout'
    Write-Fixture $checkout 'CMakePresets.json'
    Write-Fixture $checkout 'gadgets/package_indigenous.py'
    Write-Fixture $checkout 'build/indigenous/batbox.exe'
    $script:Commands = @()
    $script:FixturePackage = $package
    function Invoke-BatboxCommand([string]$Executable, [string[]]$Arguments, [string]$WorkingDirectory) {
        $script:Commands += [pscustomobject]@{Executable = $Executable; Arguments = $Arguments; Directory = $WorkingDirectory}
        if ($Executable.EndsWith('python.exe')) {
            $index = [Array]::IndexOf($Arguments, '--output')
            Compress-Archive -LiteralPath $script:FixturePackage -DestinationPath $Arguments[$index + 1]
        }
    }
    $sourceDestination = Join-Path $base 'source install'
    Install-BatboxWindows -SourceRoot $checkout -RuntimeRoot $runtime -InstallDirectory $sourceDestination -Jobs 3 -NoPathUpdate
    Assert-Equal $script:Commands.Count 3 'Source install configures, builds, and packages'
    Assert-Equal ($script:Commands[1].Arguments -join '|') '--build|build/indigenous|--target|batbox|--parallel|3' 'Source build target and job cap'
    Assert-Equal $script:Commands[0].Directory $checkout 'Source configure working directory'
    $runtimeFlag = [Array]::IndexOf($script:Commands[2].Arguments, '--runtime-root')
    Assert-Equal $script:Commands[2].Arguments[$runtimeFlag + 1] $runtime 'Packager receives coherent runtime'
    Assert-Equal ([IO.File]::ReadAllText((Join-Path $sourceDestination 'bin/runtime.dll'))) 'package bin/runtime.dll' 'Source install retains DLL closure'
    Assert-Equal $env:Path $originalPath 'Build restores process PATH'
    Assert-Equal ((Get-Location).Path) $originalLocation 'Source build preserves caller working directory'
    function Invoke-BatboxCommand { throw 'fixture compiler failure' }
    $failedDestination = Join-Path $base 'failed source install'
    Assert-Throws { Install-BatboxWindows -SourceRoot $checkout -RuntimeRoot $runtime -InstallDirectory $failedDestination -NoPathUpdate } 'fixture compiler failure'
    Assert-Equal $env:Path $originalPath 'Failed build restores process PATH'
    Assert-Equal (Test-Path -LiteralPath $failedDestination) $false 'Failed build does not install'

    $packagedInstaller = Join-Path $package 'gadgets/install-windows.ps1'
    New-Item -ItemType Directory -Path (Split-Path $packagedInstaller -Parent) -Force | Out-Null
    Copy-Item -LiteralPath $InstallerPath -Destination $packagedInstaller
    . $packagedInstaller
    $detectedDestination = Join-Path $base 'detected package install'
    Install-BatboxWindows -InstallDirectory $detectedDestination -NoPathUpdate
    Assert-Equal (Test-Path -LiteralPath (Join-Path $detectedDestination 'bin/batbox.exe')) $true 'Package root auto-detection'
    Push-Location -LiteralPath $base
    try {
        Install-BatboxWindows -PackageRoot $package -InstallDirectory 'relative install' -NoPathUpdate
        Assert-Equal (Test-Path -LiteralPath (Join-Path $base 'relative install/bin/batbox.exe')) $true 'Relative destination uses PowerShell working directory'
    } finally {
        Pop-Location
    }
    Write-Host "$script:Assertions installer assertions passed. No real User PATH or install directory changed."
} finally {
    $env:Path = $originalPath
    $env:BATBOX_CONFIG_DIR = $originalConfig
    Set-Location -LiteralPath $originalLocation
    Remove-Item -LiteralPath $base -Recurse -Force
}

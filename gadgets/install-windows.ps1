<#
.SYNOPSIS
Build or install Batbox for the current Windows user.
.DESCRIPTION
Run from a source checkout or an extracted Windows package. Source installs need
an MSYS2 UCRT64 toolchain and its dependencies. Existing configuration and unrelated
installation files are preserved. Administrator access is not required.
.EXAMPLE
.\gadgets\install-windows.ps1 -RuntimeRoot C:\msys64\ucrt64
.EXAMPLE
.\install-windows.ps1 -InstallDirectory D:\Apps\Batbox
#>
[CmdletBinding()]
param(
    [string]$InstallDirectory,
    [string]$RuntimeRoot,
    [string]$SourceRoot,
    [string]$PackageRoot,
    [ValidateRange(1, 128)][int]$Jobs = 2,
    [switch]$NoPathUpdate
)

function Get-BatboxPathKey([string]$Entry) {
    return [Environment]::ExpandEnvironmentVariables($Entry.Trim().Trim('"')).Replace('/', '\').TrimEnd('\').ToUpperInvariant()
}

function Add-BatboxPathEntry([string]$ExistingPath, [string]$BinDirectory) {
    $key = Get-BatboxPathKey $BinDirectory
    foreach ($entry in ($ExistingPath -split ';')) {
        if ((Get-BatboxPathKey $entry) -eq $key) { return $ExistingPath }
    }
    if ([string]::IsNullOrWhiteSpace($ExistingPath)) { return $BinDirectory }
    return $ExistingPath.TrimEnd(';') + ';' + $BinDirectory
}

function Get-BatboxUserPath { return [Environment]::GetEnvironmentVariable('Path', 'User') }
function Set-BatboxUserPath([string]$Value) { [Environment]::SetEnvironmentVariable('Path', $Value, 'User') }
function Get-BatboxProcessPath { return $env:Path }
function Set-BatboxProcessPath([string]$Value) { $env:Path = $Value }

function Update-BatboxPath([string]$BinDirectory) {
    $oldUserPath = Get-BatboxUserPath
    $newUserPath = Add-BatboxPathEntry $oldUserPath $BinDirectory
    if ($newUserPath -cne $oldUserPath) { Set-BatboxUserPath $newUserPath }
    $oldProcessPath = Get-BatboxProcessPath
    $newProcessPath = Add-BatboxPathEntry $oldProcessPath $BinDirectory
    if ($newProcessPath -cne $oldProcessPath) { Set-BatboxProcessPath $newProcessPath }
}

function Resolve-BatboxRuntimeRoot([string]$ExplicitRoot) {
    $candidates = @()
    if ($ExplicitRoot) {
        $candidates += $ExplicitRoot
    } else {
        foreach ($value in @($env:UCRT64_ROOT, $env:MSYSTEM_PREFIX, $env:MINGW_PREFIX)) {
            if ($value) { $candidates += $value }
        }
        foreach ($value in @($env:MSYS2_LOCATION, $env:MSYS2_ROOT)) {
            if ($value) { $candidates += Join-Path $value 'ucrt64' }
        }
        foreach ($name in @('cmake.exe', 'gcc.exe', 'python.exe')) {
            $command = Get-Command $name -ErrorAction SilentlyContinue
            if ($command -and $command.Source) {
                $prefix = Split-Path (Split-Path $command.Source -Parent) -Parent
                if ((Split-Path $prefix -Leaf) -eq 'ucrt64') { $candidates += $prefix }
            }
        }
        $bash = Get-Command bash.exe -ErrorAction SilentlyContinue
        if ($bash -and $bash.Source) {
            $bin = Split-Path $bash.Source -Parent
            $usr = Split-Path $bin -Parent
            if ((Split-Path $usr -Leaf) -eq 'usr') {
                $candidates += Join-Path (Split-Path $usr -Parent) 'ucrt64'
            }
        }
        $candidates += 'C:\msys64\ucrt64'
    }
    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath (Join-Path $candidate 'bin/cmake.exe') -PathType Leaf) {
            $resolved = (Resolve-Path -LiteralPath $candidate).ProviderPath
            foreach ($tool in @('python.exe', 'gcc.exe', 'g++.exe', 'ninja.exe', 'pkgconf.exe')) {
                if (-not (Test-Path -LiteralPath (Join-Path $resolved "bin/$tool") -PathType Leaf)) {
                    throw "UCRT64 toolchain is missing bin/$tool in $resolved. Install the dependencies in docs/BUILDING.md."
                }
            }
            return $resolved
        }
    }
    throw 'MSYS2 UCRT64 was not found. Pass -RuntimeRoot C:\msys64\ucrt64 after installing the dependencies in docs/BUILDING.md.'
}

function Invoke-BatboxCommand([string]$Executable, [string[]]$Arguments, [string]$WorkingDirectory) {
    Push-Location -LiteralPath $WorkingDirectory
    try {
        & $Executable @Arguments | Out-Host
        if ($LASTEXITCODE -ne 0) { throw "$Executable failed with exit code $LASTEXITCODE." }
    } finally {
        Pop-Location
    }
}

function Build-BatboxWindowsPackage([string]$Checkout, [string]$Runtime, [string]$TemporaryDirectory, [int]$Jobs = 2) {
    $cmake = Join-Path $Runtime 'bin/cmake.exe'
    $python = Join-Path $Runtime 'bin/python.exe'
    $runtimeBin = Join-Path $Runtime 'bin'
    $savedPath = $env:Path
    try {
        $env:Path = $runtimeBin + ';' + $savedPath
        Invoke-BatboxCommand $cmake @('--preset', 'indigenous', "-DCMAKE_PREFIX_PATH=$Runtime",
            "-DCMAKE_C_COMPILER=$(Join-Path $runtimeBin 'gcc.exe')",
            "-DCMAKE_CXX_COMPILER=$(Join-Path $runtimeBin 'g++.exe')",
            "-DCMAKE_MAKE_PROGRAM=$(Join-Path $runtimeBin 'ninja.exe')") $Checkout
        Invoke-BatboxCommand $cmake @('--build', 'build/indigenous', '--target', 'batbox', '--parallel', "$Jobs") $Checkout
        $binary = Join-Path $Checkout 'build/indigenous/batbox.exe'
        if (-not (Test-Path -LiteralPath $binary -PathType Leaf)) { throw "Build did not produce $binary." }
        $archive = Join-Path $TemporaryDirectory 'batbox-windows.zip'
        Invoke-BatboxCommand $python @((Join-Path $Checkout 'gadgets/package_indigenous.py'),
            '--binary', $binary, '--source', $Checkout, '--runtime-root', $Runtime,
            '--output', $archive, '--label', 'local-install') $Checkout
    } finally {
        $env:Path = $savedPath
    }
    $expanded = Join-Path $TemporaryDirectory 'expanded'
    Expand-Archive -LiteralPath $archive -DestinationPath $expanded
    $packages = @(Get-ChildItem -LiteralPath $expanded -Directory | Where-Object {
        Test-Path -LiteralPath (Join-Path $_.FullName 'bin/batbox.exe') -PathType Leaf
    })
    if ($packages.Count -ne 1) { throw 'The package must contain exactly one Batbox installation folder.' }
    return $packages[0].FullName
}

function Copy-BatboxPackage([string]$Package, [string]$Destination) {
    foreach ($required in @('bin/batbox.exe', 'etc/ssl/certs/ca-bundle.crt', 'LICENSE', 'THIRD_PARTY_NOTICES.md')) {
        if (-not (Test-Path -LiteralPath (Join-Path $Package $required) -PathType Leaf)) {
            throw "Windows package is missing $required. Use a complete extracted Batbox package."
        }
    }
    $sourcePath = [IO.Path]::GetFullPath($Package).TrimEnd([IO.Path]::DirectorySeparatorChar)
    $targetPath = [IO.Path]::GetFullPath($Destination).TrimEnd([IO.Path]::DirectorySeparatorChar)
    if ($targetPath -eq $sourcePath) { return }
    if ($targetPath.StartsWith($sourcePath + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
        throw 'The installation directory cannot be inside the extracted package.'
    }
    $entries = @(Get-ChildItem -LiteralPath $sourcePath -Recurse -Force)
    foreach ($entry in $entries) {
        if ($entry.Attributes -band [IO.FileAttributes]::ReparsePoint) {
            throw "Package links are not supported: $($entry.FullName)"
        }
    }
    New-Item -ItemType Directory -Path $targetPath -Force | Out-Null
    foreach ($entry in $entries) {
        $relative = $entry.FullName.Substring($sourcePath.Length + 1)
        $target = Join-Path $targetPath $relative
        if ($entry.PSIsContainer) {
            New-Item -ItemType Directory -Path $target -Force | Out-Null
        } else {
            if ($relative -in @('native-client.json', 'settings.json', '.env') -and (Test-Path -LiteralPath $target)) {
                continue
            }
            New-Item -ItemType Directory -Path (Split-Path $target -Parent) -Force | Out-Null
            Copy-Item -LiteralPath $entry.FullName -Destination $target -Force
        }
    }
}

function Install-BatboxWindows {
    param([string]$InstallDirectory, [string]$RuntimeRoot, [string]$SourceRoot,
          [string]$PackageRoot, [ValidateRange(1, 128)][int]$Jobs = 2, [switch]$NoPathUpdate)
    if ($SourceRoot -and $PackageRoot) { throw 'Choose either -SourceRoot or -PackageRoot.' }
    if (-not $InstallDirectory) {
        if (-not $env:LOCALAPPDATA) { throw 'LOCALAPPDATA is unset. Pass -InstallDirectory explicitly.' }
        $InstallDirectory = Join-Path $env:LOCALAPPDATA 'Batbox'
    }
    if (-not $SourceRoot -and -not $PackageRoot) {
        $candidate = $PSScriptRoot
        if ((Split-Path $candidate -Leaf) -eq 'gadgets') { $candidate = Split-Path $candidate -Parent }
        if (Test-Path -LiteralPath (Join-Path $candidate 'CMakePresets.json') -PathType Leaf) {
            $SourceRoot = $candidate
        } elseif (Test-Path -LiteralPath (Join-Path $candidate 'bin/batbox.exe') -PathType Leaf) {
            $PackageRoot = $candidate
        } else {
            throw 'Run the installer from a checkout or extracted package, or pass -SourceRoot or -PackageRoot.'
        }
    }
    $temporary = $null
    try {
        if ($SourceRoot) {
            $SourceRoot = (Resolve-Path -LiteralPath $SourceRoot).ProviderPath
            $runtime = Resolve-BatboxRuntimeRoot $RuntimeRoot
            $temporary = Join-Path ([IO.Path]::GetTempPath()) ('batbox-install-' + [Guid]::NewGuid().ToString('N'))
            New-Item -ItemType Directory -Path $temporary | Out-Null
            $PackageRoot = Build-BatboxWindowsPackage $SourceRoot $runtime $temporary $Jobs
        } else {
            $PackageRoot = (Resolve-Path -LiteralPath $PackageRoot).ProviderPath
        }
        $destination = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($InstallDirectory)
        Copy-BatboxPackage $PackageRoot $destination
        $bin = Join-Path $destination 'bin'
        if (-not $NoPathUpdate) { Update-BatboxPath $bin }
        Write-Host "Batbox installed in $destination"
        if ($NoPathUpdate) {
            Write-Host "PATH was not changed. Run $(Join-Path $bin 'batbox.exe') from your project."
        } else {
            Write-Host 'Run batbox from your project. New terminal windows will also find the command.'
        }
    } finally {
        if ($temporary -and (Test-Path -LiteralPath $temporary)) {
            Remove-Item -LiteralPath $temporary -Recurse -Force
        }
    }
}

# Dot-sourcing exposes the installer functions for isolated checks.
if ($MyInvocation.InvocationName -ne '.') {
    $ErrorActionPreference = 'Stop'
    if ([Environment]::OSVersion.Platform -ne [PlatformID]::Win32NT) {
        throw 'Use this installer in Windows PowerShell or PowerShell on Windows.'
    }
    Install-BatboxWindows @PSBoundParameters
}

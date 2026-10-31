# Batbox on Windows 🪟

Unzip the Windows package and keep its folders together. From PowerShell in the
extracted folder:

```powershell
.\gadgets\install-windows.ps1
batbox --cwd C:\path\to\project
```

Keep `bin/` beside `etc/`: the package’s UCRT64 DLLs and certificate bundle travel
with the executable. The installer copies that complete layout to
`%LOCALAPPDATA%\Batbox` and adds its `bin` directory to your user PATH and current
PowerShell session. No administrator access is needed. When launched through
a separate PowerShell process, open a new terminal afterward to refresh PATH.
For portable use, launch `.\bin\batbox.exe` directly from the extracted folder.

## Settle in

Follow [getting started](getting-started.md) to connect a Messages-compatible
model. Configuration lives at `%USERPROFILE%\.batbox\native-client.json`.
Use `/config` for endpoints, `/model` to pick a model, and `/help` for the command
catalog. Unicode paths and arguments pass through the indigenous Windows adapter.

Install Git for Windows for the Bash coding gadget, plus ripgrep and Scrapling with
its MCP dependencies. Current startup requires Scrapling on PATH or configured
through `scrapling_command`. SearXNG is a separate service for web search.
PowerShell can launch the app; its Bash gadget still uses Bash to run shell commands.

## What comes in the box

The package includes the executable, its linked UCRT64 DLLs, a CA bundle, licenses,
and example settings. `BUILD.json` records the packaged binary and library hashes;
`DEPENDENCIES.txt` describes runtime requirements. Optional libvips media loader
modules, PDF helpers, Git, ripgrep, and Scrapling remain external.

Start with `--help`, send a short message, and try reading a project file.
Headless Windows flows have been exercised under Wine. Interactive console,
clipboard, browser sign-in, UNC paths, and indigenous symlink behavior still need
acceptance on Windows itself. MSVC and indigenous PowerShell gadget execution remain
separate work.

For your next adventure, see [Everyday Batbox](everyday-use.md).
Toolchains, cross-build evidence, and packaging live in the
[developer reference](BUILDING.md).

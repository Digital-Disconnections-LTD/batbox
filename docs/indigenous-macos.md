# Batbox on macOS 🍎

A little terminal magic for your Mac. Unpack the macOS package, open Terminal,
and install it from the extracted folder:

```sh
bash gadgets/install-macos.sh
```

Open a new terminal, then run `batbox --cwd /path/to/project`. The installer adds
`~/.local/bin` to your shell PATH and keeps the package under
`~/.local/share/batbox`. You can also run `./bin/batbox` directly from the package.

Use a package matching your Mac’s architecture: Apple Silicon (`arm64`) or Intel
(`x86_64`). Current packages use installed Homebrew libraries. `MACHO-LINKS.json`
records the exact architectures and library paths; this preview is not a
standalone signed or notarized installer.

## Connect and go

Batbox reads `~/.batbox/native-client.json`. Set your model and Anthropic
Messages endpoint there, or start from the package’s `native-client.example.json`.
The endpoint serves `/v1/messages`; it needs Messages streaming support.
API credentials can use an `auth_token`, environment variable in a named profile,
or a credential helper. `/config` manages endpoints and `/model` selects a model.

Git, a shell, and ripgrep support coding gadgets. Current startup also needs
Scrapling with its MCP dependencies on PATH, or its executable in
`scrapling_command`. Install the helper in your preferred Python environment:

```sh
python3 -m pip install 'scrapling[ai]'
scrapling install
```

SearXNG is a separate search service; `searxng_url` sets its address. These helpers
and the inference backend are not bundled into Batbox.

## At home on your Mac

Clipboard image and file-URL reads use AppKit. Text copying uses `pbcopy`, browser
sign-in opens through `open`, and supported credentials use the macOS keychain.
PNG and TIFF clipboard images use the indigenous reader.

Start with a short piece of poetry, then try reading a project file. `/help` shows the
command map; `/bats` opens child conversations, `/memory` opens notes, and
`/exit` returns you to your shell. `--continue` resumes a roost conversation.

The preview still needs broader clipboard, keychain, terminal, and Intel runtime
acceptance. Build and package recipes are in `BUILDING.md`, the developer
reference supplied with the package and source tree.

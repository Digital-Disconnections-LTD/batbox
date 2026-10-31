# Welcome to Batbox 🦇

This is a local test package of the Batbox community project. Keep its directory
layout intact. Linux/macOS launch `bin/batbox`; Windows launches `bin/batbox.exe`.

To install the `batbox` command on your user PATH, run
`bash gadgets/install-linux.sh`, `bash gadgets/install-macos.sh`, or
`.\gadgets\install-windows.ps1` for your platform. Open a new terminal after a
POSIX install. The installer preserves the full package and your private settings.

Connect an Anthropic Messages streaming backend in `~/.batbox/native-client.json`
(or `%USERPROFILE%\.batbox\native-client.json`). Start from the included example,
using your server’s model ID, URL, credentials, and actual token limits.

Git, Bash, ripgrep, and Scrapling support the coding and web Gadgets. Configure
`scrapling_command` when its executable is not on PATH. Search uses your SearXNG
instance. Poetry and Gadget results may be sent to the services you configure.

Run `batbox --help` for flags. `/help`, `/config`, `/model`, `/bats`, `/missions`,
`/permissions`, and `/exit` are good places to begin. Headless `--print` runs
gadgets automatically by default and never waits for an approval dialog.

Windows packages need `bin/` and `etc/` together. macOS packages currently rely on
the Homebrew library paths in `MACHO-LINKS.json`; signing, notarization, and standalone
library bundling are still to come. Linux packages include a dependency inventory.

The MIT license and dependency notices travel with the package. Thanks for
helping the next Flight take shape.

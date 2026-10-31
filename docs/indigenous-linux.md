# Batbox on Linux 🐧

Open a terminal, unpack the Linux package, and install from the extracted folder:

```sh
bash gadgets/install-linux.sh
```

Open a new terminal, then run `batbox --cwd /path/to/project`. The command lives
in `~/.local/bin`, with the complete package under `~/.local/share/batbox`.
You can also use `./bin/batbox` directly from the package.

Follow [getting started](getting-started.md) to connect your Messages backend.
Your settings live in `~/.batbox/`, just as they do on macOS.

## Keep the helpers close

The Linux preview uses system shared libraries. `DEPENDENCIES.txt` in the package
records the exact libraries expected by that executable. Use a matching system;
the preview does not bundle those libraries.

Git, Bash, and ripgrep support coding gadgets. Scrapling and its MCP dependencies
must be available for current startup; SearXNG supplies web search. PDF handling
can require Poppler and Ghostscript. These helpers and an inference server are
separate from the Batbox package.

## Ready for a first lap

Try `batbox --help`, send a short message, and then ask Batbox to read a
small project file. `/exit` returns you to your shell. The
[everyday guide](everyday-use.md) has bats, memory, and flight tips.

Source builds and package creation live in the [developer reference](BUILDING.md).
[The roadmap](../ROADMAP.md) records tested behavior and remaining work.

# Web magic, with excellent company ✨

Batbox works without any web helpers. Two optional open-source projects add web
search and page fetching:

- **[Scrapling](https://github.com/D4Vinci/Scrapling)** fetches pages for the
  WebFetch Gadget and provides browser tools through its MCP server.
- **[SearXNG](https://github.com/searxng/searxng)** answers WebSearch through
  an instance you run or choose.

Run `batbox setup web` to install both, or point Batbox at your own instance.
These integrations do not imply affiliation or endorsement. We are grateful for
the work of both communities.

## Without them

Startup never waits for either helper. Scrapling starts the first time WebFetch
runs, and SearXNG is contacted only when WebSearch runs. When a helper is
missing, the welcome card and `/status` show one line such as
`Web search and fetch are not set up · run: batbox setup web`, and the model
is told in the Gadget's description. A call made anyway returns a tool error
that names the problem and the fix; nothing hangs or crashes.

## `batbox setup web`

The command runs the bundled `gadgets/setup-web.sh` with bash on macOS or Linux.
It never uses Docker, asks no questions, and is safe to re-run.

```sh
batbox setup web --dry-run     # print the plan, change nothing
batbox setup web               # install both helpers
batbox setup web --uninstall   # remove the SearXNG service and its files
```

Other options: `--port N` (SearXNG port, default 8888), `--scrapling-only`,
`--searxng-only`, `--no-browsers` (skip Scrapling's browser downloads; WebFetch
only needs HTTP), `--force` (replace a `searxng/settings.yml` the script did not
write, keeping a `.bak`), and `--write-scrapling-command PATH`.

What it does for Scrapling:

1. Keeps an existing `scrapling` on PATH if its Python has the MCP extras.
2. Otherwise runs `pipx install 'scrapling[ai]'`, or `uv tool install
   'scrapling[ai]'` when pipx is missing. With neither, it prints how to install
   pipx (`brew install pipx` or `python3 -m pip install --user pipx`).
3. Runs `scrapling install` for the browser dependencies.
4. Changes no configuration when `scrapling` is on PATH. If the install
   directory is not on PATH, it sets `scrapling_command` in
   `native-client.json` through a JSON merge that keeps every other key, writes
   a `.bak` first and leaves the file at mode 0600.

What it does for SearXNG:

1. Downloads SearXNG at a pinned upstream commit (named at the top of the
   script) into `~/.local/share/batbox/searxng/src` and installs it into a
   virtual environment at `~/.local/share/batbox/searxng/venv`, following
   SearXNG's own non-container steps. `$XDG_DATA_HOME` replaces
   `~/.local/share` when set.
2. Writes `~/.batbox/searxng/settings.yml`: 127.0.0.1 only, port 8888, a random
   secret key, HTML and JSON output, the limiter off, and five engines that need
   no API keys (DuckDuckGo, Brave, Wikipedia, GitHub, Stack Overflow).
3. Installs a user service and starts it:
   - macOS: launchd agent `com.batbox.searxng` at
     `~/Library/LaunchAgents/com.batbox.searxng.plist`, logging to
     `~/Library/Logs/batbox/searxng.stdout.log` and `searxng.stderr.log`.
   - Linux: systemd user unit `batbox-searxng.service` in
     `~/.config/systemd/user/`, enabled with `systemctl --user enable`. Read its
     log with `journalctl --user -u batbox-searxng.service`.
4. Waits for the port, runs one JSON search, and prints the result count.

Batbox finds this instance through the settings file, so SearXNG needs no
change to `native-client.json`. `--uninstall` stops and removes the service,
the virtual environment, the source, the logs and the settings file (only if
the script wrote it). It leaves Scrapling installed; remove it with
`pipx uninstall scrapling` or `uv tool uninstall scrapling`.

Windows has no setup script yet. Install Scrapling with pipx and set
`searxng_url` to an instance you run.

## Checking status

`/connectors` opens a dialog listing Scrapling (the executable found, and its
version when the package metadata is readable), SearXNG (the URL, whether it
answered, whether JSON output is on) and each MCP server Batbox loaded, each
with its state and the exact fix. **Set up now** shows the
setup command; run it in a terminal, then restart Batbox.

`batbox doctor` (also `batbox --doctor`) prints the same checks as plain text
and exits 0 when every check passes, 1 otherwise. It starts no helper and only
reads MCP server commands; it does contact SearXNG with one search.

## Your own instances

In your private `native-client.json`:

```json
{
  "scrapling_command": "/absolute/path/to/scrapling",
  "searxng_url": "http://127.0.0.1:8888/search"
}
```

`scrapling_command` is an executable path, not a shell command. Batbox starts
it with the `mcp` argument, so its Python needs Scrapling's MCP extras
(`scrapling[ai]`). `scrapling` on PATH is the default.

SearXNG is chosen in this order:

1. `searxng_url` (or `BATBOX_SEARXNG_URL`). Give the complete search endpoint,
   including `/search`; the homepage can answer with an HTTP 308 redirect.
2. `searxng/settings.yml` in your Batbox configuration folder. Batbox reads
   `server.port` and `server.bind_address` from it.
3. `http://127.0.0.1:8888/search`, SearXNG's default port, tried only when a
   search runs.

The instance must have JSON output enabled (`search.formats` includes `json`);
otherwise SearXNG answers HTTP 403 and `/connectors` says so.

Search terms go to your SearXNG instance; fetch and browser requests go to the
target sites. Saved browser state can contain authentication data, so keep it
outside your repository.

Search and browser work support cancellation. A browser helper is a separate
process, and its own website timeouts may differ from Batbox's connection timeout.

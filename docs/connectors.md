# Web magic, with excellent company ✨

Batbox works without any web helpers. Two optional open-source projects add web
search and page fetching:

- **[Scrapling](https://github.com/D4Vinci/Scrapling)** fetches pages for the
  WebFetch Gadget and provides browser tools through its MCP server. Batbox
  needs Scrapling 0.3 or later.
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

## Which Scrapling tool WebFetch calls

Batbox talks to `scrapling mcp` through the same MCP client it uses for your
own [MCP servers](#mcp-servers). The first time WebFetch runs, it lists the server's tools once
and picks the plain HTTP fetch tool by name: `make_request` (Scrapling 0.4.15
and later) or `get` (Scrapling 0.3 to 0.4.14). Both take `url` and
`extraction_type` (Batbox asks for `markdown`) and return the page's status,
content and final URL. When the server offers neither, the call fails with a
message such as `Scrapling 0.2.9 has no tool named make_request or get (it
offers: ...); Batbox needs Scrapling >= 0.3. Run: batbox setup web`.

Scrapling 0.3 is the oldest release Batbox supports, and there is no upper
bound. Two upstream changes are worth knowing:

- Releases before 0.4.13 accept the `mcp` 2 package, which removed the
  `mcp.server.fastmcp` module their server imports. Installed fresh today, they
  pull in `mcp` 2 and their MCP server does not start. `batbox setup web`
  therefore installs the newest release, and `batbox doctor` names this case
  with its fix (`batbox setup web --upgrade`, or `mcp<2` in Scrapling's
  environment).
- On its first start, Scrapling 0.3.x prints its model downloads on stdout.
  Batbox skips stdout lines that are not JSON, so the first WebFetch works.
  Those lines appear with the server's stderr if Scrapling then fails.

## `batbox setup web`

The command runs the bundled `gadgets/setup-web.sh` with bash on macOS or Linux.
It never uses Docker, asks no questions, and is safe to re-run.

```sh
batbox setup web --dry-run     # print the plan, change nothing
batbox setup web               # install both helpers
batbox setup web --upgrade     # reinstall both at the versions this Batbox wants
batbox setup web --uninstall   # remove the SearXNG service and its files
```

Other options: `--port N` (SearXNG port, default 8888), `--scrapling-only`,
`--searxng-only`, `--no-browsers` (skip Scrapling's browser downloads; WebFetch
only needs HTTP), `--force` (replace a `searxng/settings.yml` the script did not
write, keeping a `.bak`), and `--write-scrapling-command PATH`.

What it does for Scrapling:

1. Keeps an existing `scrapling` on PATH if its Python can import Scrapling's
   command line and MCP server modules and it is Scrapling 0.3 or later. With
   `--upgrade`, or when that import fails, it reinstalls instead.
2. Otherwise runs `pipx install 'scrapling[ai]>=0.3'`, or `uv tool install
   'scrapling[ai]>=0.3'` when pipx is missing, adding `--force` to replace an
   older Scrapling. With neither, it prints how to install pipx (`brew install
   pipx` or `python3 -m pip install --user pipx`).
3. Runs `scrapling install` for the browser dependencies.
4. Changes no configuration when `scrapling` is on PATH. If the install
   directory is not on PATH, it sets `scrapling_command` in
   `native-client.json` through a JSON merge that keeps every other key, writes
   a `.bak` first and leaves the file at mode 0600.

What it does for SearXNG:

1. Downloads SearXNG at a pinned upstream commit into
   `~/.local/share/batbox/searxng/src` and installs it into a
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

SearXNG has no versioned releases, so the script pins a master commit:
currently `54ca1d9cfb08` from 2026-10-25 (SearXNG `2026.10.25+54ca1d9cf`),
named at the top of `gadgets/setup-web.sh`. Batbox never follows SearXNG master
on its own. When a Batbox release moves the pin, run
`batbox setup web --upgrade`. It downloads the new commit, rebuilds the
virtual environment (also picking up newer dependency releases), restarts the
service and reinstalls Scrapling at its newest release. Re-running without
`--upgrade` also moves to a new pin, but leaves a current install alone.

Batbox finds this instance through the settings file, so SearXNG needs no
change to `native-client.json`. `--uninstall` stops and removes the service,
the virtual environment, the source, the logs and the settings file (only if
the script wrote it). It leaves Scrapling installed; remove it with
`pipx uninstall scrapling` or `uv tool uninstall scrapling`.

Windows has no setup script yet. Install Scrapling with pipx and set
`searxng_url` to an instance you run.

## Checking status

`/connectors` opens a dialog listing Scrapling (the executable found, its
version when the package metadata is readable, and whether its MCP server
answers with a fetch tool Batbox can use), SearXNG (the URL, whether it
answered, whether JSON output is on) and each MCP server Batbox loaded, each
with its state and the exact fix. **Set up now** shows the
setup command; run it in a terminal, then restart Batbox.

`batbox doctor` (also `batbox --doctor`) prints the same checks as plain text
and exits 0 when every check passes, 1 otherwise. It starts `scrapling mcp`
once to list its tools, then stops it, and flags a Scrapling that offers no
fetch tool Batbox can use as `incompatible`. It only reads your MCP server
commands, and it contacts SearXNG with one search plus a request for `/config`
(limited to 2 seconds) to show the instance's version:

```text
  [ok]  SearXNG: ready · http://127.0.0.1:8888/search (searxng_url) · SearXNG 2026.10.25+54ca1d9cf · 12 results for "batbox"
  [--]  SearXNG: JSON off · http://127.0.0.1:8888/search (searxng_url) · expected HTTP 200 with JSON search results, got HTTP 403 (text/html; charset=utf-8): "<!DOCTYPE html>..."
        fix: SearXNG answers 403 when search.formats in its settings.yml lacks json; add json there and restart it, or run: batbox setup web
  [--]  SearXNG: shape mismatch · http://127.0.0.1:8888/search (searxng_url) · expected a JSON object with a "results" array, got an object without "results" (keys: query, answers)
        fix: this SearXNG answers in a shape Batbox does not read; run: batbox setup web for the tested version, or check that searxng_url names a SearXNG /search endpoint
```

## What Batbox reads from SearXNG

WebSearch requests `<searxng_url>?q=...&format=json` and needs HTTP 200, a JSON
object, and a `results` array whose entries have a string `url`. It also uses
`title` when it is a string. Every other field (`content`, `engine`,
`publishedDate`, `number_of_results`, `suggestions`, ...) is optional, and
nulls are fine. Entries without a `url` are skipped; when none has one, the
format has changed. In that case, and for any other answer, the tool error
and `batbox doctor` say what was expected and what came back: the status, the
content type, the first 120 characters of the body, or the missing field.

## Keeping up with upstream

`.github/workflows/web-helper-drift.yml` runs every Monday (and on demand)
with `checks/web_helper_drift.py`. It tests the newest `main` build at both
ends of the supported range:

- **newest**: SearXNG master and the newest Scrapling;
- **pinned**: the pinned SearXNG commit and Scrapling 0.3, with dependencies
  as of 0.3's release (what its users have).

Each end gets `batbox doctor` (both helpers `ready`), a fetch of
https://example.com through the Scrapling connector, and a real search.
On failure the workflow opens an issue titled "Web helper drift detected", or
comments on the open one. To try a candidate pin locally (needs `uv` and
`git`; everything stays in a temporary directory):

```sh
cmake --build build/indigenous-checks --target batbox batbox-connectors
python3 checks/web_helper_drift.py --batbox build/indigenous-checks/batbox \
  --connectors build/indigenous-checks/batbox-connectors --searxng <commit>
```

`--scrapling <version>` or `--scrapling-command <path>` choose the Scrapling
for that run. To move the pin, update `SEARXNG_COMMIT`, `SEARXNG_COMMIT_DATE`
and `SEARXNG_VERSION` in `gadgets/setup-web.sh` and this page together.

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
otherwise SearXNG answers HTTP 403 and `/connectors` and `batbox doctor` say
so.

Search terms go to your SearXNG instance; fetch and browser requests go to the
target sites. Saved browser state can contain authentication data, so keep it
outside your repository.

Search and browser work support cancellation. A browser helper is a separate
process, and its own website timeouts may differ from Batbox's connection timeout.

## MCP servers

MCP servers add Gadgets, poetry commands and resources. `/mcp` shows each
server's state and offers **Reconnect**, **Enable** or **Disable**, **Log in**
and **Log out**. Batbox remembers disabled servers in `mcp-state.json` in your
configuration folder.

User servers load from `~/.batbox/mcp.json`; a project's `.mcp.json` adds its
servers and replaces user entries with the same name. An `mcpServers` object in
`native-client.json`, or `--mcp-config PATH`, is used instead of both files.

```json
{
  "mcpServers": {
    "local": {"command": "my-mcp-server", "args": [], "env": {}},
    "remote": {"type": "http", "url": "https://example.org/mcp"}
  }
}
```

Batbox connects over stdio, Streamable HTTP (`"type": "http"`) and SSE
(`"type": "sse"`). An entry with a `url` and no `type` is HTTP. A server's poetry
commands join the slash-command catalog, and its resources can join the
conversation context. Strings in an entry accept `${VARIABLE}` and
`${VARIABLE:-default}`; an unset variable without a default is an error that
names it.

Remote servers take static `headers`, and `headersHelper` can add more. Batbox
runs the helper as a shell command in the project directory with
`BATBOX_MCP_SERVER_NAME` and `BATBOX_MCP_SERVER_URL` set; it must print a JSON
object of string header values within 10 seconds. If it fails, Batbox connects
with the static headers. Keep secrets in your private configuration or
environment.

Servers that use OAuth sign in with **Log in** in `/mcp`. On macOS the
credentials go to the Keychain; on Linux and Windows they are stored in
`.batbox-credentials.json` in your configuration folder. WebSocket servers and
enterprise XAA sign-in are not supported.

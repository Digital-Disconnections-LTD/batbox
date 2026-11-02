#!/usr/bin/env python3
"""Check gadgets/setup-web.sh and `batbox doctor` in throwaway homes.

Nothing is installed: the script runs its config merge, dry runs and option
handling; doctor talks to a local fake SearXNG and a fake Scrapling MCP server.
"""
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path
import stat
import subprocess
import sys
import tempfile
import threading

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "gadgets" / "setup-web.sh"
SCRAPLING_047_TOOLS = ["open_session", "close_session", "list_sessions", "get", "bulk_get",
                       "fetch", "bulk_fetch", "stealthy_fetch", "bulk_stealthy_fetch", "screenshot"]
# An MCP server that advertises the tool names listed in tools.json beside it.
FAKE_SCRAPLING = """#!/usr/bin/env python3
import json, sys
from pathlib import Path
names = json.loads(Path(__file__).with_name("tools.json").read_text())
for line in sys.stdin:
    request = json.loads(line)
    if "id" not in request:
        continue
    if request["method"] == "initialize":
        result = {"protocolVersion": "2025-03-26", "capabilities": {"tools": {}}}
    elif request["method"] == "tools/list":
        result = {"tools": [{"name": name, "inputSchema": {"type": "object"}} for name in names]}
    else:
        result = {"content": [{"type": "text", "text": "Unknown tool"}], "isError": True}
    print(json.dumps({"jsonrpc": "2.0", "id": request["id"], "result": result}), flush=True)
"""


def environment(home, **extra):
    env = {key: value for key, value in os.environ.items()
           if not key.startswith(("BATBOX_", "XDG_", "PIPX_", "UV_"))}
    env.update(HOME=str(home), NO_PROXY="127.0.0.1,localhost", no_proxy="127.0.0.1,localhost")
    env.update(extra)
    return env


def script(home, *arguments, env=None, success=True):
    result = subprocess.run(["bash", str(SCRIPT), *arguments], env=env or environment(home),
                            text=True, capture_output=True, timeout=30)
    if (result.returncode == 0) != success:
        raise AssertionError(f"setup-web.sh {arguments} exited {result.returncode}:\n"
                             + result.stdout + result.stderr)
    return result


def mode(path):
    return stat.S_IMODE(path.stat().st_mode)


def check(condition, message):
    if not condition:
        raise AssertionError(message)


def merge_cases(base):
    home = base / "merge home"
    config = home / ".batbox" / "native-client.json"
    config.parent.mkdir(parents=True)
    original = {
        "profile": "main::model",
        "endpoints": {"main": {"base_url": "https://example.invalid", "models": ["a", "b"],
                               "headers": {"x-trace": "on"}, "context_window": 200000}},
        "label": "café ✓",
        "numbers": [1, 2.5, 10000000000, None, True],
        "scrapling_command": "/old/scrapling",
        "zeta": {},
    }
    text = json.dumps(original, indent=4, ensure_ascii=False) + "\n"
    config.write_text(text, encoding="utf-8")
    config.chmod(0o644)
    script(home, "--write-scrapling-command", "/new path/bin/scrapling")
    merged = json.loads(config.read_text(encoding="utf-8"))
    check(list(merged) == list(original), f"key order changed: {list(merged)}")
    for key, value in original.items():
        if key != "scrapling_command":
            check(merged[key] == value, f"{key} changed: {merged[key]!r}")
    check(merged["scrapling_command"] == "/new path/bin/scrapling", "scrapling_command not set")
    check(mode(config) == 0o600, f"config mode is {oct(mode(config))}")
    backup = config.with_name("native-client.json.bak")
    check(backup.read_text(encoding="utf-8") == text, "backup differs from the original")
    check(mode(backup) == 0o600, f"backup mode is {oct(mode(backup))}")
    check(config.read_text(encoding="utf-8").startswith('{\n    "profile"'), "indent not kept")
    before = config.read_bytes()
    again = script(home, "--write-scrapling-command", "/new path/bin/scrapling")
    check("already set" in again.stdout and config.read_bytes() == before, "second merge rewrote")

    fresh = base / "fresh config dir"
    created = fresh / "native-client.json"
    script(home, "--write-scrapling-command", "/opt/scrapling",
           env=environment(home, BATBOX_CONFIG_DIR=str(fresh)))
    check(json.loads(created.read_text()) == {"scrapling_command": "/opt/scrapling"},
          "new config has unexpected keys")
    check(mode(created) == 0o600, "new config is not 0600")

    odd = base / "array config"
    odd.mkdir()
    (odd / "native-client.json").write_text("[1, 2]\n")
    result = script(home, "--write-scrapling-command", "/x", success=False,
                    env=environment(home, BATBOX_CONFIG_DIR=str(odd)))
    check("not a JSON object" in result.stderr + result.stdout, "array config error unclear")
    check((odd / "native-client.json").read_text() == "[1, 2]\n", "array config was modified")


def dry_runs(base, binary):
    home = base / "dry home"
    home.mkdir()
    bare = environment(home, PATH="/usr/bin:/bin")
    scrapling = script(home, "--dry-run", "--scrapling-only", env=bare)
    out = scrapling.stdout
    check("Neither pipx nor uv" in out or "already installed" in out or "would run" in out,
          "Scrapling dry run said nothing useful:\n" + out)
    searxng = script(home, "--dry-run", "--searxng-only", "--port", "18999")
    out = searxng.stdout
    check("54ca1d9cfb08" in out and "18999" in out and "nothing was changed" in out,
          "SearXNG dry run lacks commit, port or summary:\n" + out)
    check(("com.batbox.searxng" in out) if sys.platform == "darwin" else
          ("batbox-searxng.service" in out), "dry run does not name the service")
    leftovers = [path for path in home.rglob("*")]
    check(not leftovers, f"dry run wrote files: {leftovers}")
    outdated(base)
    script(home, "--uninstall", "--dry-run")
    script(home, "--port", "0", success=False)
    script(home, "--bogus", success=False)
    direct = script(home, "--help").stdout
    embedded = subprocess.run([str(binary), "setup", "web", "--help"], env=environment(home),
                              text=True, capture_output=True, timeout=30)
    check(embedded.returncode == 0 and embedded.stdout == direct,
          "batbox setup web does not run the bundled script:\n" + embedded.stdout + embedded.stderr)
    usage = subprocess.run([str(binary), "setup"], env=environment(home), text=True,
                           capture_output=True, timeout=30)
    check(usage.returncode == 2 and "setup web" in usage.stderr, "bare `batbox setup` usage")


def outdated(base):
    home = base / "outdated home"
    tools = base / "outdated-tools"
    tools.mkdir()
    for version in ("0.2.9", "0.4.7"):
        python = tools / f"python-{version}"
        python.write_text(f"#!/bin/sh\necho {version}\n")
        python.chmod(0o755)
        bin_dir = tools / f"bin-{version}"
        bin_dir.mkdir()
        (bin_dir / "scrapling").write_text(f"#!{python}\n")
        (bin_dir / "scrapling").chmod(0o755)
    old = script(home, "--dry-run", "--scrapling-only", "--no-browsers",
                 env=environment(home, PATH=f"{tools / 'bin-0.2.9'}:/usr/bin:/bin")).stdout
    check("is Scrapling 0.2.9; Batbox needs Scrapling 0.3 or later" in old and
          "already installed" not in old, "outdated Scrapling was kept:\n" + old)
    current = script(home, "--dry-run", "--scrapling-only", "--no-browsers",
                     env=environment(home, PATH=f"{tools / 'bin-0.4.7'}:/usr/bin:/bin")).stdout
    check("already installed" in current and "Scrapling 0.4.7" in current,
          "a current Scrapling was not kept:\n" + current)
    installer = tools / "installer"
    installer.mkdir()
    (installer / "pipx").write_text("#!/bin/sh\nexit 0\n")
    (installer / "pipx").chmod(0o755)
    upgraded = script(home, "--dry-run", "--scrapling-only", "--no-browsers", "--upgrade",
                      env=environment(home, PATH=f"{tools / 'bin-0.4.7'}:{installer}:/usr/bin:/bin")).stdout
    check("upgrading" in upgraded and "(Scrapling 0.4.7) to the newest release" in upgraded and
          "would run: pipx install --force scrapling[ai]>=0.3" in upgraded and
          "already installed" not in upgraded, "--upgrade kept the installed Scrapling:\n" + upgraded)
    broken_python = tools / "python-broken"
    broken_python.write_text("#!/bin/sh\nexit 1\n")
    broken_python.chmod(0o755)
    broken_bin = tools / "bin-broken"
    broken_bin.mkdir()
    (broken_bin / "scrapling").write_text(f"#!{broken_python}\n")
    (broken_bin / "scrapling").chmod(0o755)
    broken = script(home, "--dry-run", "--scrapling-only", "--no-browsers",
                    env=environment(home, PATH=f"{broken_bin}:{installer}:/usr/bin:/bin")).stdout
    check("cannot load Scrapling's MCP server" in broken and
          "would run: pipx install --force scrapling[ai]>=0.3" in broken,
          "a Scrapling whose MCP server cannot load was kept:\n" + broken)
    searxng = script(home, "--dry-run", "--searxng-only", "--upgrade", "--port", "18998").stdout
    check("pinned commit 54ca1d9cfb08 from 2026-10-25" in searxng and
          "would download" in searxng and "already present" not in searxng,
          "--upgrade did not reinstall the pinned SearXNG:\n" + searxng)
    both = script(home, "--upgrade", "--uninstall", success=False)
    check("cannot be combined" in both.stderr, "--upgrade --uninstall was accepted:\n" + both.stderr)


GOOD_SEARCH = (200, "application/json",
               json.dumps({"results": [{"title": "t", "url": "https://example.invalid"}]}))


def doctor_line(output, name):
    lines = output.splitlines()
    for index, line in enumerate(lines):
        if line.startswith(("  [ok]  " + name + ":", "  [--]  " + name + ":")):
            following = lines[index + 1] if index + 1 < len(lines) else ""
            return line + ("\n" + following if following.startswith("        fix: ") else "")
    raise AssertionError(f"doctor printed no {name} line:\n{output}")


def doctor(base, binary):
    state = {"search": GOOD_SEARCH, "version": "2026.10.25+54ca1d9cf"}

    class Handler(BaseHTTPRequestHandler):
        def log_message(self, *_):
            pass

        def do_GET(self):
            if self.path.startswith("/config"):
                if state["version"] is None:
                    status, kind, body = 404, "text/html", "<h1>Not Found</h1>"
                else:
                    status, kind = 200, "application/json"
                    body = json.dumps({"brand": {}, "version": state["version"]})
            else:
                status, kind, body = state["search"]
            self.send_response(status)
            self.send_header("Content-Type", kind)
            self.end_headers()
            self.wfile.write(body.encode())

    server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    try:
        home = base / "doctor home"
        venv = home / "tools" / "venv"
        executable = venv / "bin" / "scrapling"
        executable.parent.mkdir(parents=True)
        executable.write_text(FAKE_SCRAPLING)
        executable.chmod(0o755)
        advertised = executable.with_name("tools.json")
        advertised.write_text(json.dumps(SCRAPLING_047_TOOLS))
        (venv / "lib" / "python3.12" / "site-packages" / "scrapling-9.9.dist-info").mkdir(parents=True)
        config = home / ".batbox"
        config.mkdir()
        (config / "native-client.json").write_text(json.dumps({
            "scrapling_command": str(executable),
            "searxng_url": f"http://127.0.0.1:{server.server_port}/search",
            "mcpServers": {"tool": {"command": "sh"}}}))
        run = lambda *args: subprocess.run([str(binary), *args], env=environment(home),
                                           text=True, capture_output=True, timeout=30)
        url = f"http://127.0.0.1:{server.server_port}/search"
        healthy = run("doctor")
        check(healthy.returncode == 0, "healthy doctor failed:\n" + healthy.stdout + healthy.stderr)
        check(healthy.stdout.count("[ok]") == 3 and "9.9" in healthy.stdout and
              "fetch uses get" in healthy.stdout and "1 results" in healthy.stdout,
              "healthy doctor output:\n" + healthy.stdout)
        check(doctor_line(healthy.stdout, "SearXNG") ==
              f'  [ok]  SearXNG: ready · {url} (searxng_url) · SearXNG 2026.10.25+54ca1d9cf · '
              '1 results for "batbox"', "ready line:\n" + healthy.stdout)
        state["version"] = None
        unversioned = run("doctor")
        check(unversioned.returncode == 0 and doctor_line(unversioned.stdout, "SearXNG") ==
              f'  [ok]  SearXNG: ready · {url} (searxng_url) · 1 results for "batbox"',
              "ready line without /config:\n" + unversioned.stdout)
        state["version"] = "2026.10.25+54ca1d9cf"
        state["search"] = (200, "application/json", json.dumps({
            "query": "batbox", "number_of_results": None, "suggestions": None, "results": [
                {"url": "https://a.example", "title": None, "content": None, "engine": None,
                 "publishedDate": None}, {"title": "no url"}, None]}))
        nulls = run("doctor")
        check(nulls.returncode == 0 and doctor_line(nulls.stdout, "SearXNG").endswith(
              '· SearXNG 2026.10.25+54ca1d9cf · 1 results for "batbox"'),
              "null fields failed the doctor:\n" + nulls.stdout)
        mismatches = {
            "not json": ((200, "text/html", "<html><body>Sign in</body></html>"),
                         f'  [--]  SearXNG: shape mismatch · {url} (searxng_url) · expected a JSON '
                         'body, got HTTP 200 (text/html): "<html><body>Sign in</body></html>"\n'
                         '        fix: check that searxng_url names a SearXNG /search endpoint with '
                         'json in search.formats, or run: batbox setup web'),
            "no results": ((200, "application/json", json.dumps({"query": "batbox", "answers": []})),
                           f'  [--]  SearXNG: shape mismatch · {url} (searxng_url) · expected a JSON '
                           'object with a "results" array, got an object without "results" (keys: '
                           'query, answers)\n        fix: this SearXNG answers in a shape Batbox does '
                           'not read; run: batbox setup web for the tested version, or check that '
                           'searxng_url names a SearXNG /search endpoint'),
            "no url": ((200, "application/json", json.dumps({"results": [{"link": "https://a.example"}]})),
                       f'  [--]  SearXNG: shape mismatch · {url} (searxng_url) · expected each result '
                       'to have a "url" string, got 1 result without one (first result: keys link)\n'
                       '        fix: this SearXNG answers in a shape Batbox does not read; run: batbox '
                       'setup web for the tested version, or check that searxng_url names a SearXNG '
                       '/search endpoint'),
            "server error": ((502, "text/plain", "Bad Gateway"),
                             f'  [--]  SearXNG: shape mismatch · {url} (searxng_url) · expected HTTP 200 '
                             'with JSON search results, got HTTP 502 (text/plain): "Bad Gateway"\n'
                             '        fix: check that searxng_url names the /search endpoint of a '
                             'running SearXNG, or run: batbox setup web'),
        }
        for name, (answer, expected) in mismatches.items():
            state["search"] = answer
            result = run("doctor")
            check(result.returncode == 1 and doctor_line(result.stdout, "SearXNG") == expected,
                  f"{name} doctor line:\n" + result.stdout)
        state["search"] = GOOD_SEARCH
        advertised.write_text(json.dumps(["browser_open", "browser_fetch"]))
        incompatible = run("doctor")
        check(incompatible.returncode == 1 and "Scrapling: incompatible" in incompatible.stdout and
              "Scrapling 9.9 has no tool named make_request or get (it offers: browser_open, "
              "browser_fetch); Batbox needs Scrapling >= 0.3" in incompatible.stdout and
              "fix: batbox setup web" in incompatible.stdout,
              "incompatible doctor output:\n" + incompatible.stdout)
        advertised.write_text(json.dumps(SCRAPLING_047_TOOLS))
        state["search"] = (403, "text/html; charset=utf-8",
                           "<!DOCTYPE html><html><head><title>403 Forbidden</title></head></html>")
        blocked = run("--doctor")
        check(blocked.returncode == 1 and "JSON off" in blocked.stdout and "search.formats" in
              blocked.stdout, "403 doctor output:\n" + blocked.stdout)
        check(doctor_line(blocked.stdout, "SearXNG") ==
              f'  [--]  SearXNG: JSON off · {url} (searxng_url) · expected HTTP 200 with JSON search '
              'results, got HTTP 403 (text/html; charset=utf-8): "<!DOCTYPE html><html><head><title>'
              '403 Forbidden</title></head></html>"\n        fix: SearXNG answers 403 when '
              'search.formats in its settings.yml lacks json; add json there and restart it, or run: '
              'batbox setup web', "403 doctor line:\n" + blocked.stdout)
        empty = base / "empty doctor home"
        empty.mkdir()
        bare = subprocess.run([str(binary), "doctor"], env=environment(empty, PATH="/usr/bin:/bin"),
                              text=True, capture_output=True, timeout=30)
        check(bare.returncode == 1 and "batbox setup web" in bare.stdout and
              "none configured" in bare.stdout, "empty doctor output:\n" + bare.stdout)
    finally:
        server.shutdown()
        server.server_close()


def main():
    if len(sys.argv) != 2:
        sys.exit("usage: setup_web_script.py BATBOX_BINARY")
    binary = Path(sys.argv[1]).resolve(strict=True)
    with tempfile.TemporaryDirectory(prefix="batbox-setup-web-") as temporary:
        base = Path(temporary)
        merge_cases(base)
        dry_runs(base, binary)
        doctor(base, binary)
    print("setup-web merge, dry runs, embedded script and doctor passed")


if __name__ == "__main__":
    main()

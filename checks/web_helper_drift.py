#!/usr/bin/env python3
"""Check Batbox against real Scrapling and SearXNG releases.

Each end installs a SearXNG commit and a Scrapling release into a work
directory, starts SearXNG on a free local port with JSON output on, then runs
`batbox doctor`, a fetch of https://example.com through the Scrapling
connector and a search through the SearXNG connector.

  newest  SearXNG master and the newest Scrapling on PyPI
  pinned  the SearXNG commit gadgets/setup-web.sh pins and the minimum
          Scrapling it accepts, with dependencies as of that release

Run both (the default), or one custom end with --searxng/--scrapling/
--scrapling-command. Needs uv and git; nothing outside --work is changed.

  python3 checks/web_helper_drift.py --batbox build/indigenous/batbox \\
      --connectors build/indigenous/batbox-connectors
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
import datetime
import json
import os
from pathlib import Path
import re
import secrets
import shutil
import signal
import socket
import subprocess
import sys
import tarfile
import tempfile
import threading
import time
import urllib.error
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
SETUP_WEB = ROOT / "gadgets" / "setup-web.sh"
SEARXNG_REPOSITORY = "https://github.com/searxng/searxng"
FETCH_URL = "https://example.com"
SEARCH_TERM = "Python programming language"
INSTALL_TIMEOUT = 900
# Scrapling 0.3.0's command line imports IPython, which only its shell extra
# installs; without it `scrapling mcp` never started, even when it was new.
EXTRA_EXTRAS = {"0.3": ["shell"]}

print_lock = threading.Lock()


class DriftFailure(Exception):
    pass


def say(text=""):
    with print_lock:
        print(text, flush=True)


def setup_web_value(name):
    match = re.search(rf"^{name}=(\S+)$", SETUP_WEB.read_text(), re.MULTILINE)
    if not match:
        raise DriftFailure(f"{SETUP_WEB} has no {name}=")
    return match.group(1)


def setup_web_engines():
    text = SETUP_WEB.read_text()
    block = re.search(r"^    keep_only:\n((?:      - \S+\n)+)", text, re.MULTILINE)
    if not block:
        raise DriftFailure(f"{SETUP_WEB} has no engines keep_only list")
    return re.findall(r"- (\S+)", block.group(1))


def run(command, log, timeout, env=None, cwd=None):
    with open(log, "a", encoding="utf-8") as output:
        output.write(f"$ {' '.join(map(str, command))}\n")
        output.flush()
        try:
            result = subprocess.run([str(part) for part in command], stdout=output,
                                    stderr=subprocess.STDOUT, env=env, cwd=cwd, timeout=timeout)
        except subprocess.TimeoutExpired:
            raise DriftFailure(f"{Path(str(command[0])).name} timed out after {timeout} s; see {log}")
    if result.returncode != 0:
        raise DriftFailure(f"{' '.join(map(str, command[:4]))} ... exited {result.returncode}:\n"
                           + tail(log, 25))


def tail(path, lines):
    try:
        text = Path(path).read_text(encoding="utf-8", errors="replace").splitlines()
    except OSError:
        return ""
    return "\n".join(text[-lines:])


def http_get(url, timeout):
    request = urllib.request.Request(url, headers={"User-Agent": "batbox-web-helper-drift"})
    with urllib.request.urlopen(request, timeout=timeout) as response:
        return response.status, response.read()


def free_port():
    with socket.socket() as probe:
        probe.bind(("127.0.0.1", 0))
        return probe.getsockname()[1]


def master_commit():
    result = subprocess.run(["git", "ls-remote", SEARXNG_REPOSITORY + ".git", "refs/heads/master"],
                            capture_output=True, text=True, timeout=60)
    match = re.match(r"([0-9a-f]{40})\s", result.stdout)
    if result.returncode != 0 or not match:
        raise DriftFailure("cannot read SearXNG master: " + (result.stderr or result.stdout).strip())
    return match.group(1)


def pypi(path):
    status, body = http_get(f"https://pypi.org/pypi/{path}/json", 30)
    if status != 200:
        raise DriftFailure(f"PyPI answered HTTP {status} for {path}")
    return json.loads(body)


def newest_scrapling():
    return pypi("scrapling")["info"]["version"]


def release_cutoff(version):
    """One day after the release's first upload: the dependencies its users got."""
    files = pypi(f"scrapling/{version}")["urls"]
    if not files:
        raise DriftFailure(f"Scrapling {version} has no files on PyPI")
    uploaded = min(datetime.datetime.fromisoformat(item["upload_time_iso_8601"].replace("Z", "+00:00"))
                   for item in files)
    return (uploaded + datetime.timedelta(days=1)).strftime("%Y-%m-%dT%H:%M:%SZ")


class End:
    def __init__(self, name, work, searxng, scrapling, scrapling_command):
        self.name = name
        self.root = work / name
        self.log = self.root / "install.log"
        self.searxng_choice = searxng
        self.scrapling_choice = scrapling
        self.scrapling_command = Path(scrapling_command) if scrapling_command else None
        self.scrapling = None
        self.commit = ""
        self.commit_date = ""
        self.scrapling_version = ""
        self.port = 0
        self.process = None
        self.lines = []

    def note(self, text):
        self.lines.append(text)
        say(f"  [{self.name}] {text}")

    # ------------------------------------------------------------ SearXNG
    def install_searxng(self):
        choice = self.searxng_choice
        if choice == "pinned":
            self.commit = setup_web_value("SEARXNG_COMMIT")
        elif choice == "master":
            self.commit = master_commit()
        elif re.fullmatch(r"[0-9a-f]{40}", choice):
            self.commit = choice
        else:
            raise DriftFailure(f"--searxng takes master, pinned or a 40-character commit, not {choice}")
        started = time.monotonic()
        download = self.root / "download"
        download.mkdir(parents=True)
        archive = download / "searxng.tar.gz"
        try:
            status, body = http_get(f"{SEARXNG_REPOSITORY}/archive/{self.commit}.tar.gz", 120)
        except urllib.error.URLError as error:
            raise DriftFailure(f"cannot download SearXNG {self.commit[:12]}: {error}")
        if status != 200:
            raise DriftFailure(f"SearXNG archive answered HTTP {status}")
        archive.write_bytes(body)
        tree = self.root / "tree"
        tree.mkdir()
        with tarfile.open(archive) as bundle:
            members = bundle.getmembers()
            # GitHub stamps archive entries with the commit time.
            stamp = max(member.mtime for member in members)
            bundle.extractall(tree, filter="data")
        tops = list(tree.iterdir())
        if len(tops) != 1 or not tops[0].is_dir():
            raise DriftFailure("unexpected SearXNG archive layout")
        source = self.root / "src"
        tops[0].rename(source)
        shutil.rmtree(tree)
        shutil.rmtree(download)
        date = datetime.datetime.fromtimestamp(stamp, datetime.timezone.utc)
        self.commit_date = date.strftime("%Y-%m-%d")
        version = f"{date.year}.{date.month}.{date.day}+{self.commit[:9]}"
        # Same frozen version file gadgets/setup-web.sh writes.
        (source / "searx" / "version_frozen.py").write_text(
            f'VERSION_STRING = "{version}"\nVERSION_TAG = "{version}"\n'
            f'DOCKER_TAG = "{version.replace("+", "-")}"\n'
            f'GIT_URL = "{SEARXNG_REPOSITORY}"\nGIT_BRANCH = "master"\n')
        venv = self.root / "searxng-venv"
        run(["uv", "venv", "--quiet", "--seed", "--python", "3.12", venv], self.log, 300)
        pip = [venv / "bin" / "python", "-m", "pip", "install", "--quiet", "--disable-pip-version-check"]
        run([*pip, "-U", "pip"], self.log, INSTALL_TIMEOUT)
        run([*pip, "-U", "setuptools", "wheel", "pyyaml", "msgspec", "typing-extensions", "pybind11"],
            self.log, INSTALL_TIMEOUT)
        run([*pip, "--use-pep517", "--no-build-isolation", "-e", source], self.log, INSTALL_TIMEOUT)
        self.note(f"SearXNG {version} ({choice}, commit {self.commit[:12]} from {self.commit_date}) "
                  f"installed in {time.monotonic() - started:.0f} s")

    def start_searxng(self):
        self.port = free_port()
        settings = self.root / "settings.yml"
        engines = "".join(f"      - {engine}\n" for engine in setup_web_engines())
        settings.write_text(
            "use_default_settings:\n  engines:\n    keep_only:\n" + engines +
            "general:\n  debug: false\n  instance_name: \"Batbox drift check\"\n"
            f"server:\n  port: {self.port}\n  bind_address: \"127.0.0.1\"\n"
            f"  secret_key: \"{secrets.token_hex(32)}\"\n  limiter: false\n  public_instance: false\n"
            "search:\n  formats:\n    - html\n    - json\n")
        env = dict(os.environ, SEARXNG_SETTINGS_PATH=str(settings), PYTHONUNBUFFERED="1")
        output = open(self.root / "searxng.log", "ab")
        self.process = subprocess.Popen([str(self.root / "searxng-venv" / "bin" / "searxng-run")],
                                        cwd=self.root / "src", env=env, stdout=output,
                                        stderr=subprocess.STDOUT, start_new_session=True)
        output.close()
        url = f"http://127.0.0.1:{self.port}/search"
        deadline = time.monotonic() + 180
        last = ""
        while time.monotonic() < deadline:
            if self.process.poll() is not None:
                raise DriftFailure(f"SearXNG exited with {self.process.returncode}:\n"
                                   + tail(self.root / "searxng.log", 30))
            try:
                status, body = http_get(f"{url}?q=batbox&format=json", 20)
                if status == 200 and isinstance(json.loads(body).get("results"), list):
                    self.note(f"SearXNG answers JSON at {url}")
                    return url
                last = f"HTTP {status}"
            except (OSError, ValueError) as error:
                last = str(error)
            time.sleep(2)
        raise DriftFailure(f"SearXNG did not answer a JSON search in 180 s ({last}):\n"
                           + tail(self.root / "searxng.log", 30))

    def stop_searxng(self):
        if self.process and self.process.poll() is None:
            try:
                os.killpg(self.process.pid, signal.SIGTERM)
                self.process.wait(timeout=15)
            except (ProcessLookupError, subprocess.TimeoutExpired):
                try:
                    os.killpg(self.process.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
                self.process.wait(timeout=15)

    # ----------------------------------------------------------- Scrapling
    def install_scrapling(self):
        if self.scrapling_command:
            self.scrapling = self.scrapling_command
            self.scrapling_version = installed_version(self.scrapling) or "unknown version"
            self.note(f"Scrapling {self.scrapling_version} at {self.scrapling} (already installed)")
            return
        choice = self.scrapling_choice
        if choice == "newest":
            version, cutoff = newest_scrapling(), None
        else:
            version = setup_web_value("SCRAPLING_MINIMUM") if choice == "minimum" else choice
            cutoff = release_cutoff(version)
        extras = ",".join(["ai", *EXTRA_EXTRAS.get(version, [])])
        venv = self.root / "scrapling-venv"
        run(["uv", "venv", "--quiet", "--python", "3.12", venv], self.log, 300)
        command = ["uv", "pip", "install", "--quiet", "--python", venv / "bin" / "python"]
        if cutoff:
            command += ["--exclude-newer", cutoff]
        run([*command, f"scrapling[{extras}]=={version}"], self.log, INSTALL_TIMEOUT)
        self.scrapling = venv / "bin" / "scrapling"
        self.scrapling_version = installed_version(self.scrapling) or version
        resolved = f", dependencies as of {cutoff}" if cutoff else ""
        self.note(f"Scrapling {self.scrapling_version} ({choice}{resolved}) installed")

    # -------------------------------------------------------------- checks
    def environment(self, url):
        home = self.root / "home"
        config = self.root / "batbox-config"
        home.mkdir(exist_ok=True)
        config.mkdir(exist_ok=True)
        env = {key: value for key, value in os.environ.items() if not key.startswith("BATBOX_")}
        env.update(HOME=str(home), BATBOX_CONFIG_DIR=str(config),
                   BATBOX_SCRAPLING_COMMAND=str(self.scrapling), BATBOX_SEARXNG_URL=url,
                   NO_PROXY="127.0.0.1,localhost", no_proxy="127.0.0.1,localhost")
        return env

    def fetch(self, connectors, env):
        wrapper = self.root / "scrapling-mcp"
        wrapper.write_text(f"#!/bin/sh\nexec '{self.scrapling}' mcp \"$@\"\n")
        wrapper.chmod(0o755)
        result = subprocess.run([connectors, "fetch", FETCH_URL], capture_output=True, text=True,
                                timeout=240, env=dict(env, BATBOX_SCRAPLING_EXECUTABLE=str(wrapper),
                                                      BATBOX_CONNECTOR_TIMEOUT_MS="180000"))
        if result.returncode != 0:
            raise DriftFailure(f"fetch {FETCH_URL} failed: {result.stderr.strip()[-2000:]}")
        try:
            reply = json.loads(result.stdout)
            if reply.get("isError"):
                raise DriftFailure(f"Scrapling returned a tool error: {result.stdout[:500]}")
            page = reply.get("structuredContent") or json.loads(reply["content"][0]["text"])
            status, content = page.get("status"), page.get("content")
        except (ValueError, KeyError, IndexError, TypeError, AttributeError):
            raise DriftFailure(f"fetch {FETCH_URL} returned an unexpected result: {result.stdout[:500]}")
        text = "".join(content) if isinstance(content, list) else str(content or "")
        if status != 200 or not text.strip():
            raise DriftFailure(f"fetch {FETCH_URL} returned status {status} and "
                               f"{len(text.strip())} characters")
        self.note(f"fetch {FETCH_URL}: HTTP {status}, {len(text.strip())} characters")

    def search(self, connectors, env):
        last = ""
        for attempt in range(4):
            result = subprocess.run([connectors, "search", SEARCH_TERM], capture_output=True,
                                    text=True, timeout=150, env=env)
            if result.returncode == 0:
                try:
                    hits = json.loads(result.stdout)["results"][0]["content"]
                except (ValueError, KeyError, IndexError, TypeError):
                    raise DriftFailure(f"search returned an unexpected result: {result.stdout[:500]}")
                if hits:
                    self.note(f"search \"{SEARCH_TERM}\": {len(hits)} results, first {hits[0]['url']}")
                    return
                last = "0 results"
            else:
                last = result.stderr.strip()
                if "SearXNG at" in last:
                    raise DriftFailure(f"search failed: {last}")
            time.sleep(5 * (attempt + 1))
        raise DriftFailure(f"search \"{SEARCH_TERM}\" found nothing after 4 tries ({last})")

    def doctor(self, batbox, env):
        result = subprocess.run([batbox, "doctor"], capture_output=True, text=True, timeout=120,
                                env=env)
        lines = result.stdout.splitlines()
        rows = {name: next((line.strip() for line in lines if f"]  {name}:" in line), "")
                for name in ("Scrapling", "SearXNG")}
        for name, row in rows.items():
            self.note(f"doctor: {row or name + ' missing from the output'}")
        failed = [name for name, row in rows.items() if not row.startswith(f"[ok]  {name}: ready")]
        if failed:
            raise DriftFailure(f"batbox doctor did not report {' and '.join(failed)} ready:\n"
                               + result.stdout + result.stderr)

    def check(self, batbox, connectors):
        url = self.start_searxng()
        env = self.environment(url)
        self.fetch(connectors, env)
        self.search(connectors, env)
        self.doctor(batbox, env)


def installed_version(executable):
    script = ("import sys\nout, sys.stdout = sys.stdout, sys.stderr\n"
              "import importlib.metadata as m\nout.write(m.version('scrapling'))\n")
    try:
        first = Path(executable).read_text(errors="replace").splitlines()[0]
    except (OSError, IndexError):
        return ""
    if not first.startswith("#!"):
        return ""
    interpreter = first[2:].strip().split()[0]
    try:
        result = subprocess.run([interpreter, "-I", "-c", script], capture_output=True, text=True,
                                timeout=60)
    except (OSError, subprocess.TimeoutExpired):
        return ""
    return result.stdout.strip() if result.returncode == 0 else ""


def prepare(end):
    end.root.mkdir(parents=True)
    with ThreadPoolExecutor(max_workers=2) as pool:
        jobs = [pool.submit(end.install_searxng), pool.submit(end.install_scrapling)]
        for job in jobs:
            job.result()


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--batbox", required=True, type=Path, help="the batbox executable")
    parser.add_argument("--connectors", required=True, type=Path,
                        help="the batbox-connectors executable (built with BUILD_TESTING=ON)")
    parser.add_argument("--work", type=Path, help="work directory (default: a new temporary one)")
    parser.add_argument("--ends", default="newest,pinned", help="ends to check (newest,pinned)")
    parser.add_argument("--searxng", help="custom end: master, pinned or a commit")
    parser.add_argument("--scrapling", help="custom end: newest, minimum or a version")
    parser.add_argument("--scrapling-command", type=Path,
                        help="custom end: an installed scrapling executable")
    parser.add_argument("--keep", action="store_true", help="keep the work directory")
    args = parser.parse_args()

    for tool in ("uv", "git"):
        if not shutil.which(tool):
            sys.exit(f"web_helper_drift: {tool} is required")
    batbox = args.batbox.resolve(strict=True)
    connectors = args.connectors.resolve(strict=True)
    owned = args.work is None
    work = Path(tempfile.mkdtemp(prefix="batbox-drift-")) if owned else args.work.resolve()
    if not owned:
        work.mkdir(parents=True, exist_ok=True)
        if any(work.iterdir()):
            sys.exit(f"web_helper_drift: {work} is not empty")

    if args.searxng or args.scrapling or args.scrapling_command:
        ends = [End("custom", work, args.searxng or "pinned", args.scrapling or "newest",
                    args.scrapling_command)]
    else:
        plans = {"newest": ("master", "newest"), "pinned": ("pinned", "minimum")}
        names = [name.strip() for name in args.ends.split(",") if name.strip()]
        unknown = [name for name in names if name not in plans]
        if unknown or not names:
            sys.exit(f"web_helper_drift: unknown end {unknown or args.ends!r}; use newest,pinned")
        ends = [End(name, work, *plans[name], None) for name in names]

    say(f"Batbox web helper drift check · {batbox}")
    say(f"work directory: {work}")
    failures = {}
    started = time.monotonic()
    try:
        with ThreadPoolExecutor(max_workers=len(ends)) as pool:
            prepared = {end.name: pool.submit(prepare, end) for end in ends}
            for end in ends:
                try:
                    prepared[end.name].result()
                except Exception as error:  # noqa: BLE001 - reported per end
                    failures[end.name] = f"install: {error}"
        for end in ends:
            if end.name in failures:
                continue
            say(f"\n== {end.name}")
            try:
                end.check(batbox, connectors)
            except Exception as error:  # noqa: BLE001 - reported per end
                failures[end.name] = str(error)
            finally:
                end.stop_searxng()
    finally:
        for end in ends:
            end.stop_searxng()
        if owned and not args.keep:
            shutil.rmtree(work, ignore_errors=True)

    say(f"\nSummary ({time.monotonic() - started:.0f} s)")
    for end in ends:
        state = "FAIL" if end.name in failures else "ok"
        say(f"  {state:4}  {end.name}: SearXNG {end.searxng_choice} "
            f"{end.commit[:12] or '?'} ({end.commit_date or 'not installed'}) · "
            f"Scrapling {end.scrapling_version or end.scrapling_choice}")
    for name, reason in failures.items():
        say(f"\n{name} failed: {reason}")
    if failures:
        sys.exit(1)
    say("\nWeb helper drift check passed.")


if __name__ == "__main__":
    main()

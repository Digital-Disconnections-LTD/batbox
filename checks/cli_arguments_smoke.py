#!/usr/bin/env python3
"""A word on the command line reaches the model only through -p or --prompt.

`batbox version` and `batbox help` answer locally. An unknown first word such
as `batbox update` exits 2 with the usage and a hint, and a stray word after
options is refused; none of them sends a request to the fake model server.
`-p "text"`, `-p` with piped stdin, and `--prompt "text"` in the terminal each
reach it.
"""
import errno
import fcntl
import json
import os
import pty
import re
import select
import signal
import struct
import subprocess
import sys
import tempfile
import termios
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path


def check(condition, message):
    if not condition:
        raise SystemExit("FAIL: " + message)


def stream(model, text):
    rows = [
        {"type": "message_start", "message": {"id": "cli-arguments", "type": "message",
         "role": "assistant", "model": model, "content": [], "stop_reason": None,
         "usage": {"input_tokens": 5, "output_tokens": 0}}},
        {"type": "content_block_start", "index": 0, "content_block": {"type": "text", "text": ""}},
        {"type": "content_block_delta", "index": 0, "delta": {"type": "text_delta", "text": text}},
        {"type": "content_block_stop", "index": 0},
        {"type": "message_delta", "delta": {"stop_reason": "end_turn"}, "usage": {"output_tokens": 3}},
        {"type": "message_stop"},
    ]
    return "".join(f"event: {r['type']}\ndata: {json.dumps(r)}\n\n" for r in rows).encode()


def main(binary):
    requests = []

    class Handler(BaseHTTPRequestHandler):
        def log_message(self, *_):
            pass

        def do_GET(self):
            requests.append({"method": "GET", "path": self.path, "body": None})
            self.send_error(404)

        def do_POST(self):
            body = json.loads(self.rfile.read(int(self.headers["Content-Length"])))
            requests.append({"method": "POST", "path": self.path, "body": body})
            response = stream(body.get("model", ""), f"CLI_REPLY_{len(requests)}")
            self.send_response(200)
            self.send_header("Content-Type", "text/event-stream")
            self.send_header("Content-Length", str(len(response)))
            self.end_headers()
            self.wfile.write(response)

    server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    try:
        with tempfile.TemporaryDirectory(prefix="batbox-cli-arguments-") as temporary:
            root = Path(temporary)
            config = root / "config"
            project = root / "project"
            config.mkdir()
            project.mkdir()
            (config / "native-client.json").write_text(json.dumps({
                "model": "cli-fixture", "base_url": f"http://127.0.0.1:{server.server_port}",
                "state_directory": str(root / "state"), "mcpServers": {}, "auto_title": False,
                "thinking": False}))
            env = {key: value for key, value in os.environ.items()
                   if not key.startswith("BATBOX_") and key not in {"TMUX", "STY", "SSH_CONNECTION"}}
            env.update(TERM="xterm-256color", BATBOX_CONFIG_DIR=str(config), HOME=str(root),
                       NO_PROXY="127.0.0.1,localhost", no_proxy="127.0.0.1,localhost")

            def run(*arguments, stdin=None):
                return subprocess.run([str(binary), *arguments], cwd=project, env=env,
                                      input=stdin, stdin=None if stdin is not None else subprocess.DEVNULL,
                                      capture_output=True, text=True, timeout=60)

            version = run("--version")
            lines = version.stdout.splitlines()
            check(version.returncode == 0 and len(lines) == 3 and
                  re.fullmatch(r"batbox \d+\.\d+\.\d+", lines[0]) is not None and
                  re.fullmatch(r"commit ([0-9a-f]{7,40}|unknown)", lines[1]) is not None and
                  re.fullmatch(r"built (\d{4}-\d{2}-\d{2}|unknown)", lines[2]) is not None,
                  "--version: " + version.stdout + version.stderr)
            alias = run("version")
            check(alias.returncode == 0 and alias.stdout == version.stdout and not alias.stderr,
                  "batbox version: " + alias.stdout + alias.stderr)

            usage = run("--help")
            check(usage.returncode == 0 and "--prompt TEXT" in usage.stdout and
                  "Usage: batbox [options]" in usage.stdout and not usage.stderr,
                  "--help: " + usage.stdout + usage.stderr)
            for arguments in (["help"], ["-h"]):
                same = run(*arguments)
                check(same.returncode == 0 and same.stdout == usage.stdout and not same.stderr,
                      f"{arguments}: {same.stdout}{same.stderr}")

            for arguments in (["update"], ["upgrade", "now"], ["Fix the failing test"],
                              ["hello", "-p"]):
                refused = run(*arguments)
                lines = refused.stderr.splitlines()
                check(refused.returncode == 2 and not refused.stdout and lines and
                      lines[0] == f'batbox: unknown command "{arguments[0]}"' and
                      "Usage: batbox [options]" in refused.stderr and
                      'batbox -p "your prompt"' in refused.stderr and
                      "--prompt" in refused.stderr,
                      f"{arguments}: exit {refused.returncode}: {refused.stdout}{refused.stderr}")

            for arguments in (["--inline", "hello"], ["--", "hello"],
                              ["--resume", "parser", "next step"]):
                stray = run(*arguments)
                check(stray.returncode != 0 and "Unexpected argument" in stray.stderr and
                      "--prompt" in stray.stderr,
                      f"{arguments}: exit {stray.returncode}: {stray.stdout}{stray.stderr}")

            check(not requests, f"a refused command line reached the model server: {requests}")

            def turns():
                return [r for r in requests if r["method"] == "POST" and r["path"] == "/v1/messages"]

            def sent(text):
                return any(text in json.dumps(r["body"].get("messages", [])) for r in turns())

            printed = run("-p", "Say the print marker")
            check(printed.returncode == 0 and "CLI_REPLY_" in printed.stdout and
                  sent("Say the print marker"),
                  f"-p: exit {printed.returncode}: {printed.stdout}{printed.stderr}")
            before = len(turns())
            piped = run("--cwd", str(project), "-p", stdin="Prompt from a pipe\n")
            check(piped.returncode == 0 and "CLI_REPLY_" in piped.stdout and
                  len(turns()) == before + 1 and sent("Prompt from a pipe"),
                  f"-p with stdin: exit {piped.returncode}: {piped.stdout}{piped.stderr}")

            before = len(turns())
            interactive(binary, project, env, "Opened with a prompt flag")
            check(len(turns()) == before + 1 and sent("Opened with a prompt flag"),
                  f"--prompt did not reach the model server: {len(turns())} turns")
    finally:
        server.shutdown()
        server.server_close()
    print("version and help answered locally; unknown commands and stray words exited "
          "without a request; -p, piped stdin and --prompt reached the fake server")


def interactive(binary, project, env, text):
    pid, master = pty.fork()
    if pid == 0:
        fcntl.ioctl(0, termios.TIOCSWINSZ, struct.pack("HHHH", 30, 120, 0, 0))
        os.execve(str(binary), [str(binary), "--cwd", str(project), "--prompt", text], env)
    output = bytearray()
    state = {"status": None}

    def drain(seconds):
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline and state["status"] is None:
            if select.select([master], [], [], 0.02)[0]:
                try:
                    output.extend(os.read(master, 65536))
                except OSError as error:
                    if error.errno != errno.EIO:
                        raise
            finished, result = os.waitpid(pid, os.WNOHANG)
            if finished:
                state["status"] = result

    def plain():
        return re.sub(rb"\x1b\[[0-9;?]*[A-Za-z]|\x1b\][^\x07]*\x07", b"", bytes(output))

    try:
        deadline = time.monotonic() + 25
        while b"CLI_REPLY_" not in plain() and state["status"] is None and \
                time.monotonic() < deadline:
            drain(0.05)
        check(b"CLI_REPLY_" in plain(),
              f"--prompt reply never appeared; exit={state['status']}; "
              f"tail={plain()[-1500:].decode(errors='replace')}")
        drain(0.3)
        os.write(master, b"/exit\r")
        deadline = time.monotonic() + 10
        while state["status"] is None and time.monotonic() < deadline:
            drain(0.05)
        check(state["status"] is not None and os.waitstatus_to_exitcode(state["status"]) == 0,
              f"--prompt session did not exit cleanly: {state['status']}")
    finally:
        if state["status"] is None:
            os.kill(pid, signal.SIGKILL)
            os.waitpid(pid, 0)
        os.close(master)


if __name__ == "__main__":
    main(Path(sys.argv[1]).resolve(strict=True))

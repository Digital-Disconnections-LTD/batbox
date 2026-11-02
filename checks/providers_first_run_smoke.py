#!/usr/bin/env python3
"""First run with nothing configured.

--print: Batbox exits 1 with a one-line hint naming `batbox providers`.
Interactive: /providers opens by itself; a prompt sent before connecting is
refused with a pointer back to /providers; picking llama.cpp, pointing it at a
local fake server, testing the connection and choosing a model saves an
endpoint (mode 0600), and the next prompt reaches that server.
"""
import errno
import fcntl
import json
import os
import pty
import re
import select
import struct
import subprocess
import sys
import tempfile
import termios
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

REPLY = "FIRST_RUN_REPLY_OK"


def stream(model):
    rows = [
        {"type": "message_start", "message": {"id": "first-run", "type": "message",
         "role": "assistant", "model": model, "content": [], "stop_reason": None,
         "usage": {"input_tokens": 5, "output_tokens": 0}}},
        {"type": "content_block_start", "index": 0, "content_block": {"type": "text", "text": ""}},
        {"type": "content_block_delta", "index": 0, "delta": {"type": "text_delta", "text": REPLY}},
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
            if self.path != "/v1/models":
                self.send_error(404)
                return
            body = json.dumps({"data": [{"id": "fixture-coder"}, {"id": "fixture-small"}]}).encode()
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)

        def do_POST(self):
            body = json.loads(self.rfile.read(int(self.headers["Content-Length"])))
            requests.append({"path": self.path, "body": body})
            response = stream(body.get("model", ""))
            self.send_response(200)
            self.send_header("Content-Type", "text/event-stream")
            self.send_header("Content-Length", str(len(response)))
            self.end_headers()
            self.wfile.write(response)

    server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    url = f"http://127.0.0.1:{server.server_port}"
    with tempfile.TemporaryDirectory(prefix="batbox-first-run-") as temporary:
        root = Path(temporary)
        config = root / "config"
        config.mkdir()
        (root / "project").mkdir()
        (config / "settings.json").write_text(json.dumps({
            "state_directory": str(root / "state"), "mcpServers": {}, "auto_title": False}))
        env = {key: value for key, value in os.environ.items() if not key.startswith("BATBOX_")}
        env.update(TERM="xterm-256color", BATBOX_CONFIG_DIR=str(config), HOME=str(root),
                   NO_PROXY="127.0.0.1,localhost", no_proxy="127.0.0.1,localhost")

        printed = subprocess.run([str(binary), "--cwd", str(root / "project"), "--print", "hi"],
                                 env=env, stdin=subprocess.DEVNULL, capture_output=True,
                                 text=True, timeout=30)
        if printed.returncode != 1 or "batbox providers" not in printed.stderr or \
                printed.stderr.count("\n") != 1:
            raise SystemExit(f"--print first run: exit {printed.returncode}: {printed.stderr!r}")

        pid, master = pty.fork()
        if pid == 0:
            fcntl.ioctl(0, termios.TIOCSWINSZ, struct.pack("HHHH", 40, 110, 0, 0))
            os.execve(str(binary), [str(binary), "--cwd", str(root / "project")], env)
        output = bytearray()
        status = None

        def drain(seconds):
            nonlocal status
            deadline = time.monotonic() + seconds
            while time.monotonic() < deadline and status is None:
                if select.select([master], [], [], 0.02)[0]:
                    try:
                        output.extend(os.read(master, 65536))
                    except OSError as error:
                        if error.errno != errno.EIO:
                            raise
                finished, result = os.waitpid(pid, os.WNOHANG)
                if finished:
                    status = result

        def plain():
            return re.sub(rb"\x1b\[[0-9;?]*[A-Za-z]|\x1b\][^\x07]*\x07", b"", bytes(output))

        def wait_for(needle, label, since=0):
            deadline = time.monotonic() + 25
            while needle not in plain()[since:] and status is None and time.monotonic() < deadline:
                drain(0.05)
            if needle not in plain()[since:]:
                raise RuntimeError(f"Timed out waiting for {label}; exit={status}; "
                                   f"tail={plain()[-2500:].decode(errors='replace')}")

        def mark():
            return len(plain())

        def keys(data, pause=0.15):
            os.write(master, data)
            drain(pause)

        try:
            wait_for(b"Connect a model server", "the provider picker on first run")
            wait_for(b"Not connected", "the not-connected welcome notice")
            keys(b"\x1b", 0.4)
            at = mark()
            for ch in "hello":
                keys(ch.encode(), 0.03)
            keys(b"\r", 0.5)
            wait_for(b"Connect a model before sending a prompt", "the refusal before connecting", at)
            wait_for(b"Connect a model server", "the picker reopening", at)
            at = mark()
            keys(b"llama-server", 0.4)
            wait_for(b"llama.cpp (llama-server)", "the llama.cpp catalog row", at)
            # The search leaves catalog rows only; the first match is llama.cpp.
            at = mark()
            keys(b"\r", 0.4)
            wait_for(b"Base URL", "the base URL step", at)
            keys(b"\x15", 0.2)
            at = mark()
            keys(url.encode(), 0.3)
            keys(b"\r", 0.3)
            wait_for(b"fixture-coder", "the live model list", at)
            at = mark()
            keys(b"\r", 0.5)
            wait_for(b"Added llama.cpp", "the saved notice", at)
            wait_for(b"/model to switch", "the saved notice hint", at)
            at = mark()
            for ch in "Say hi":
                keys(ch.encode(), 0.03)
            keys(b"\r", 0.5)
            wait_for(REPLY.encode(), "the reply from the new endpoint", at)
            keys(b"/exit\r", 0.5)
            deadline = time.monotonic() + 15
            while status is None and time.monotonic() < deadline:
                drain(0.1)
        finally:
            if status is None:
                os.kill(pid, 9)
                os.waitpid(pid, 0)
            server.shutdown()
        saved = config / "native-client.json"
        document = json.loads(saved.read_text())
        endpoint = document["endpoints"]["llama.cpp"]
        problems = []
        if document.get("profile") != "llama.cpp::fixture-coder":
            problems.append(f"profile {document.get('profile')}")
        if endpoint["base_url"] != url or endpoint["provider"] != "anthropic" or \
                endpoint["preset"] != "llama.cpp" or endpoint["auth_header"] != "none":
            problems.append(f"endpoint {endpoint}")
        if endpoint["models"][:2] != ["fixture-coder", "fixture-small"]:
            problems.append(f"models {endpoint['models']}")
        if (saved.stat().st_mode & 0o777) != 0o600:
            problems.append(f"mode {oct(saved.stat().st_mode & 0o777)}")
        if not requests or requests[-1]["path"] != "/v1/messages" or \
                requests[-1]["body"].get("model") != "fixture-coder":
            problems.append(f"requests {[r['path'] for r in requests]}")
        if problems:
            raise SystemExit("; ".join(problems))
    print("first run: --print hint, picker on launch, refusal, llama.cpp saved and used")


if __name__ == "__main__":
    main(Path(sys.argv[1]).resolve(strict=True))

#!/usr/bin/env python3
"""Check immediate /effort and /config during an active terminal model turn."""
import errno
import fcntl
import http.server
import json
import os
from pathlib import Path
import pty
import select
import signal
import struct
import sys
import tempfile
import termios
import threading
import time


def main(binary):
    requests = []
    release_first = threading.Event()

    class Handler(http.server.BaseHTTPRequestHandler):
        def log_message(self, *_):
            pass

        def do_POST(self):
            requests.append(json.loads(self.rfile.read(int(self.headers["Content-Length"]))))
            if len(requests) == 1:
                release_first.wait(30)
            events = [
                ("message_start", {"type": "message_start", "message": {
                    "id": "effort", "type": "message", "role": "assistant",
                    "model": "effort-fixture", "content": [], "stop_reason": None,
                    "usage": {"input_tokens": 10, "output_tokens": 0}}}),
                ("content_block_start", {"type": "content_block_start", "index": 0,
                                         "content_block": {"type": "text", "text": ""}}),
                ("content_block_delta", {"type": "content_block_delta", "index": 0,
                    "delta": {"type": "text_delta", "text": "READY"}}),
                ("content_block_stop", {"type": "content_block_stop", "index": 0}),
                ("message_delta", {"type": "message_delta", "delta": {"stop_reason": "end_turn"},
                                   "usage": {"output_tokens": 2}}),
                ("message_stop", {"type": "message_stop"}),
            ]
            body = "".join("event: " + name + "\ndata: " + json.dumps(data) + "\n\n"
                           for name, data in events).encode()
            self.send_response(200)
            self.send_header("Content-Type", "text/event-stream")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)

    with tempfile.TemporaryDirectory(prefix="batbox-effort-") as temporary:
        root = Path(temporary)
        config = root / "config"
        config.mkdir()
        server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        (config / "native-client.json").write_text(json.dumps({
            "model": "effort-fixture", "base_url": f"http://127.0.0.1:{server.server_port}",
            "state_directory": str(root / "state"), "mcpServers": {}, "fullscreen": True, "auto_title": False,
            "max_output_tokens": 50000,
        }))
        pid, master = pty.fork()
        if pid == 0:
            fcntl.ioctl(0, termios.TIOCSWINSZ, struct.pack("HHHH", 24, 100, 0, 0))
            env = {key: value for key, value in os.environ.items()
                   if not key.startswith("BATBOX_")}
            env.update(TERM="xterm-256color", BATBOX_CONFIG_DIR=str(config))
            os.execve(str(binary), [str(binary), "--cwd", str(root)], env)
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

        def wait_for(condition, label):
            deadline = time.monotonic() + 20
            while not condition() and status is None and time.monotonic() < deadline:
                drain(0.05)
            if not condition():
                raise RuntimeError(f"Timed out waiting for {label}; exit={status}; "
                                   f"settings_exists={(config / 'settings.json').exists()}; "
                                   f"terminal_tail={output[-3000:]!r}")

        def send(command):
            os.write(master, command.encode() + b"\r")
            drain(0.1)

        try:
            wait_for(lambda: b"\x1b[?2004h" in output and
                     b"effort-fixture" in output, "terminal startup")
            send("Hold this turn")
            wait_for(lambda: len(requests) == 1, "first Messages request")
            send("/effort low")
            wait_for(lambda: b"Effort level set to low" in output,
                     "effort change during active turn")
            if len(requests) != 1:
                raise RuntimeError("Effort setting unexpectedly started a model request")
            send("/config")
            wait_for(lambda: b"BATBOX  /  CONFIG" in output,
                     "configuration panel during active turn")
            os.write(master, b"\x1b")
            wait_for(lambda: output.rfind(b"BATBOX  /  CONFIG") <
                     output.rfind(b"effort-fixture"), "configuration panel close")
            release_first.set()
            wait_for(lambda: b"READY" in output, "first turn completion")
            send("Say two")
            wait_for(lambda: len(requests) == 2, "second Messages request")
            wait_for(lambda: output.count(b"READY") >= 2, "second turn completion")
            send("/exit")
            wait_for(lambda: status is not None, "clean exit")
            if os.waitstatus_to_exitcode(status) != 0:
                raise RuntimeError(f"Batbox exited with {status}")
            if requests[0].get("output_config", {}).get("effort") is not None:
                raise RuntimeError("First request unexpectedly had an effort override")
            if requests[1].get("output_config", {}).get("effort") != "low":
                raise RuntimeError("Low effort did not reach Messages output_config")
            if requests[0].get("thinking") != requests[1].get("thinking"):
                raise RuntimeError("Server effort changed the separate thinking budget")
            saved = json.loads((config / "settings.json").read_text())
            if saved.get("effortLevel") != "low":
                raise RuntimeError("Effort level was not persisted")
            print("/effort and /config immediate terminal smoke passed")
        finally:
            release_first.set()
            if status is None:
                os.kill(pid, signal.SIGKILL)
                os.waitpid(pid, 0)
            os.close(master)
            server.shutdown()
            server.server_close()
            thread.join(timeout=5)


if __name__ == "__main__":
    main(Path(sys.argv[1]).resolve(strict=True))

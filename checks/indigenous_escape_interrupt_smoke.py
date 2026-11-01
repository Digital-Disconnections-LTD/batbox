#!/usr/bin/env python3
"""Escape cancels an active streamed turn and permits the next prompt."""
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
    slow_release = threading.Event()
    requests = []
    interruption = "[Interrupted the Batbox. What should the model do differently?]"

    class Handler(http.server.BaseHTTPRequestHandler):
        def log_message(self, *_):
            pass

        def do_POST(self):
            body = self.rfile.read(int(self.headers["Content-Length"]))
            requests.append(body)
            slow = len(requests) == 1
            self.send_response(200)
            self.send_header("Content-Type", "text/event-stream")
            self.end_headers()

            def event(name, data):
                payload = ("event: " + name + "\ndata: " + json.dumps(data) +
                           "\n\n").encode()
                self.wfile.write(payload)
                self.wfile.flush()

            try:
                event("message_start", {"type": "message_start", "message": {
                    "id": "slow" if slow else "fast", "type": "message",
                    "role": "assistant", "model": "interrupt-fixture", "content": [],
                    "stop_reason": None,
                    "usage": {"input_tokens": 10, "output_tokens": 0}}})
                event("content_block_start", {"type": "content_block_start",
                      "index": 0, "content_block": {"type": "text", "text": ""}})
                event("content_block_delta", {"type": "content_block_delta",
                      "index": 0, "delta": {"type": "text_delta",
                      "text": "SLOW BEGIN" if slow else "FAST COMPLETE"}})
                if slow:
                    slow_release.wait(12)
                    event("content_block_delta", {"type": "content_block_delta",
                          "index": 0, "delta": {"type": "text_delta",
                          "text": "SLOW END"}})
                event("content_block_stop", {"type": "content_block_stop",
                      "index": 0})
                event("message_delta", {"type": "message_delta",
                      "delta": {"stop_reason": "end_turn"},
                      "usage": {"output_tokens": 5}})
                event("message_stop", {"type": "message_stop"})
            except (BrokenPipeError, ConnectionResetError):
                pass

    with tempfile.TemporaryDirectory(prefix="batbox-escape-") as temporary:
        root = Path(temporary)
        config = root / "config"
        config.mkdir()
        server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        (config / "native-client.json").write_text(json.dumps({
            "model": "interrupt-fixture",
            "base_url": f"http://127.0.0.1:{server.server_port}",
            "state_directory": str(root / "state"), "mcpServers": {},
            "fullscreen": True, "auto_title": False, "max_output_tokens": 50000,
        }))
        pid, master = pty.fork()
        if pid == 0:
            fcntl.ioctl(0, termios.TIOCSWINSZ, struct.pack("HHHH", 24, 100, 0, 0))
            env = {key: value for key, value in os.environ.items()
                   if not key.startswith("BATBOX_") and key not in
                   {"TMUX", "STY", "SSH_CONNECTION"}}
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

        def wait_for(condition, label, seconds=5):
            deadline = time.monotonic() + seconds
            while not condition() and status is None and time.monotonic() < deadline:
                drain(0.05)
            if not condition():
                raise RuntimeError(f"Timed out waiting for {label}; exit={status}; "
                                   f"terminal_tail={output[-1200:]!r}")

        def send(raw):
            os.write(master, raw)
            drain(0.1)

        try:
            wait_for(lambda: b"\x1b[?2004h" in output and
                     b"interrupt-fixture" in output, "terminal startup")
            send(b"Begin slow response\r")
            wait_for(lambda: b"SLOW BEGIN" in output, "slow stream")
            send(b"\x1b")
            wait_for(lambda: interruption.encode() in output,
                     "visible interruption message")
            send(b"\x1b")  # An idle Escape must not add another interruption.
            send(b"Second request\r")
            wait_for(lambda: b"FAST COMPLETE" in output, "next response after Escape")
            if b"SLOW END" in output:
                raise RuntimeError("Slow response completed despite Escape")
            slow_release.set()
            send(b"/exit\r")
            wait_for(lambda: status is not None, "clean exit")
            if os.waitstatus_to_exitcode(status) != 0:
                raise RuntimeError(f"Batbox exited with {status}")
            notices = {}
            for path in root.rglob("*.jsonl"):
                for line in path.read_text().splitlines():
                    record = json.loads(line)
                    if record.get("type") == "system" and \
                            record.get("content") == interruption:
                        notices[record["uuid"]] = record
            if len(notices) != 1:
                raise RuntimeError(f"Expected one persisted interruption, got {len(notices)}")
            if interruption.encode() in requests[-1]:
                raise RuntimeError("Local interruption notice was sent as model conversation")
            print("Escape stopped the turn, displayed one saved interruption, "
                  "and allowed a new prompt")
        finally:
            slow_release.set()
            if status is None:
                os.kill(pid, signal.SIGKILL)
                os.waitpid(pid, 0)
            os.close(master)
            server.shutdown()
            server.server_close()
            thread.join(timeout=5)


if __name__ == "__main__":
    main(Path(sys.argv[1]).resolve(strict=True))

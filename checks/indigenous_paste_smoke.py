#!/usr/bin/env python3
"""Exercise large Unicode pastes in a real POSIX terminal and Messages request."""
import argparse
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
import tempfile
import termios
import threading
import time


def run(binary, scrapling_fixture, artifacts=None):
    if artifacts:
        artifacts.mkdir(parents=True, exist_ok=True)
    payload = "SUMMARY — café 雪 🦇\r\n" + (
        "- Built private systems; preserved operations and documented migrations.\r\n" * 70)
    normalized = payload.replace("\r\n", "\n")
    expected = "before middle nested " + normalized + " after"
    requests = []
    errors = []

    class Handler(http.server.BaseHTTPRequestHandler):
        def log_message(self, *_):
            pass

        def do_POST(self):
            request = json.loads(self.rfile.read(int(self.headers["Content-Length"])))
            requests.append(request)
            if artifacts:
                (artifacts / "paste-request.json").write_text(json.dumps(request, indent=2))
            content = request["messages"][-1]["content"]
            # The application can prepend separate system-reminder text blocks;
            # the final text block is the user's edited poetry.
            texts = [content] if isinstance(content, str) else [
                item["text"] for item in content if item.get("type") == "text"]
            text = texts[-1] if texts else ""
            if self.path != "/v1/messages" or text != expected:
                errors.append({"error": "Submitted text did not preserve the paste and surrounding edits",
                               "expected_length": len(expected), "actual_length": len(text),
                               "actual_prefix": text[:100], "actual_suffix": text[-100:]})
            events = [
                ("message_start", {"type": "message_start", "message": {
                    "id": "paste", "type": "message", "role": "assistant", "model": "paste-fixture",
                    "content": [], "stop_reason": None, "usage": {"input_tokens": 100, "output_tokens": 0}}}),
                ("content_block_start", {"type": "content_block_start", "index": 0,
                    "content_block": {"type": "text", "text": ""}}),
                ("content_block_delta", {"type": "content_block_delta", "index": 0,
                    "delta": {"type": "text_delta", "text": "PASTE_ROUND_TRIP_OK"}}),
                ("content_block_stop", {"type": "content_block_stop", "index": 0}),
                ("message_delta", {"type": "message_delta", "delta": {"stop_reason": "end_turn"},
                    "usage": {"output_tokens": 8}}),
                ("message_stop", {"type": "message_stop"}),
            ]
            body = "".join("event: " + name + "\ndata: " + json.dumps(data) + "\n\n"
                           for name, data in events).encode()
            self.send_response(200)
            self.send_header("Content-Type", "text/event-stream")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)

    with tempfile.TemporaryDirectory(prefix="batbox-paste-") as temporary:
        root = Path(temporary)
        config = root / "config"
        config.mkdir()
        server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        (config / "native-client.json").write_text(json.dumps({
            "model": "paste-fixture", "base_url": f"http://127.0.0.1:{server.server_port}",
            "scrapling_command": str(scrapling_fixture), "mcpServers": {},
            "state_directory": str(root / "state"), "fullscreen": True, "auto_title": False,
        }))
        pid, master = pty.fork()
        if pid == 0:
            fcntl.ioctl(0, termios.TIOCSWINSZ, struct.pack("HHHH", 24, 100, 0, 0))
            for name in list(os.environ):
                if name.startswith("BATBOX_"):
                    del os.environ[name]
            os.environ.update(TERM="xterm-256color", BATBOX_CONFIG_DIR=str(config))
            os.execl(str(binary), str(binary), "--cwd", str(root))
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

        def wait_for(marker):
            deadline = time.monotonic() + 15
            while marker not in output and status is None and time.monotonic() < deadline:
                drain(0.05)
            if marker not in output or status is not None:
                raise RuntimeError(f"Terminal did not reach {marker!r}; status={status}")

        def send(data):
            if status is not None:
                raise RuntimeError("Terminal exited during paste")
            os.write(master, data)
            drain(0.08)

        def paste():
            wire = b"\x1b[200~" + payload.encode() + b"\x1b[201~"
            # Force a read boundary after the first byte of an em dash.
            split = wire.index("—".encode()) + 1
            send(wire[:split])
            for offset in range(split, len(wire), 1009):
                send(wire[offset:offset + 1009])

        try:
            wait_for(b"\x1b[?2004h")
            wait_for(b"paste-fixture")
            send(b"before ")
            paste()
            wait_for(f"[Pasted text #1 ({len(normalized)} characters)]".encode())
            send(b"\x1b[D\x1b[3~")  # Left skips the chip; Delete removes it.
            send(b"middle ")
            paste()
            send(b"\x7f")            # Backspace removes the second chip.
            paste()
            send(b"\x1b[D")
            send(b"nested ")
            send(b"\x1b[C")
            send(b" after")
            if artifacts:
                artifacts.mkdir(parents=True, exist_ok=True)
                (artifacts / "paste-before-resize.terminal").write_bytes(output)
            fcntl.ioctl(master, termios.TIOCSWINSZ, struct.pack("HHHH", 12, 40, 0, 0))
            os.kill(pid, signal.SIGWINCH)
            drain(0.3)
            if requests:
                raise RuntimeError("Paste submitted before Enter")
            send(b"\r")
            wait_for(b"PASTE_ROUND_TRIP_OK")
            send(b"/exit\r")
            drain(3)
            if status is None or os.waitstatus_to_exitcode(status) != 0:
                raise RuntimeError(f"Terminal did not exit cleanly: {status}")
            if errors or len(requests) != 1:
                raise RuntimeError(json.dumps({"errors": errors, "requests": len(requests)}))
            if b"\x1b[?2004l" not in output:
                raise RuntimeError("Bracketed paste mode not restored on exit")
            print(json.dumps({"passed": True, "characters": len(normalized),
                              "requests": len(requests), "unicodeReadBoundary": True,
                              "atomicNavigationAndDeletion": True, "resize": True}))
        finally:
            if artifacts:
                (artifacts / "paste-terminal.log").write_bytes(output)
            if status is None:
                os.kill(pid, signal.SIGKILL)
                os.waitpid(pid, 0)
            os.close(master)
            server.shutdown()
            server.server_close()
            thread.join(timeout=5)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary", type=Path)
    parser.add_argument("--scrapling-fixture", type=Path, required=True)
    parser.add_argument("--artifacts", type=Path)
    args = parser.parse_args()
    run(args.binary.resolve(strict=True), args.scrapling_fixture.resolve(strict=True), args.artifacts)

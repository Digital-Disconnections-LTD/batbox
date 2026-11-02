#!/usr/bin/env python3
"""Escape cancels an active streamed turn and permits the next prompt."""
import http.server
import json
from pathlib import Path
import sys
import tempfile
import threading

from loopback_http import LoopbackHTTPServer
from pty_smoke import PtySession, STEP_SECONDS, child_environment


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
                    slow_release.wait(120)
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
        server = LoopbackHTTPServer(("127.0.0.1", 0), Handler)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        (config / "native-client.json").write_text(json.dumps({
            "model": "interrupt-fixture",
            "base_url": f"http://127.0.0.1:{server.server_port}",
            "state_directory": str(root / "state"), "mcpServers": {},
            "fullscreen": True, "auto_title": False, "max_output_tokens": 50000,
        }))
        terminal = PtySession([str(binary), "--cwd", str(root)],
                              child_environment(config), rows=24, columns=100)
        output = terminal.output
        send = terminal.send

        def wait_for(condition, label, seconds=STEP_SECONDS):
            terminal.wait_for(condition, label, seconds, tail=1200)

        try:
            wait_for(lambda: b"\x1b[?2004h" in output and
                     b"interrupt-fixture" in output, "terminal startup")
            send(b"Begin slow response\r")
            wait_for(lambda: b"SLOW BEGIN" in output, "slow stream")
            send(b"\x1b")
            wait_for(lambda: interruption.encode() in output,
                     "visible interruption message")
            # An idle Escape must not add another interruption. A lone Escape is
            # only distinguishable from Alt+key once input pauses after it.
            send(b"\x1b", settle=0.5)
            send(b"Second request\r")
            wait_for(lambda: b"FAST COMPLETE" in output, "next response after Escape")
            if b"SLOW END" in output:
                raise RuntimeError("Slow response completed despite Escape")
            slow_release.set()
            send(b"/exit\r")
            if terminal.wait_for_exit() != 0:
                raise RuntimeError(f"Batbox exited with {terminal.status}")
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
            terminal.close()
            server.shutdown()
            server.server_close()
            thread.join(timeout=5)


if __name__ == "__main__":
    main(Path(sys.argv[1]).resolve(strict=True))

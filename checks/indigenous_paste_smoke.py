#!/usr/bin/env python3
"""Exercise large Unicode pastes in a real POSIX terminal and Messages request."""
import argparse
import http.server
import json
from pathlib import Path
import tempfile
import threading

from loopback_http import LoopbackHTTPServer
from pty_smoke import PtySession, child_environment


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
        server = LoopbackHTTPServer(("127.0.0.1", 0), Handler)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        (config / "native-client.json").write_text(json.dumps({
            "model": "paste-fixture", "base_url": f"http://127.0.0.1:{server.server_port}",
            "scrapling_command": str(scrapling_fixture), "mcpServers": {},
            "state_directory": str(root / "state"), "fullscreen": True, "auto_title": False,
        }))
        terminal = PtySession([str(binary), "--cwd", str(root)], child_environment(config))
        send = terminal.send
        marker = b"[Pasted text #"
        chip = f"({len(normalized)} characters)]".encode()

        def paste():
            wire = b"\x1b[200~" + payload.encode() + b"\x1b[201~"
            before = len(terminal.output)
            # Ask for a read boundary after the first byte of an em dash. The
            # pause only makes the split likely; correctness never depends on it.
            split = wire.index("—".encode()) + 1
            send(wire[:split], settle=0.2)
            terminal.write(wire[split:])
            terminal.wait_for(lambda: marker in terminal.output[before:] and
                              chip in terminal.output[before:], "paste chip")

        try:
            terminal.wait_for(lambda: b"\x1b[?2004h" in terminal.output and
                              b"paste-fixture" in terminal.output, "terminal startup")
            send(b"before ")
            paste()
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
                (artifacts / "paste-before-resize.terminal").write_bytes(terminal.output)
            before_resize = len(terminal.output)
            terminal.resize(12, 40)
            terminal.wait_for(lambda: len(terminal.output) > before_resize, "redraw after resize")
            terminal.drain(0.3)
            if requests:
                raise RuntimeError("Paste submitted before Enter")
            send(b"\r")
            terminal.wait_for(lambda: b"PASTE_ROUND_TRIP_OK" in terminal.output, "model response")
            send(b"/exit\r")
            code = terminal.wait_for_exit()
            if code != 0:
                raise RuntimeError(f"Terminal did not exit cleanly: {terminal.status}")
            if errors or len(requests) != 1:
                raise RuntimeError(json.dumps({"errors": errors, "requests": len(requests)}))
            if b"\x1b[?2004l" not in terminal.output:
                raise RuntimeError("Bracketed paste mode not restored on exit")
            print(json.dumps({"passed": True, "characters": len(normalized),
                              "requests": len(requests), "unicodeReadBoundary": True,
                              "atomicNavigationAndDeletion": True, "resize": True}))
        finally:
            if artifacts:
                (artifacts / "paste-terminal.log").write_bytes(terminal.output)
            terminal.close()
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

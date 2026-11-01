#!/usr/bin/env python3
"""Select transcript text across a scroll in a real terminal and inspect OSC 52."""
import base64
import errno
import fcntl
import http.server
import json
import os
from pathlib import Path
import pty
import re
import select
import signal
import struct
import sys
import tempfile
import termios
import threading
import time


def main(binary):
    lines = [f"ROW{index:02d} one two three" for index in range(50)]
    lines[0] += "\x1b]52;c;EVIL\x07"

    class Handler(http.server.BaseHTTPRequestHandler):
        def log_message(self, *_):
            pass

        def do_POST(self):
            self.rfile.read(int(self.headers["Content-Length"]))
            events = [
                ("message_start", {"type": "message_start", "message": {
                    "id": "copy", "type": "message", "role": "assistant",
                    "model": "copy-fixture", "content": [], "stop_reason": None,
                    "usage": {"input_tokens": 10, "output_tokens": 0}}}),
                ("content_block_start", {"type": "content_block_start", "index": 0,
                                         "content_block": {"type": "text", "text": ""}}),
                ("content_block_delta", {"type": "content_block_delta", "index": 0,
                    "delta": {"type": "text_delta", "text": "\n".join(lines)}}),
                ("content_block_stop", {"type": "content_block_stop", "index": 0}),
                ("message_delta", {"type": "message_delta", "delta": {"stop_reason": "end_turn"},
                                   "usage": {"output_tokens": 500}}),
                ("message_stop", {"type": "message_stop"}),
            ]
            body = "".join("event: " + name + "\ndata: " + json.dumps(data) + "\n\n"
                           for name, data in events).encode()
            self.send_response(200)
            self.send_header("Content-Type", "text/event-stream")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)

    with tempfile.TemporaryDirectory(prefix="batbox-copy-") as temporary:
        root = Path(temporary)
        config = root / "config"
        config.mkdir()
        server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        (config / "native-client.json").write_text(json.dumps({
            "model": "copy-fixture", "base_url": f"http://127.0.0.1:{server.server_port}",
            "state_directory": str(root / "state"), "mcpServers": {}, "fullscreen": True,
            "max_output_tokens": 50000,
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

        def wait_for(condition, label):
            deadline = time.monotonic() + 20
            while not condition() and status is None and time.monotonic() < deadline:
                drain(0.05)
            if not condition():
                raise RuntimeError(f"Timed out waiting for {label}; exit={status}; "
                                   f"terminal_tail={output[-1200:]!r}")

        def send(raw):
            os.write(master, raw)
            drain(0.1)

        try:
            wait_for(lambda: b"\x1b[?2004h" in output and b"copy-fixture" in output,
                     "terminal startup")
            send(b"Show rows\r")
            wait_for(lambda: b"ROW49" in output, "long response")
            send(b"\x0f")  # Ctrl+O opens transcript.
            drain(0.3)
            send(b"c")
            wait_for(lambda: b"COPY TRANSCRIPT" in output, "copy view")
            send(b"\x1b[5~")  # Page up: leave rows below the viewport.
            drain(0.3)
            send(b"\x1b[<0;1;2M")    # Press at first body row.
            send(b"\x1b[<32;11;23M")  # Drag to last body row.
            send(b"\x1b[<65;11;23M")  # Wheel down while still dragging.
            drain(0.3)
            send(b"\x1b[<0;11;23m")  # Release.

            def copied_texts():
                matches = re.findall(rb"\x1b\]52;c;([A-Za-z0-9+/=]+)(?:\x07|\x1b\\)",
                                     output)
                return [base64.b64decode(match).decode() for match in matches]

            wait_for(lambda: any("ROW" in text for text in copied_texts()),
                     "clipboard transfer")
            copied = copied_texts()[-1]
            if "ROW06" not in copied or "ROW30" not in copied:
                raise RuntimeError(f"Copied range did not cross the viewport: {copied!r}")
            if "one two three" not in copied:
                raise RuntimeError(f"Copied text lost paragraph spaces: {copied!r}")
            first_copies = len(copied_texts())
            send(b"\x1b[<0;1;2M")    # Start a second selection.
            send(b"\x1b[<32;11;24M")  # Drag onto the footer edge to auto-scroll.
            drain(0.8)
            send(b"\x1b[<0;11;24m")
            wait_for(lambda: len(copied_texts()) > first_copies,
                     "edge-scroll clipboard transfer")
            edge_copy = copied_texts()[-1]
            if "ROW35" not in edge_copy:
                raise RuntimeError(f"Drag edge did not extend the selection: {edge_copy!r}")
            before_native = len(output)
            send(b"y")
            wait_for(lambda: b"BATBOX TRANSCRIPT  /  NATIVE COPY" in
                     output[before_native:], "selected native copy")
            selected_native = output[before_native:]
            if b"\x1b[?1049l" not in selected_native:
                raise RuntimeError("Native copy did not leave the alternate screen")
            if edge_copy.encode() not in selected_native.replace(b"\r\n", b"\n"):
                raise RuntimeError(f"Selected native copy lost text or spaces: "
                                   f"selection={edge_copy!r} output={selected_native[-1500:]!r}")
            if b"ROW49" in selected_native:
                raise RuntimeError("Selected native copy printed the full transcript")
            send(b"\r")
            wait_for(lambda: b"COPY TRANSCRIPT" in output[before_native +
                     len(selected_native):], "copy view after native copy")
            if b"\x1b[?1049h" not in output[before_native +
                    len(selected_native):]:
                raise RuntimeError("Native copy did not restore fullscreen")
            send(b"\x1b")
            drain(0.2)
            before_full = len(output)
            send(b"y")
            wait_for(lambda: b"Press Return to return to Batbox" in
                     output[before_full:], "full native copy")
            native_full = output[before_full:]
            if b"ROW00 one two three" not in native_full or \
                    b"ROW49 one two three" not in native_full:
                raise RuntimeError("Native copy did not include the full transcript")
            if b"\x1b]52;c;EVIL" in native_full or b"\x07" in native_full:
                raise RuntimeError("Native copy emitted model-supplied terminal controls")
            send(b"\r")
            drain(0.2)
            send(b"\x0f")
            drain(0.2)
            send(b"/exit\r")
            wait_for(lambda: status is not None, "clean exit")
            if os.waitstatus_to_exitcode(status) != 0:
                raise RuntimeError(f"Batbox exited with {status}")
            print("Transcript OSC 52 and native terminal copy passed")
        finally:
            if status is None:
                os.kill(pid, signal.SIGKILL)
                os.waitpid(pid, 0)
            os.close(master)
            server.shutdown()
            server.server_close()
            thread.join(timeout=5)


if __name__ == "__main__":
    main(Path(sys.argv[1]).resolve(strict=True))

#!/usr/bin/env python3
"""Flights get titles, list with useful rows, and resume from the picker, by name and with -c."""
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

TITLE = "Parser port smoke"


def main(binary):
    requests = []

    class Handler(http.server.BaseHTTPRequestHandler):
        def log_message(self, *_):
            pass

        def do_POST(self):
            body = json.loads(self.rfile.read(int(self.headers["Content-Length"])))
            requests.append(body)
            system = json.dumps(body.get("system", ""))
            if "concise, sentence-case title" in system:
                text = json.dumps({"title": TITLE})
            else:
                text = "REPLY " + str(len(requests))
            self.send_response(200)
            self.send_header("Content-Type", "text/event-stream")
            self.end_headers()

            def event(name, data):
                self.wfile.write(("event: " + name + "\ndata: " + json.dumps(data) +
                                  "\n\n").encode())
                self.wfile.flush()

            try:
                event("message_start", {"type": "message_start", "message": {
                    "id": "m" + str(len(requests)), "type": "message",
                    "role": "assistant", "model": "resume-fixture", "content": [],
                    "stop_reason": None, "usage": {"input_tokens": 10, "output_tokens": 0}}})
                event("content_block_start", {"type": "content_block_start", "index": 0,
                                              "content_block": {"type": "text", "text": ""}})
                event("content_block_delta", {"type": "content_block_delta", "index": 0,
                                              "delta": {"type": "text_delta", "text": text}})
                event("content_block_stop", {"type": "content_block_stop", "index": 0})
                event("message_delta", {"type": "message_delta",
                                        "delta": {"stop_reason": "end_turn"},
                                        "usage": {"output_tokens": 5}})
                event("message_stop", {"type": "message_stop"})
            except (BrokenPipeError, ConnectionResetError):
                pass

    with tempfile.TemporaryDirectory(prefix="batbox-resume-") as temporary:
        root = Path(temporary)
        config = root / "config"
        work = root / "work"
        config.mkdir()
        work.mkdir()
        server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        (config / "native-client.json").write_text(json.dumps({
            "model": "resume-fixture",
            "base_url": f"http://127.0.0.1:{server.server_port}",
            "state_directory": str(root / "state"), "mcpServers": {},
            "fullscreen": True, "max_output_tokens": 50000,
        }))

        def transcripts():
            return sorted((root / "state" / "projects").rglob("*.jsonl"))

        def records(path):
            result = []
            for line in path.read_text().splitlines():
                try:
                    result.append(json.loads(line))
                except json.JSONDecodeError:
                    pass
            return result

        def session(arguments, script):
            pid, master = pty.fork()
            if pid == 0:
                fcntl.ioctl(0, termios.TIOCSWINSZ, struct.pack("HHHH", 30, 140, 0, 0))
                env = {key: value for key, value in os.environ.items()
                       if not key.startswith("BATBOX_") and key not in
                       {"TMUX", "STY", "SSH_CONNECTION"}}
                env.update(TERM="xterm-256color", BATBOX_CONFIG_DIR=str(config))
                os.execve(str(binary), [str(binary), "--cwd", str(work)] + arguments, env)
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

            def wait_for(needle, label, seconds=8):
                deadline = time.monotonic() + seconds
                while needle not in output and state["status"] is None and \
                        time.monotonic() < deadline:
                    drain(0.05)
                if needle not in output:
                    raise RuntimeError(f"Timed out waiting for {label}; exit={state['status']}; "
                                       f"terminal_tail={output[-1500:]!r}")

            def send(raw):
                os.write(master, raw)
                drain(0.15)

            try:
                wait_for(b"\x1b[?2004h", "terminal startup")
                script(wait_for, send, output)
                send(b"/exit\r")
                deadline = time.monotonic() + 8
                while state["status"] is None and time.monotonic() < deadline:
                    drain(0.05)
                if state["status"] is None or os.waitstatus_to_exitcode(state["status"]) != 0:
                    raise RuntimeError(f"Batbox did not exit cleanly: {state['status']}; "
                                       f"terminal_tail={output[-1500:]!r}")
            finally:
                if state["status"] is None:
                    os.kill(pid, signal.SIGKILL)
                    os.waitpid(pid, 0)
                os.close(master)

        try:
            def first(wait_for, send, _output):
                send(b"Port the expression parser to C++20\r")
                wait_for(b"REPLY", "first reply")
                # Exit only once the finished reply and its title are on disk;
                # an earlier /exit would legitimately mark the Flight interrupted.
                deadline = time.monotonic() + 15
                while time.monotonic() < deadline:
                    saved = [record for path in transcripts() for record in records(path)]
                    if any(record.get("type") == "assistant" for record in saved) and \
                            any(record.get("type") == "ai-title" for record in saved):
                        break
                    time.sleep(0.1)
            session([], first)
            files = transcripts()
            if len(files) != 1:
                raise RuntimeError(f"Expected one Flight transcript, got {files}")
            saved = records(files[0])
            types = [record.get("type") for record in saved]
            if not any(record.get("type") == "ai-title" and record.get("aiTitle") == TITLE
                       for record in saved):
                raise RuntimeError(f"No generated title saved: {types}")
            if types[-1] != "flight-end":
                raise RuntimeError(f"Clean exit did not end with flight-end: {types}")
            flight_id = files[0].stem

            def picker(wait_for, send, output):
                wait_for(b"Resume a Flight", "picker at startup")
                types = [record.get("type") for record in records(files[0])]
                wait_for(TITLE.encode(), "generated title in the picker row")
                wait_for(b"ended", f"clean-exit marker in the picker row (transcript {types})")
                if flight_id[:8].encode() in output:
                    raise RuntimeError("Picker still shows a raw Flight id")
                send(b"parser")
                wait_for(b"filter: parser", "type-to-filter")
                send(b"\r")
                wait_for(b"Port the expression parser", "restored transcript")
                send(b"/rename Parser rewrite\r")
                wait_for(b"Flight renamed to: Parser rewrite", "rename confirmation")
            session(["--resume"], picker)
            saved = records(files[0])
            if not any(record.get("type") == "custom-title" and
                       record.get("customTitle") == "Parser rewrite" for record in saved):
                raise RuntimeError("/rename did not save a custom title")

            def by_name(wait_for, _send, _output):
                wait_for(b"Port the expression parser", "resume by title words")
            session(["--resume", "parser rewrite"], by_name)

            def latest(wait_for, _send, _output):
                wait_for(b"Port the expression parser", "continue the latest Flight")
            session(["-c"], latest)
            if len(transcripts()) != 1:
                raise RuntimeError("Resuming created a second transcript")
            print("Flights were titled, listed by title with an end marker, filtered, "
                  "renamed, and resumed from the picker, by name and with -c")
        finally:
            server.shutdown()
            server.server_close()
            thread.join(timeout=5)


if __name__ == "__main__":
    main(Path(sys.argv[1]).resolve(strict=True))

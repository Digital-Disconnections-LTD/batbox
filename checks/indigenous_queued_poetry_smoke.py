#!/usr/bin/env python3
"""Poetry sent during a turn shows at once, reaches the model mid-turn, and resumes.

Each layout (full-screen and --inline) runs one Flight against a fixture
server: a Bash Gadget that waits on a file, then a text answer held open. A
message typed during each must appear as a queued row before the turn moves
on, must not cut the Gadget short, and must reach the next model request with
a note that it arrived while the model worked. The saved Flight then resumes
with both messages where they were delivered.
"""
import http.server
import json
from pathlib import Path
import sys
import tempfile
import threading

from loopback_http import LoopbackHTTPServer
from pty_smoke import PtySession, STEP_SECONDS, child_environment

LABEL = "queued · delivered at the next Gadget round".encode()


def run(binary, fullscreen):
    requests = []
    text_release = threading.Event()
    lock = threading.Lock()
    layout = "full-screen" if fullscreen else "inline"

    with tempfile.TemporaryDirectory(prefix="batbox-queued-") as temporary:
        root = Path(temporary)
        started = root / "gadget-started"
        release = root / "gadget-release"
        command = (f"touch '{started}'; while [ ! -f '{release}' ]; do sleep 0.05; done; "
                   "echo GADGET_DONE")

        class Handler(http.server.BaseHTTPRequestHandler):
            def log_message(self, *_):
                pass

            def do_POST(self):
                body = json.loads(self.rfile.read(int(self.headers["Content-Length"])))
                with lock:
                    requests.append(body)
                    turn = len(requests)
                self.send_response(200)
                self.send_header("Content-Type", "text/event-stream")
                self.end_headers()

                def event(value):
                    self.wfile.write(("event: " + value["type"] + "\ndata: " +
                                      json.dumps(value) + "\n\n").encode())
                    self.wfile.flush()

                def text_block(index, text):
                    event({"type": "content_block_start", "index": index,
                           "content_block": {"type": "text", "text": ""}})
                    event({"type": "content_block_delta", "index": index,
                           "delta": {"type": "text_delta", "text": text}})

                try:
                    event({"type": "message_start", "message": {
                        "id": f"turn-{turn}", "type": "message", "role": "assistant",
                        "model": "queued-fixture", "content": [], "stop_reason": None,
                        "usage": {"input_tokens": 10, "output_tokens": 0}}})
                    stop = "end_turn"
                    if turn == 1:
                        event({"type": "content_block_start", "index": 0, "content_block": {
                            "type": "tool_use", "id": "wait-1", "name": "Bash", "input": {}}})
                        event({"type": "content_block_delta", "index": 0, "delta": {
                            "type": "input_json_delta", "partial_json": json.dumps({
                                "command": command, "description": "Wait for the fixture"})}})
                        stop = "tool_use"
                    elif turn == 2:
                        text_block(0, "TEXT BEGIN\n")
                        text_release.wait(STEP_SECONDS * 2)
                        event({"type": "content_block_delta", "index": 0,
                               "delta": {"type": "text_delta", "text": "TEXT END"}})
                    else:
                        text_block(0, f"REPLY {turn}")
                    event({"type": "content_block_stop", "index": 0})
                    event({"type": "message_delta", "delta": {"stop_reason": stop},
                           "usage": {"output_tokens": 5}})
                    event({"type": "message_stop"})
                except (BrokenPipeError, ConnectionResetError):
                    pass

        config = root / "config"
        config.mkdir()
        server = LoopbackHTTPServer(("127.0.0.1", 0), Handler)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        (config / "native-client.json").write_text(json.dumps({
            "model": "queued-fixture", "base_url": f"http://127.0.0.1:{server.server_port}",
            "state_directory": str(root / "state"), "mcpServers": {},
            "fullscreen": fullscreen, "auto_title": False, "max_output_tokens": 50000,
            "permissions": {"defaultMode": "bypassPermissions"}, "thinking": False,
        }))
        environment = child_environment(config)
        terminal = PtySession([str(binary), "--cwd", str(root)], environment,
                              rows=30, columns=110)

        def wait_for(condition, label, seconds=STEP_SECONDS):
            terminal.wait_for(condition, f"{layout}: {label}", seconds, tail=2000)

        def history(index):
            return json.dumps(requests[index].get("messages", []))

        try:
            output = terminal.output
            # Inline mode has no status row naming the model; its prompt is enough.
            ready = b"queued-fixture" if fullscreen else "❯ ".encode()
            wait_for(lambda: ready in output, "startup")
            terminal.send(b"Begin\r")
            wait_for(started.exists, "Bash Gadget running")
            mark = len(output)
            terminal.send(b"QUEUED_DURING_GADGET\r")
            wait_for(lambda: LABEL in output[mark:], "queued row while the Gadget runs")
            terminal.drain(0.5)
            if len(requests) != 1:
                raise RuntimeError(f"{layout}: Enter during a Gadget moved the turn on")
            release.touch()
            wait_for(lambda: len(requests) >= 2, "the next Gadget round")
            sent = history(1)
            if "GADGET_DONE" not in sent:
                raise RuntimeError(f"{layout}: the Gadget was cut short: {sent[-800:]}")
            if "QUEUED_DURING_GADGET" not in sent or "while you were working" not in sent:
                raise RuntimeError(f"{layout}: queued poetry missed the next round: {sent[-800:]}")
            wait_for(lambda: b"TEXT BEGIN" in output, "held text answer")
            mark = len(output)
            terminal.send(b"QUEUED_DURING_TEXT\r")
            wait_for(lambda: LABEL in output[mark:], "queued row during a text answer")
            terminal.drain(0.5)
            if len(requests) != 2:
                raise RuntimeError(f"{layout}: Enter during a text answer started a request")
            text_release.set()
            wait_for(lambda: len(requests) >= 3, "delivery after the text answer")
            sent = history(2)
            if "QUEUED_DURING_TEXT" not in sent or "while you were still working" not in sent:
                raise RuntimeError(f"{layout}: text-answer poetry lacked its note: {sent[-800:]}")
            if sent.count("QUEUED_DURING_TEXT") != 1:
                raise RuntimeError(f"{layout}: the queued message was sent twice")
            wait_for(lambda: b"REPLY 3" in output, "reply to the queued message")
            if b"[queued_command]" in output:
                raise RuntimeError(f"{layout}: a delivered message rendered as [queued_command]")
            terminal.send(b"/exit\r")
            if terminal.wait_for_exit() != 0:
                raise RuntimeError(f"{layout}: Batbox exited with {terminal.status}")
        finally:
            text_release.set()
            release.touch()
            terminal.close()

        records = []
        for path in (root / "state").rglob("*.jsonl"):
            for line in path.read_text().splitlines():
                try:
                    records.append(json.loads(line))
                except json.JSONDecodeError:
                    pass
        saved = [record for record in records if record.get("type") == "attachment" and
                 record.get("attachment", {}).get("type") == "queued_command"]
        if len(saved) != 1 or saved[0]["attachment"].get("prompt") != "QUEUED_DURING_GADGET":
            raise RuntimeError(f"{layout}: the mid-turn message was not saved once: {saved}")

        resumed = PtySession([str(binary), "--cwd", str(root), "-c"], environment,
                             rows=40, columns=110)
        try:
            resumed.wait_for(lambda: b"QUEUED_DURING_GADGET" in resumed.output and
                             b"REPLY 3" in resumed.output,
                             f"{layout}: resumed Flight with the mid-turn message",
                             tail=2000)
            if b"[queued_command]" in resumed.output:
                raise RuntimeError(f"{layout}: resume rendered [queued_command]")
            resumed.send(b"/exit\r")
            if resumed.wait_for_exit() != 0:
                raise RuntimeError(f"{layout}: resumed Batbox exited with {resumed.status}")
        finally:
            resumed.close()
            server.shutdown()
            server.server_close()
            thread.join(timeout=5)
    print(f"{layout}: queued poetry showed at once, reached the model mid-turn, and resumed")


def main(binary):
    run(binary, True)
    run(binary, False)


if __name__ == "__main__":
    main(Path(sys.argv[1]).resolve(strict=True))

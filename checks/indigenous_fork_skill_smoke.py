#!/usr/bin/env python3
"""A Skill with `context: fork` typed as /name runs as a background Bat.

The command returns at once and the input stays free while the Bat runs as an
Active Mission. The Bat gets the Skill's text with the typed arguments and runs
on the Skill's model; its result reaches the model as a task notification.
"""
import http.server
import json
from pathlib import Path
import re
import sys
import tempfile
import threading

from loopback_http import LoopbackHTTPServer
from pty_smoke import PtySession, STEP_SECONDS, child_environment

BODY = "FORK_SKILL_BODY"
RESULT = "FORK_SKILL_RESULT"
CONTROL = re.compile(rb"\x1b(?:\[[0-9;?<>=]*[ -/]*[@-~]|\][^\x07\x1b]*(?:\x07|\x1b\\)|[()][0-9A-Za-z]|[=>78])")


def main(binary):
    release = threading.Event()
    lock = threading.Lock()
    leader_requests = []
    bat_requests = []

    with tempfile.TemporaryDirectory(prefix="batbox-fork-skill-") as temporary:
        root = Path(temporary)
        skill = root / ".batbox" / "skills" / "fork-probe"
        skill.mkdir(parents=True)
        (skill / "SKILL.md").write_text(
            "---\nname: fork-probe\ndescription: Fork fixture\ncontext: fork\n"
            "model: fix::fork-model\nallowed-tools: Bash\n---\n" + BODY + " $ARGUMENTS\n")

        class Handler(http.server.BaseHTTPRequestHandler):
            def log_message(self, *_):
                pass

            def do_POST(self):
                body = json.loads(self.rfile.read(int(self.headers["Content-Length"])))
                history = json.dumps(body.get("messages", []))
                if BODY in history and "<task-notification>" not in history:
                    with lock:
                        bat_requests.append(body)
                    release.wait(STEP_SECONDS * 4)
                    text = RESULT
                elif not body.get("tools"):
                    text = "ok"
                else:
                    with lock:
                        leader_requests.append(history)
                    text = "LEADER_OK"
                try:
                    self.send_response(200)
                    self.send_header("Content-Type", "text/event-stream")
                    self.end_headers()

                    def event(value):
                        self.wfile.write(("event: " + value["type"] + "\ndata: " +
                                          json.dumps(value) + "\n\n").encode())
                        self.wfile.flush()

                    event({"type": "message_start", "message": {
                        "id": "fixture", "type": "message", "role": "assistant",
                        "model": body.get("model"), "content": [], "stop_reason": None,
                        "usage": {"input_tokens": 10, "output_tokens": 0}}})
                    event({"type": "content_block_start", "index": 0,
                           "content_block": {"type": "text", "text": ""}})
                    event({"type": "content_block_delta", "index": 0,
                           "delta": {"type": "text_delta", "text": text}})
                    event({"type": "content_block_stop", "index": 0})
                    event({"type": "message_delta", "delta": {"stop_reason": "end_turn"},
                           "usage": {"output_tokens": 10}})
                    event({"type": "message_stop"})
                except (BrokenPipeError, ConnectionResetError):
                    pass

        server = LoopbackHTTPServer(("127.0.0.1", 0), Handler)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        config = root / "config"
        config.mkdir()
        (config / "native-client.json").write_text(json.dumps({
            "endpoints": {"fix": {
                "label": "Fork fixture", "base_url": f"http://127.0.0.1:{server.server_port}",
                "provider": "anthropic", "auth_token": "smoke-key",
                "context_window": 32000, "max_output_tokens": 2000,
                "models": ["main-model", "fork-model"]}},
            "profile": "fix::main-model",
            "state_directory": str(root / "state"), "mcpServers": {}, "fullscreen": True,
            "auto_title": False, "permissions": {"defaultMode": "bypassPermissions"},
            "thinking": False,
        }))
        terminal = PtySession([str(binary), "--cwd", str(root)],
                              child_environment(config), rows=30, columns=130)
        output = terminal.output

        def screen(since):
            return CONTROL.sub(b"", bytes(output[since:])).decode("utf-8", "replace")

        def wait_for(condition, label, seconds=STEP_SECONDS):
            try:
                terminal.wait_for(condition, label, seconds, tail=1)
            except RuntimeError as error:
                raise RuntimeError(f"{error}\nscreen tail: {screen(max(0, len(output) - 12000))[-2500:]}")

        try:
            wait_for(lambda: b"main-model" in output, "startup")
            mark = len(output)
            terminal.write(b"/fork-probe alpha beta\r")
            wait_for(lambda: "Started /fork-probe as a Bat" in screen(mark), "command returned")
            wait_for(lambda: bat_requests, "forked Bat request")
            bat = bat_requests[0]
            sent = json.dumps(bat.get("messages", []))
            if BODY + " alpha beta" not in sent:
                raise RuntimeError(f"The Bat did not get the Skill text with its arguments: {sent[-800:]}")
            if bat.get("model") != "fork-model":
                raise RuntimeError(f"The Bat ignored the Skill's model: {bat.get('model')}")
            wait_for(lambda: re.search(r"● /fork-probe +running", screen(mark)),
                     "Skill Bat listed as a running Mission")

            terminal.write(b"WHILE_FORK_RUNS\r")
            wait_for(lambda: any("WHILE_FORK_RUNS" in sent for sent in leader_requests),
                     "a message sent while the Skill Bat runs")
            if not release.is_set() and len(bat_requests) != 1:
                raise RuntimeError("The Skill Bat ran more than once")

            release.set()
            wait_for(lambda: any("<task-notification>" in sent and RESULT in sent
                                 for sent in leader_requests), "Skill Bat result reaching the model")
            terminal.write(b"/exit\r")
            if terminal.wait_for_exit() != 0:
                raise RuntimeError(f"Batbox exited with {terminal.status}")
            print("A forked Skill typed as /fork-probe ran as a background Bat on its own "
                  "model and its result reached the model")
        finally:
            release.set()
            terminal.close()
            server.shutdown()
            server.server_close()
            thread.join(timeout=5)


if __name__ == "__main__":
    main(Path(sys.argv[1]).resolve(strict=True))

#!/usr/bin/env python3
"""A Bat launches a Bat: the Missions tree, breadcrumb and keys with a real nest.

Usage: python3 checks/indigenous_nested_bat_smoke.py /path/to/batbox
Uses a local SSE fixture and an isolated state directory; no external inference.
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

PARENT, CHILD = "Audit the parser", "Check the lexer"
HOME_HINT = "↓ Bats · Ctrl+T list"
CONTROL = re.compile(rb"\x1b(?:\[[0-9;?<>=]*[ -/]*[@-~]|\][^\x07\x1b]*(?:\x07|\x1b\\)|[()][0-9A-Za-z]|[=>78])")


def main(binary):
    release = threading.Event()
    child_waiting = threading.Event()
    lock = threading.Lock()
    finished = []
    with tempfile.TemporaryDirectory(prefix="batbox-nested-bat-") as temporary:
        root = Path(temporary)
        source = root / "grammar.txt"
        source.write_text("NESTED_FIXTURE_GRAMMAR")
        agents = root / ".batbox" / "agents"
        agents.mkdir(parents=True)
        (agents / "auditor.md").write_text(
            "---\nname: auditor\ndescription: Nested fixture auditor\n---\nAUDITOR_SYSTEM\n")
        (agents / "lexer.md").write_text(
            "---\nname: lexer\ndescription: Nested fixture lexer\n---\nLEXER_SYSTEM\n")

        class Handler(http.server.BaseHTTPRequestHandler):
            def log_message(self, *_):
                pass

            def do_POST(self):
                body = json.loads(self.rfile.read(int(self.headers["Content-Length"])))
                system = json.dumps(body.get("system", []))
                history = json.dumps(body.get("messages", []))
                if "LEXER_SYSTEM" in system:
                    if "lexer-read-again" in history:
                        # Only a sub-Bat that outlived its stopped parent gets here.
                        with lock:
                            finished.append("LEXER_CONTINUED")
                        content = [{"type": "text", "text": "LEXER_FINAL"}]
                    elif "tool_result" in history:
                        child_waiting.set()
                        release.wait(STEP_SECONDS * 4)
                        content = [{"type": "tool_use", "id": "lexer-read-again", "name": "Read",
                                    "input": {"file_path": str(source)}}]
                    else:
                        content = [{"type": "tool_use", "id": "lexer-read", "name": "Read",
                                    "input": {"file_path": str(source)}}]
                elif "AUDITOR_SYSTEM" in system:
                    if "tool_result" in history:
                        content = [{"type": "text", "text": "AUDITOR_FINAL"}]
                    else:
                        content = [{"type": "tool_use", "id": "launch-lexer", "name": "Agent",
                                    "input": {"subagent_type": "lexer", "description": CHILD,
                                              "prompt": "Check the lexer tokens"}}]
                elif "tool_result" in history:
                    content = [{"type": "text", "text": "LEADER_READY"}]
                else:
                    content = [{"type": "tool_use", "id": "launch-auditor", "name": "Agent",
                                "input": {"subagent_type": "auditor", "description": PARENT,
                                          "prompt": "Audit the parser",
                                          "run_in_background": True}}]
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
                        "model": "nested-fixture", "content": [], "stop_reason": None,
                        "usage": {"input_tokens": 10, "output_tokens": 0}}})
                    for index, block in enumerate(content):
                        tool = block["type"] == "tool_use"
                        start = {**block, "input": {}} if tool else {"type": "text", "text": ""}
                        event({"type": "content_block_start", "index": index,
                               "content_block": start})
                        event({"type": "content_block_delta", "index": index,
                               "delta": {"type": "input_json_delta",
                                         "partial_json": json.dumps(block["input"])} if tool else
                               {"type": "text_delta", "text": block["text"]}})
                        event({"type": "content_block_stop", "index": index})
                    event({"type": "message_delta", "delta": {
                        "stop_reason": "tool_use" if content[0]["type"] == "tool_use" else "end_turn"},
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
            "model": "nested-fixture", "base_url": f"http://127.0.0.1:{server.server_port}",
            "state_directory": str(root / "state"), "mcpServers": {}, "fullscreen": True,
            "auto_title": False, "max_output_tokens": 2000,
            "permissions": {"defaultMode": "bypassPermissions"}, "thinking": False,
        }))
        terminal = PtySession([str(binary), "--cwd", str(root)],
                              child_environment(config), rows=32, columns=130)
        output = terminal.output

        def screen(since):
            return CONTROL.sub(b"", bytes(output[since:])).decode("utf-8", "replace")

        def wait_for(condition, label, seconds=STEP_SECONDS):
            terminal.wait_for(condition, label, seconds, tail=2400)

        def press(data, label, condition):
            mark = len(output)
            terminal.send(data)
            wait_for(lambda: condition(screen(mark)), label)
            terminal.drain(0.3)
            return screen(mark)

        def latest(text, needle):
            at = text.rfind(needle)
            return text[at:] if at >= 0 else ""

        def crumb(text):
            found = re.findall(r"Flight › (.*?)(?: · |\s{2,}|\r|\n|$)", text)
            return found[-1].strip() if found else ""

        def in_parent(text):
            return crumb(text) == PARENT and ("▸ ● " + PARENT) in text

        def in_child(text):
            return (crumb(text) == PARENT + " › " + CHILD and
                    re.search(r"▸ +└ ● " + CHILD, text) is not None)

        def at_home(text):
            return HOME_HINT in text and "Flight ›" not in latest(text, HOME_HINT)

        try:
            wait_for(lambda: b"nested-fixture" in output, "startup")
            terminal.write(b"Audit the parser with a sub-Bat\r")
            wait_for(child_waiting.is_set, "the sub-Bat waiting on its second round")
            wait_for(lambda: "LEADER_READY" in screen(0) and HOME_HINT in screen(0),
                     "leader idle with the Bats hint")
            wait_for(lambda: re.search(r"└ ● " + CHILD, screen(0)) is not None,
                     "the sub-Bat listed under its parent")

            press(b"\x1b[1;3B", "Alt+Down opens the parent first", in_parent)
            wait_for(lambda: (CHILD + " · Launched by " + PARENT) in screen(0),
                     "the parent's launch line naming who launched the sub-Bat")
            press(b"\x1b[1;3B", "Alt+Down then opens the sub-Bat", in_child)
            mark = len(output)
            terminal.send(b"\x1b", settle=0.5)
            wait_for(lambda: in_parent(screen(mark)), "Esc goes back to the parent")
            press(b"\x1b[1;3B", "Alt+Down to the sub-Bat again", in_child)
            press(b"\x1b[1;3D", "Alt+Left goes home from the sub-Bat", at_home)

            text = press(b"\x14", "Ctrl+T collapses the list",
                         lambda t: "Active Missions (2)" in t and "Ctrl+T collapse" not in
                         latest(t, "Active Missions (2)")[:60])
            press(b"\x14", "Ctrl+T expands the list again",
                  lambda t: "Active Missions (2) · ↓ select · Ctrl+T collapse" in t)

            # Stopping the parent stops the sub-Bat it launched.
            press(b"\x1b[1;3B", "Alt+Down to the parent before stopping it", in_parent)
            mark = len(output)
            terminal.send(b"\x03", settle=0.5)
            wait_for(lambda: re.search(re.escape(PARENT) + r"[^\n]*stopped", screen(mark)) and
                     "Active Missions (0)" in screen(mark),
                     "the parent and its sub-Bat stopping")
            release.set()
            terminal.write(b"/bats\r")
            wait_for(lambda: re.search(CHILD + r" · stopped", screen(mark)) is not None and
                     re.search(PARENT + r" · stopped", screen(mark)) is not None,
                     "/bats listing both the parent and its sub-Bat as stopped")
            terminal.drain(2.0)
            if finished:
                raise RuntimeError("The sub-Bat kept running after its parent was stopped")
            terminal.send(b"\x1b", settle=0.5)
            terminal.write(b"/exit\r")
            if terminal.wait_for_exit() != 0:
                raise RuntimeError(f"Batbox exited with {terminal.status}")
            print("A Bat launched a sub-Bat: Alt+Down walked parent then child with the "
                  "breadcrumb, Esc went to the parent, Alt+Left went home, and stopping "
                  "the parent stopped the child")
        finally:
            release.set()
            terminal.close()
            server.shutdown()
            server.server_close()
            thread.join(timeout=5)


if __name__ == "__main__":
    main(Path(sys.argv[1]).resolve(strict=True))

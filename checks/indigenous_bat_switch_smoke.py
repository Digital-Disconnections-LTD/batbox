#!/usr/bin/env python3
"""Alt+arrows move between running Bats, Ctrl+T folds the Missions list.

Usage: python3 checks/indigenous_bat_switch_smoke.py /path/to/batbox
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

TITLES = ("Research alpha", "Research beta")
HOME_HINT = "Alt+↓ Bats · Ctrl+T list"
CONTROL = re.compile(rb"\x1b(?:\[[0-9;?<>=]*[ -/]*[@-~]|\][^\x07\x1b]*(?:\x07|\x1b\\)|[()][0-9A-Za-z]|[=>78])")


def main(binary):
    release = threading.Event()
    waiting = set()
    finished = set()
    lock = threading.Lock()
    with tempfile.TemporaryDirectory(prefix="batbox-bat-switch-") as temporary:
        root = Path(temporary)
        source = root / "notes.txt"
        source.write_text("SWITCH_FIXTURE_NOTES")
        agents = root / ".batbox" / "agents"
        agents.mkdir(parents=True)
        (agents / "worker.md").write_text(
            "---\nname: worker\ndescription: Switch fixture worker\n---\n"
            "SWITCH_WORKER_SYSTEM\n")

        class Handler(http.server.BaseHTTPRequestHandler):
            def log_message(self, *_):
                pass

            def do_POST(self):
                body = json.loads(self.rfile.read(int(self.headers["Content-Length"])))
                history = json.dumps(body.get("messages", []))
                if "SWITCH_WORKER_SYSTEM" in json.dumps(body.get("system", [])):
                    name = "ALPHA" if "CHILD_ALPHA" in history else "BETA"
                    if "tool_result" in history:
                        with lock:
                            waiting.add(name)
                        release.wait(STEP_SECONDS * 4)
                        content = [{"type": "text", "text": "CHILD_FINAL_" + name}]
                    else:
                        content = [{"type": "tool_use", "id": "child-read-" + name,
                                    "name": "Read", "input": {"file_path": str(source)}}]
                elif "tool_result" in history or "CHILD_FINAL" in history:
                    content = [{"type": "text", "text": "LEADER_READY"}]
                else:
                    content = [{"type": "tool_use", "id": "launch-" + name,
                                "name": "Agent", "input": {
                                    "subagent_type": "worker",
                                    "description": "Research " + name.lower(),
                                    "prompt": "CHILD_" + name,
                                    "run_in_background": True}}
                               for name in ("ALPHA", "BETA")]
                self.send_response(200)
                self.send_header("Content-Type", "text/event-stream")
                self.end_headers()

                def event(value):
                    self.wfile.write(("event: " + value["type"] + "\ndata: " +
                                      json.dumps(value) + "\n\n").encode())
                    self.wfile.flush()

                event({"type": "message_start", "message": {
                    "id": "fixture", "type": "message", "role": "assistant",
                    "model": "switch-fixture", "content": [], "stop_reason": None,
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
                if content[0].get("text", "").startswith("CHILD_FINAL_"):
                    with lock:
                        finished.add(content[0]["text"])

        server = LoopbackHTTPServer(("127.0.0.1", 0), Handler)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        config = root / "config"
        config.mkdir()
        (config / "native-client.json").write_text(json.dumps({
            "model": "switch-fixture", "base_url": f"http://127.0.0.1:{server.server_port}",
            "state_directory": str(root / "state"), "mcpServers": {}, "fullscreen": True,
            "auto_title": False, "max_output_tokens": 2000,
            "permissions": {"defaultMode": "bypassPermissions"}, "thinking": False,
        }))
        terminal = PtySession([str(binary), "--cwd", str(root)],
                              child_environment(config), rows=30, columns=130)
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

        def in_bat(title):
            # The open Bat's row is marked and the breadcrumb names it.
            return lambda text: ("Flight › " + title) in text and ("▸ ● " + title) in text

        def at_home(text):
            return HOME_HINT in text and "Flight ›" not in latest(text, HOME_HINT)

        try:
            wait_for(lambda: b"switch-fixture" in output, "startup")
            terminal.write(b"Launch two research Bats\r")
            wait_for(lambda: len(waiting) == 2, "both Bats waiting on their second round")
            wait_for(lambda: "LEADER_READY" in screen(0) and HOME_HINT in screen(0),
                     "leader idle with the Bats hint")

            text = press(b"\x1b[1;3B", "Alt+Down opens the top Bat",
                         lambda t: re.search(r"Flight › (Research \w+)", t) is not None)
            first = re.findall(r"Flight › (Research \w+)", text)[-1]
            second = TITLES[1] if first == TITLES[0] else TITLES[0]
            wait_for(lambda: in_bat(first)(screen(0)), "highlighted first Bat row")
            press(b"\x1b[1;3B", "Alt+Down moves to the second Bat", in_bat(second))
            terminal.send(b"\x1b[1;3B", settle=0.8)
            crumbs = re.findall(r"Flight › (Research \w+)", screen(0))
            if crumbs[-1] != second:
                raise RuntimeError(f"Alt+Down past the last Bat must not wrap: {crumbs[-3:]}")
            press(b"\x1b[1;3A", "Alt+Up moves back", in_bat(first))
            press(b"\x1bb", "Option+Left (ESC b) goes home", at_home)
            press(b"\x1b[1;3B", "Alt+Down again", in_bat(first))
            press(b"\x1b[1;3D", "Alt+Left (xterm) goes home", at_home)
            press(b"\x1b[1;3B", "Alt+Down before Esc", in_bat(first))
            mark = len(output)
            terminal.send(b"\x1b", settle=0.5)
            wait_for(lambda: at_home(screen(mark)), "Esc with an empty input goes back")

            # Terminal.app with Option as Meta sends ESC before the arrow.
            press(b"\x1b\x1b[B", "Option+Down as ESC ESC [B", in_bat(first))
            press(b"\x1b\x1b[D", "Option+Left as ESC ESC [D", at_home)

            text = press(b"\x14", "Ctrl+T collapses the list",
                         lambda t: re.search(r"Active Missions \(2\)(?! · Ctrl\+T collapse)", t))
            frame = latest(text, "Active Missions (2)")
            if frame.startswith("Active Missions (2) · Ctrl+T collapse") or "● Research" in frame:
                raise RuntimeError(f"Ctrl+T left the list expanded: {frame[:200]!r}")
            text = press(b"\x1b[1;3B", "Alt+Down works while collapsed",
                         lambda t: ("Active Missions (2) · in: " + first) in t)
            press(b"\x1b[1;3D", "home while collapsed", at_home)
            press(b"\x14", "Ctrl+T expands the list",
                  lambda t: "Active Missions (2) · Ctrl+T collapse" in t and "● " + first in t)

            # A lone Escape, then a later arrow, is two keys: no Bat opens.
            mark = len(output)
            for arrow in (b"\x1b[B", b"\x1b[A"):
                terminal.send(b"\x1b", settle=0.5)
                terminal.send(arrow, settle=0.5)
            if "Flight ›" in screen(mark):
                raise RuntimeError("Esc then a later arrow was read as Alt+arrow")
            # The plain Up recalled the last poetry; clear it before /exit.
            terminal.send(b"\x05\x15")

            # Switching never stopped the Bats: both still finish their work.
            release.set()
            wait_for(lambda: finished == {"CHILD_FINAL_ALPHA", "CHILD_FINAL_BETA"},
                     "both Bats finishing after the switches")
            terminal.write(b"/exit\r")
            if terminal.wait_for_exit() != 0:
                raise RuntimeError(f"Batbox exited with {terminal.status}")
            print("Alt+Up/Down/Left moved between Bats with highlight and breadcrumb; "
                  "Ctrl+T folded the list; Esc then a later arrow stayed home")
        finally:
            release.set()
            terminal.close()
            server.shutdown()
            server.server_close()
            thread.join(timeout=5)


if __name__ == "__main__":
    main(Path(sys.argv[1]).resolve(strict=True))

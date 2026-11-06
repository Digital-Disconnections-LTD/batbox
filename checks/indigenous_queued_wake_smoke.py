#!/usr/bin/env python3
"""A message typed while the model waits on a Bat reaches it at once.

One Flight with one Bat slot against a fixture server. A foreground Bat moves
to the background when a message is queued; a TaskOutput wait on it ends with
user_message; a launch waiting for the slot returns and the Bat starts once
the slot frees. Each queued message reaches the next model request. Escape
ends a turn held in a slot wait or a TaskOutput wait, and while the Bats list
or a Bat's view is open the status row says Escape goes back first.
"""
import http.server
import json
from pathlib import Path
import re
import sys
import tempfile
import threading
import time

from loopback_http import LoopbackHTTPServer
from pty_smoke import PtySession, STEP_SECONDS, child_environment

MARKER = "WAKE_FIXTURE_BAT_SYSTEM"
INTERRUPTION = b"Interrupted the Batbox"
CONTROL = re.compile(rb"\x1b(?:\[[0-9;?<>=]*[ -/]*[@-~]|\][^\x07\x1b]*(?:\x07|\x1b\\)|[()][0-9A-Za-z]|[=>78])")


def agent(call_id, prompt, background):
    arguments = {"subagent_type": "wake-worker", "description": prompt.lower().replace("_", " "),
                 "prompt": prompt}
    if background:
        arguments["run_in_background"] = True
    return {"type": "tool_use", "id": call_id, "name": "Agent", "input": arguments}


def main(binary):
    release = threading.Event()
    lock = threading.Lock()
    main_requests = []
    bats_seen = []

    def tool_ids(messages):
        return {block.get("id") for message in messages if message.get("role") == "assistant"
                and isinstance(message.get("content"), list)
                for block in message["content"] if block.get("type") == "tool_use"}

    def foreground_id(messages):
        found = re.search(r"agentId: ([\w-]+)", json.dumps(messages))
        if not found:
            raise RuntimeError("The backgrounded Bat's result named no agentId")
        return found.group(1)

    def leader_reply(messages):
        done = tool_ids(messages)
        if "call_FG" not in done:
            return [agent("call_FG", "FOREGROUND_BAT_TASK", False)]
        if "call_WAIT" not in done:
            return [{"type": "tool_use", "id": "call_WAIT", "name": "TaskOutput",
                     "input": {"task_id": foreground_id(messages), "block": True}}]
        if "call_SLOT" not in done:
            return [agent("call_SLOT", "SLOT_BAT_TASK", True)]
        if "call_ESC" not in done:
            return [agent("call_ESC", "ESC_BAT_TASK", True)]
        if "Second turn" in json.dumps(messages) and "call_WAIT2" not in done:
            return [{"type": "tool_use", "id": "call_WAIT2", "name": "TaskOutput",
                     "input": {"task_id": foreground_id(messages), "block": True}}]
        return [{"type": "text", "text": "LEADER_OK"}]

    with tempfile.TemporaryDirectory(prefix="batbox-queued-wake-") as temporary:
        root = Path(temporary)
        agents = root / ".batbox" / "agents"
        agents.mkdir(parents=True)
        (agents / "wake-worker.md").write_text(
            "---\nname: wake-worker\ndescription: Wake fixture worker\n---\n" + MARKER + "\n")

        class Handler(http.server.BaseHTTPRequestHandler):
            def log_message(self, *_):
                pass

            def do_POST(self):
                body = json.loads(self.rfile.read(int(self.headers["Content-Length"])))
                messages = body.get("messages", [])
                history = json.dumps(messages)
                if MARKER in json.dumps(body.get("system", [])):
                    name = next((task for task in ("FOREGROUND", "SLOT", "ESC")
                                 if task + "_BAT_TASK" in history), "OTHER")
                    with lock:
                        bats_seen.append(name)
                    if name == "FOREGROUND":
                        release.wait(STEP_SECONDS * 4)
                    content = [{"type": "text", "text": name + "_DONE"}]
                elif not body.get("tools"):
                    content = [{"type": "text", "text": "ok"}]
                else:
                    with lock:
                        main_requests.append(history)
                    content = leader_reply(messages)
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
                        "model": "wake-fixture", "content": [], "stop_reason": None,
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
            "model": "wake-fixture", "base_url": f"http://127.0.0.1:{server.server_port}",
            "state_directory": str(root / "state"), "mcpServers": {}, "fullscreen": True,
            "auto_title": False, "max_output_tokens": 2000, "max_concurrent_subagents": 1,
            "permissions": {"defaultMode": "bypassPermissions"}, "thinking": False,
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

        def deliver(text, requests_before, label):
            """Type a message during a wait; return the request it reached and the delay."""
            pressed = time.monotonic()
            terminal.write(text.encode() + b"\r")
            wait_for(lambda: len(main_requests) > requests_before, label)
            delay = time.monotonic() - pressed
            sent = main_requests[requests_before]
            if text not in sent or "while you were working" not in sent:
                raise RuntimeError(f"{label}: the queued message missed the next round: {sent[-1200:]}")
            if delay > 3:
                raise RuntimeError(f"{label}: the message took {delay:.1f}s to reach the model")
            return sent, delay

        def interrupt(label):
            mark = len(output)
            pressed = time.monotonic()
            terminal.write(b"\x1b")
            wait_for(lambda: INTERRUPTION in output[mark:], label)
            delay = time.monotonic() - pressed
            if delay > 1:
                raise RuntimeError(f"{label}: Escape took {delay:.2f}s to end the turn")
            terminal.drain(0.5)
            return delay

        try:
            wait_for(lambda: b"wake-fixture" in output, "startup")
            terminal.write(b"Start\r")
            wait_for(lambda: "FOREGROUND" in bats_seen, "foreground Bat running")
            terminal.drain(0.5)
            sent, foreground_delay = deliver("WAKE_FOREGROUND", 1, "foreground Bat")
            if "Async agent launched" not in sent:
                raise RuntimeError(f"The foreground Bat was not moved to the background: {sent[-1200:]}")

            wait_for(lambda: len(main_requests) >= 2, "TaskOutput request")
            terminal.drain(1.0)
            sent, wait_delay = deliver("WAKE_TASKOUTPUT", 2, "TaskOutput wait")
            if "<retrieval_status>user_message</retrieval_status>" not in sent or \
                    "has not failed" not in sent:
                raise RuntimeError(f"TaskOutput did not end for the message: {sent[-1200:]}")

            wait_for(lambda: len(main_requests) >= 3, "slot-waiting launch request")
            terminal.drain(1.0)
            if "SLOT" in bats_seen:
                raise RuntimeError("The second Bat started while the only slot was taken")
            sent, slot_delay = deliver("WAKE_SLOT", 3, "slot wait")
            if "No Bat slot was free" not in sent:
                raise RuntimeError(f"The slot wait did not return for the message: {sent[-1200:]}")

            wait_for(lambda: len(main_requests) >= 4, "second slot-waiting launch request")
            terminal.drain(1.0)
            slot_escape = interrupt("Escape during a slot wait")

            terminal.write(b"Second turn\r")
            wait_for(lambda: len(main_requests) >= 5, "second TaskOutput request")
            terminal.drain(1.0)
            mark = len(output)
            terminal.send(b"\x1b[B", settle=0.5)
            wait_for(lambda: "back to input" in screen(mark), "Bats list hint")
            mark = len(output)
            terminal.send(b"\x1b", settle=0.6)
            wait_for(lambda: "to interrupt" in screen(mark), "Escape returned to the input")
            mark = len(output)
            terminal.send(b"\x1b[1;3B", settle=0.5)
            wait_for(lambda: "· Esc back · Alt+↑↓ move" in screen(mark), "Bat view hint")
            mark = len(output)
            terminal.send(b"\x1b", settle=0.6)
            # Still working: Escape in the Bat's view only went back.
            wait_for(lambda: "to interrupt" in screen(mark), "Escape returned to the Flight")
            wait_escape = interrupt("Escape during a TaskOutput wait")

            release.set()
            wait_for(lambda: "SLOT" in bats_seen, "queued Bat starting once the slot freed")
            terminal.drain(1.0)
            if "ESC" in bats_seen:
                raise RuntimeError("A launch interrupted in its slot wait still started")
            terminal.write(b"/exit\r")
            if terminal.wait_for_exit() != 0:
                raise RuntimeError(f"Batbox exited with {terminal.status}")
            print("Queued messages reached the model in "
                  f"{foreground_delay:.2f}s (foreground Bat), {wait_delay:.2f}s (TaskOutput), "
                  f"{slot_delay:.2f}s (slot wait); Escape ended a slot wait in "
                  f"{slot_escape * 1000:.0f} ms and a TaskOutput wait in {wait_escape * 1000:.0f} ms")
        finally:
            release.set()
            terminal.close()
            server.shutdown()
            server.server_close()
            thread.join(timeout=5)


if __name__ == "__main__":
    main(Path(sys.argv[1]).resolve(strict=True))

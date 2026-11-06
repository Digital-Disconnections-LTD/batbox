#!/usr/bin/env python3
"""A Bat waiting for a slot can be cancelled, and the model is told it never started.

One Flight with one Bat slot against a fixture server. A background Bat holds
the slot. The next launch waits in the model's turn; x on its waiting row in
the Bats list cancels it and the Agent call says so. A third launch returns
queued_for_slot when the user types a message; the model stops it with
TaskStop, and a notification says it never started. Neither cancelled Bat runs
once the slot frees.
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

MARKER = "CANCEL_FIXTURE_BAT_SYSTEM"
CANCELLED = "was cancelled before it started"
CONTROL = re.compile(rb"\x1b(?:\[[0-9;?<>=]*[ -/]*[@-~]|\][^\x07\x1b]*(?:\x07|\x1b\\)|[()][0-9A-Za-z]|[=>78])")


def agent(call_id, prompt):
    return {"type": "tool_use", "id": call_id, "name": "Agent", "input": {
        "subagent_type": "cancel-worker", "description": prompt.lower().replace("_", " "),
        "prompt": prompt, "run_in_background": True}}


def main(binary):
    release = threading.Event()
    lock = threading.Lock()
    main_requests = []
    bats_seen = []

    def tool_ids(messages):
        return {block.get("id") for message in messages if message.get("role") == "assistant"
                and isinstance(message.get("content"), list)
                for block in message["content"] if block.get("type") == "tool_use"}

    def leader_reply(messages):
        done = tool_ids(messages)
        history = json.dumps(messages)
        if "call_HOLD" not in done:
            return [agent("call_HOLD", "HOLD_BAT_TASK")]
        if "call_A" not in done:
            return [agent("call_A", "A_BAT_TASK")]
        if "call_B" not in done:
            return [agent("call_B", "B_BAT_TASK")]
        if "call_STOP" not in done and "waiting-call_B" in history:
            return [{"type": "tool_use", "id": "call_STOP", "name": "TaskStop",
                     "input": {"task_id": "waiting-call_B"}}]
        return [{"type": "text", "text": "LEADER_OK"}]

    with tempfile.TemporaryDirectory(prefix="batbox-waiting-cancel-") as temporary:
        root = Path(temporary)
        agents = root / ".batbox" / "agents"
        agents.mkdir(parents=True)
        (agents / "cancel-worker.md").write_text(
            "---\nname: cancel-worker\ndescription: Cancel fixture worker\n---\n" + MARKER + "\n")

        class Handler(http.server.BaseHTTPRequestHandler):
            def log_message(self, *_):
                pass

            def do_POST(self):
                body = json.loads(self.rfile.read(int(self.headers["Content-Length"])))
                messages = body.get("messages", [])
                history = json.dumps(messages)
                if MARKER in json.dumps(body.get("system", [])):
                    name = next((task for task in ("HOLD", "A", "B")
                                 if task + "_BAT_TASK" in history), "OTHER")
                    with lock:
                        bats_seen.append(name)
                    if name == "HOLD":
                        release.wait(STEP_SECONDS * 6)
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
                        "model": "cancel-fixture", "content": [], "stop_reason": None,
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
            "model": "cancel-fixture", "base_url": f"http://127.0.0.1:{server.server_port}",
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

        def request_with(text, label):
            wait_for(lambda: any(text in sent for sent in main_requests), label)
            return next(sent for sent in main_requests if text in sent)

        try:
            wait_for(lambda: b"cancel-fixture" in output, "startup")
            terminal.write(b"Start\r")
            wait_for(lambda: "HOLD" in bats_seen, "slot-holding Bat running")
            wait_for(lambda: len(main_requests) >= 2, "launch that waits for the slot")
            terminal.drain(1.0)

            # x on the waiting row cancels the launch the turn is waiting on.
            terminal.send(b"\x1b[B", settle=0.5)
            for _ in range(4):
                mark = len(output)
                terminal.send(b"\x1b[B", settle=0.5)
                if "❯ ○" in screen(mark):
                    break
            else:
                raise RuntimeError("The waiting Bat's row could not be selected:\n" +
                                   screen(max(0, len(output) - 6000))[-2500:])
            terminal.send(b"x", settle=0.3)
            wait_for(lambda: len(main_requests) >= 3, "Agent call returning after x")
            sent = main_requests[2]
            if CANCELLED not in sent:
                raise RuntimeError(f"The cancelled launch did not tell the model: {sent[-1500:]}")
            terminal.send(b"\x1b", settle=0.3)

            # A launch that already returned queued_for_slot is stopped by TaskStop.
            mark = len(output)
            wait_for(lambda: "b bat task" in screen(mark), "third launch waiting for the slot")
            terminal.drain(1.0)
            terminal.write(b"WAKE_B\r")
            sent = request_with("No Bat slot was free", "queued_for_slot result")
            if 'TaskStop with task_id: \\"waiting-call_B\\"' not in sent:
                raise RuntimeError(f"The queued launch did not name its task id: {sent[-1500:]}")
            sent = request_with("Successfully stopped task: waiting-call_B", "TaskStop result")
            notice = request_with("<task-notification>", "never-started notification")
            if CANCELLED not in notice or "waiting-call_B" not in notice:
                raise RuntimeError(f"The notification did not say it never started: {notice[-1500:]}")

            release.set()
            terminal.drain(3.0)
            if "A" in bats_seen or "B" in bats_seen:
                raise RuntimeError(f"A cancelled Bat ran after the slot freed: {bats_seen}")
            terminal.write(b"/exit\r")
            if terminal.wait_for_exit() != 0:
                raise RuntimeError(f"Batbox exited with {terminal.status}")
            print("Cancelled a waiting Bat from its row and a queued one with TaskStop; "
                  "neither started once the slot freed")
        finally:
            release.set()
            terminal.close()
            server.shutdown()
            server.server_close()
            thread.join(timeout=5)


if __name__ == "__main__":
    main(Path(sys.argv[1]).resolve(strict=True))

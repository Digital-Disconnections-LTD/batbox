#!/usr/bin/env python3
"""Foreground agents show compact progress while the lead waits for results."""
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


def main(binary):
    release = threading.Event()
    parent_requests = []
    waiting_children = set()
    lock = threading.Lock()
    with tempfile.TemporaryDirectory(prefix="batbox-agent-progress-") as temporary:
        root = Path(temporary)
        source = root / "fixture.txt"
        source.write_text("CHILD_INTERNAL_OUTPUT")
        agents = root / ".batbox" / "agents"
        agents.mkdir(parents=True)
        (agents / "worker.md").write_text(
            "---\nname: worker\ndescription: Progress fixture worker\n---\n"
            "PROGRESS_WORKER_SYSTEM\n")

        class Handler(http.server.BaseHTTPRequestHandler):
            def log_message(self, *_):
                pass

            def do_POST(self):
                body = json.loads(self.rfile.read(int(self.headers["Content-Length"])))
                history = json.dumps(body.get("messages", []))
                worker = "PROGRESS_WORKER_SYSTEM" in json.dumps(body.get("system", []))
                if worker:
                    name = "ALPHA" if "CHILD_ALPHA" in history else "BETA"
                    if "tool_result" in history:
                        with lock:
                            waiting_children.add(name)
                        release.wait(15)
                        content = [{"type": "text", "text": "CHILD_FINAL_" + name}]
                    else:
                        content = [{"type": "tool_use", "id": "child-read-" + name,
                                    "name": "Read", "input": {"file_path": str(source)}}]
                else:
                    with lock:
                        parent_requests.append(body)
                    if "tool_result" in history:
                        content = [{"type": "text", "text": "LEADER_COMPLETE"}]
                    else:
                        content = [{"type": "tool_use", "id": "launch-" + name,
                                    "name": "Agent", "input": {
                                        "subagent_type": "worker",
                                        "description": "Research " + name.lower(),
                                        "prompt": "CHILD_" + name}}
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
                    "model": "agent-fixture", "content": [], "stop_reason": None,
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

        server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        config = root / "config"
        config.mkdir()
        (config / "native-client.json").write_text(json.dumps({
            "model": "agent-fixture", "base_url": f"http://127.0.0.1:{server.server_port}",
            "state_directory": str(root / "state"), "mcpServers": {}, "fullscreen": True,
            "context_window": 70000, "compact_at": 60000, "max_output_tokens": 2000,
            "permissions": {"defaultMode": "bypassPermissions"}, "thinking": False,
        }))
        pid, master = pty.fork()
        if pid == 0:
            fcntl.ioctl(0, termios.TIOCSWINSZ, struct.pack("HHHH", 30, 110, 0, 0))
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

        def wait_for(condition, label, seconds=12):
            deadline = time.monotonic() + seconds
            while not condition() and status is None and time.monotonic() < deadline:
                drain(0.05)
            if not condition():
                raise RuntimeError(f"Timed out waiting for {label}; exit={status}; "
                                   f"terminal_tail={output[-1800:]!r}")

        try:
            wait_for(lambda: b"agent-fixture" in output, "startup")
            os.write(master, b"Run two research agents\r")
            wait_for(lambda: len(waiting_children) == 2, "both child tool rounds")
            drain(0.2)
            for key in (b'"timestamp"', b'"progressMessages"', b'"agentId"'):
                if key in output:
                    raise RuntimeError(f"Internal agent JSON appeared in the UI: {key!r}")
            wait_for(lambda: b"2 Bats" in output and b"Latest tool: Read" in output,
                     "compact grouped progress")
            if len(parent_requests) != 1:
                raise RuntimeError("Lead requested more inference while waiting for agents")
            release.set()
            wait_for(lambda: b"LEADER_COMPLETE" in output, "lead completion")
            history = json.dumps(parent_requests[-1].get("messages", []))
            for name in ("ALPHA", "BETA"):
                if "CHILD_FINAL_" + name not in history:
                    raise RuntimeError("Lead did not receive both final results")
            if "agent_progress" in history or "CHILD_INTERNAL_OUTPUT" in history:
                raise RuntimeError("Child progress leaked into the lead model request")
            os.write(master, b"/exit\r")
            wait_for(lambda: status is not None, "clean exit")
            if os.waitstatus_to_exitcode(status) != 0:
                raise RuntimeError(f"Batbox exited with {status}")
            print("Compact agent progress displayed; lead waited and received only final results")
        finally:
            release.set()
            if status is None:
                os.kill(pid, signal.SIGKILL)
                os.waitpid(pid, 0)
            os.close(master)
            server.shutdown()
            server.server_close()
            thread.join(timeout=5)


if __name__ == "__main__":
    main(Path(sys.argv[1]).resolve(strict=True))

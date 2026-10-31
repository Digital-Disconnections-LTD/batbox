#!/usr/bin/env python3
"""Exercise a Bat through live messages, completion, and a resumed conversation.

Usage: python3 checks/bat_conversation_workflow.py /path/to/batbox
Uses an isolated local SSE fixture and state directory; no external inference.
"""
import collections
import json
import os
import pathlib
import subprocess
import sys
import tempfile
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer


root = pathlib.Path(tempfile.mkdtemp(prefix="batbox-agent-conversation-"))
requests = []
errors = []
lock = threading.Lock()
child_started = threading.Event()
child_second_started = threading.Event()
live_queued = threading.Event()
child_finished = threading.Event()
worker_calls = 0
end_turn_case = "--end-turn" in sys.argv[2:]


def blocks(body):
    for msg in body.get("messages", []):
        content = msg.get("content", [])
        if isinstance(content, list):
            yield from (x for x in content if isinstance(x, dict))


class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, *_):
        pass

    def do_POST(self):
        global worker_calls
        body = json.loads(self.rfile.read(int(self.headers["Content-Length"])))
        with lock:
            requests.append(body)
            index = len(requests)
            (root / "requests.json").write_text(json.dumps(requests, indent=2))
        system = json.dumps(body.get("system", []))
        history = json.dumps(body.get("messages", []))
        result_ids = {b.get("tool_use_id") for b in blocks(body)
                      if b.get("type") == "tool_result"}
        reason = "end_turn"
        if "WORKER_CONVERSATION_SYSTEM" in system:
            with lock:
                worker_calls += 1
                call = worker_calls
            if call == 1:
                child_started.set()
                if end_turn_case:
                    value = "CHILD_FIRST_COMPLETED"
                else:
                    value = ("Bash", {"command": "sleep 2; printf 'CHILD_TOOL_ROUND\\n'",
                                      "description": "Wait for live follow-up"})
                    gadget_id = "child-boundary"
                    reason = "tool_use"
            elif call == 2:
                child_second_started.set()
                if "LIVE_PENDING_MARKER" not in history:
                    errors.append("Live message did not reach the running child's next tool round")
                value = "CHILD_FIRST_COMPLETED"
            elif call == 3:
                # Resuming the Bat retains both queued input and completed messages.
                for marker in ("ORIGINAL_CHILD_PROMPT", "LIVE_PENDING_MARKER", "CHILD_FIRST_COMPLETED",
                               "HISTORICAL_FOLLOWUP_MARKER"):
                    if marker not in history:
                        errors.append("Resumed child history missing " + marker)
                value = "CHILD_RESUMED_COMPLETED"
            else:
                errors.append("Unexpected extra child request " + str(call))
                value = "UNEXPECTED_CHILD_REQUEST"
        elif "message-resume" in result_ids:
            if "wait-resume" not in result_ids:
                value = ("TaskOutput", {"task_id": self.bat_id(body),
                                        "block": True, "timeout": 30000})
                gadget_id = "wait-resume"
                reason = "tool_use"
            else:
                if not child_finished.wait(10):
                    errors.append("Final resumed child did not finish")
                value = "AGENT_CONVERSATION_CHAIN_OK"
        elif "wait-first" in result_ids:
            if end_turn_case and not child_second_started.wait(10):
                errors.append("Undrained end-turn input did not automatically resume the same child")
            value = ("SendMessage", {"to": "worker", "summary": "Continue completed child",
                                      "message": "HISTORICAL_FOLLOWUP_MARKER"})
            gadget_id = "message-resume"
            reason = "tool_use"
        elif "message-live" in result_ids:
            live_queued.set()
            value = ("TaskOutput", {"task_id": self.bat_id(body),
                                    "block": True, "timeout": 30000})
            gadget_id = "wait-first"
            reason = "tool_use"
        elif "launch-child" in result_ids:
            if not child_started.wait(10):
                errors.append("Child never started before SendMessage")
            value = ("SendMessage", {"to": "worker", "summary": "Guide running child",
                                      "message": "LIVE_PENDING_MARKER"})
            gadget_id = "message-live"
            reason = "tool_use"
        else:
            value = ("Agent", {"name": "worker", "subagent_type": "worker",
                                "description": "Conversation workflow worker",
                                "prompt": "ORIGINAL_CHILD_PROMPT",
                                "run_in_background": True})
            gadget_id = "launch-child"
            reason = "tool_use"
        self.send_response(200)
        self.send_header("Content-Type", "text/event-stream")
        self.send_header("Request-Id", f"agent-workflow-{index}")
        self.send_header("Connection", "close")
        self.end_headers()

        def emit(event):
            self.wfile.write(("event: " + event["type"] + "\ndata: " +
                             json.dumps(event) + "\n\n").encode())
            self.wfile.flush()

        emit({"type": "message_start", "message": {"type": "message",
              "id": f"msg-{index}", "model": "fixture", "role": "assistant",
              "content": [], "stop_reason": None,
              "usage": {"input_tokens": 8, "output_tokens": 0}}})
        if isinstance(value, tuple):
            emit({"type": "content_block_start", "index": 0, "content_block": {
                "type": "tool_use", "id": gadget_id, "name": value[0], "input": {}}})
            emit({"type": "content_block_delta", "index": 0, "delta": {
                "type": "input_json_delta", "partial_json": json.dumps(value[1])}})
        else:
            emit({"type": "content_block_start", "index": 0,
                  "content_block": {"type": "text", "text": ""}})
            emit({"type": "content_block_delta", "index": 0,
                  "delta": {"type": "text_delta", "text": value}})
        if end_turn_case and "WORKER_CONVERSATION_SYSTEM" in system and call == 1:
            if not live_queued.wait(10):
                errors.append("Text-only child was not steered before end_turn")
        emit({"type": "content_block_stop", "index": 0})
        emit({"type": "message_delta", "delta": {"stop_reason": reason},
              "usage": {"output_tokens": 6}})
        emit({"type": "message_stop"})
        if "WORKER_CONVERSATION_SYSTEM" in system and call == 3:
            child_finished.set()
        self.close_connection = True

    @staticmethod
    def bat_id(body):
        for block in blocks(body):
            if block.get("type") != "tool_result" or block.get("tool_use_id") != "launch-child":
                continue
            value = block.get("content")
            candidates = [value] if isinstance(value, str) else [
                b.get("text", "") for b in value or [] if isinstance(b, dict)]
            for text in candidates:
                try:
                    data = json.loads(text)
                    if "agentId" in data:
                        return data["agentId"]
                except (ValueError, TypeError):
                    pass
            # BatGadget's model-facing result is prose in some configurations.
            import re
            match = re.search(r"[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}", str(value))
            if match:
                return match.group()
        errors.append("Could not extract original agent ID")
        return "MISSING_AGENT_ID"


server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
server.daemon_threads = True
threading.Thread(target=server.serve_forever, daemon=True).start()
work = root / "project"
(work / ".batbox" / "agents").mkdir(parents=True)
(work / ".batbox" / "agents" / "worker.md").write_text(
    "---\nname: worker\ndescription: Native conversation fixture worker\n---\n"
    "WORKER_CONVERSATION_SYSTEM. Follow the conversation fixture instructions.\n")
config = root / "config.json"
config.write_text(json.dumps({"model": "fixture", "base_url": f"http://127.0.0.1:{server.server_port}",
    "state_directory": str(root / "state"), "mcpServers": {}, "thinking": False,
    "permissions": {"defaultMode": "bypassPermissions"}, "context_window": 70000,
    "max_output_tokens": 1000, "compact_at": 60000}))
(root / "mcp.json").write_text('{"mcpServers":{}}')
env = os.environ.copy()
for key in ("BATBOX_MODEL", "BATBOX_BASE_URL", "BATBOX_PROFILE", "BATBOX_AUTH_TOKEN",
            "BATBOX_CONTEXT_WINDOW", "BATBOX_MAX_OUTPUT_TOKENS", "BATBOX_COMPACT_AT"):
    env.pop(key, None)
try:
    run = subprocess.run([sys.argv[1], "--config", str(config), "--cwd", str(work),
        "--mcp-config", str(root / "mcp.json"), "--no-thinking", "--print",
        "AGENT_CONVERSATION_WORKFLOW: launch a worker, steer it, then resume it."],
        capture_output=True, text=True, stdin=subprocess.DEVNULL, env=env, timeout=65)
    (root / "stdout.txt").write_text(run.stdout)
    (root / "stderr.txt").write_text(run.stderr)
    assert run.returncode == 0, (run.returncode, run.stdout, run.stderr)
    assert "AGENT_CONVERSATION_CHAIN_OK" in run.stdout, run.stdout
    assert worker_calls == 3, worker_calls
    assert not errors, errors
    assert any(t.get("name") == "SendMessage" for r in requests for t in r.get("tools", []))
    files = list((root / "state").glob("projects/*/*/subagents/agent-*.jsonl"))
    assert len(files) == 1, ("Resume must retain the original agent identity", files)
    rows = [json.loads(line) for line in files[0].read_text().splitlines() if line.strip()]
    ids = [r["uuid"] for r in rows if "uuid" in r]
    assert len(ids) == len(set(ids)), ("Repeated history UUIDs were written on resume", collections.Counter(ids))
    assert "HISTORICAL_FOLLOWUP_MARKER" in files[0].read_text()
    ledgers = list((root / "state" / "tasks").glob("*.inputs.jsonl"))
    assert len(ledgers) == 1, ledgers
    inputs = [json.loads(line)["message"] for line in ledgers[0].read_text().splitlines()]
    assert len(inputs) == 2, inputs
    for message in inputs:
        assert ids.count(message["uuid"]) == 1, ("Canonical direct input was lost or duplicated", message)
    result = {"passed": True, "liveDeliveryAtToolBoundary": True,
              "historicalResumePreservesRecordedHistoryAndIdentity": True,
              "nativeDirectInputJournalPreservesCanonicalUUIDs": True,
              "textOnlyEndTurnContinuation": end_turn_case,
              "deduplicatedDurableTranscript": True, "requests": len(requests),
              "artifacts": str(root)}
    (root / "RESULT.json").write_text(json.dumps(result, indent=2))
    print(json.dumps(result), flush=True)
except BaseException:
    print("FAIL artifacts:", root, "errors:", errors, flush=True)
    raise
finally:
    server.shutdown()
    server.server_close()

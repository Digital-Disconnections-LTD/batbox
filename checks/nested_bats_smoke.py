#!/usr/bin/env python3
"""Bats launching Bats: depth limit, missing Agent Gadget, tree-wide limits.

Usage: python3 checks/nested_bats_smoke.py /path/to/batbox
Runs batbox --print against a local SSE fixture with an isolated state and
configuration directory; no external inference.
"""
import json
import os
import pathlib
import subprocess
import sys
import tempfile
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

# Each Bat type launches the next one in the chain with the Agent Gadget.
CHAIN = {
    "leader": ("auditor", "Audit the parser"),
    "auditor": ("lexer", "Check the lexer"),
    "lexer": ("tokens", "Count the tokens"),
    "tokens": ("grains", "Split the grains"),
    "grains": None,
}
MARKERS = {name: name.upper() + "_SYSTEM" for name in CHAIN if name != "leader"}


def role_of(body):
    system = json.dumps(body.get("system", []))
    for name, marker in MARKERS.items():
        if marker in system:
            return name
    return "leader"


def system_text(body):
    system = body.get("system", [])
    if isinstance(system, str):
        return system
    return "\n".join(b.get("text", "") for b in system if isinstance(b, dict))


def tool_results(body):
    found = {}
    for message in body.get("messages", []):
        content = message.get("content", [])
        if not isinstance(content, list):
            continue
        for block in content:
            if isinstance(block, dict) and block.get("type") == "tool_result":
                value = block.get("content")
                if isinstance(value, list):
                    value = "\n".join(b.get("text", "") for b in value if isinstance(b, dict))
                found[block.get("tool_use_id")] = str(value)
    return found


class Fixture:
    def __init__(self, parallel=False):
        self.parallel = parallel
        self.requests = []
        self.lock = threading.Lock()
        fixture = self

        class Handler(BaseHTTPRequestHandler):
            protocol_version = "HTTP/1.1"

            def log_message(self, *_):
                pass

            def do_POST(self):
                body = json.loads(self.rfile.read(int(self.headers["Content-Length"])))
                role = role_of(body)
                results = tool_results(body)
                with fixture.lock:
                    fixture.requests.append({
                        "role": role, "system": system_text(body), "results": results,
                        "tools": [t.get("name") for t in body.get("tools", [])]})
                step = CHAIN[role]
                if results or step is None:
                    blocks = [{"type": "text", "text": role.upper() + "_DONE"}]
                else:
                    target, description = step
                    names = [target, target] if fixture.parallel and role == "auditor" else [target]
                    blocks = [{"type": "tool_use", "id": f"launch-{role}-{i}", "name": "Agent",
                               "input": {"subagent_type": name, "description": description +
                                         (f" {i + 1}" if len(names) > 1 else ""),
                                         "prompt": "Go " + name}}
                              for i, name in enumerate(names)]
                self.send_response(200)
                self.send_header("Content-Type", "text/event-stream")
                self.send_header("Connection", "close")
                self.end_headers()

                def emit(event):
                    self.wfile.write(("event: " + event["type"] + "\ndata: " +
                                      json.dumps(event) + "\n\n").encode())
                    self.wfile.flush()

                emit({"type": "message_start", "message": {
                    "type": "message", "id": "nested", "model": "fixture", "role": "assistant",
                    "content": [], "stop_reason": None,
                    "usage": {"input_tokens": 8, "output_tokens": 0}}})
                for index, block in enumerate(blocks):
                    tool = block["type"] == "tool_use"
                    emit({"type": "content_block_start", "index": index, "content_block":
                          {**block, "input": {}} if tool else {"type": "text", "text": ""}})
                    emit({"type": "content_block_delta", "index": index, "delta":
                          {"type": "input_json_delta", "partial_json": json.dumps(block["input"])}
                          if tool else {"type": "text_delta", "text": block["text"]}})
                    emit({"type": "content_block_stop", "index": index})
                emit({"type": "message_delta", "delta": {
                    "stop_reason": "tool_use" if blocks[0]["type"] == "tool_use" else "end_turn"},
                    "usage": {"output_tokens": 6}})
                emit({"type": "message_stop"})
                self.close_connection = True

        self.server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
        self.server.daemon_threads = True
        threading.Thread(target=self.server.serve_forever, daemon=True).start()

    def close(self):
        self.server.shutdown()
        self.server.server_close()

    def first(self, role):
        for request in self.requests:
            if request["role"] == role:
                return request
        raise AssertionError(f"No request from {role}: {[r['role'] for r in self.requests]}")

    def result(self, role):
        for request in self.requests:
            if request["role"] == role and request["results"]:
                return "\n".join(request["results"].values())
        raise AssertionError(f"{role} never saw a Gadget result")


def run(binary, label, arguments=(), settings=None, environment=None, parallel=False):
    root = pathlib.Path(tempfile.mkdtemp(prefix="batbox-nested-" + label + "-"))
    fixture = Fixture(parallel)
    work = root / "project"
    agents = work / ".batbox" / "agents"
    agents.mkdir(parents=True)
    for name, marker in MARKERS.items():
        (agents / (name + ".md")).write_text(
            f"---\nname: {name}\ndescription: Nested fixture {name}\n---\n{marker}\n")
    config = root / "config"
    config.mkdir()
    (config / "native-client.json").write_text(json.dumps({
        "model": "fixture", "base_url": f"http://127.0.0.1:{fixture.server.server_port}",
        "state_directory": str(root / "state"), "mcpServers": {}, "thinking": False,
        "auto_title": False, "max_output_tokens": 1000,
        "permissions": {"defaultMode": "bypassPermissions"}}))
    if settings is not None:
        (config / "settings.json").write_text(json.dumps(settings))
    env = os.environ.copy()
    for key in list(env):
        if key.startswith("BATBOX_"):
            env.pop(key)
    env["BATBOX_CONFIG_DIR"] = str(config)
    env.update(environment or {})
    try:
        done = subprocess.run([str(binary), "--cwd", str(work), "--no-thinking", *arguments,
                               "--print", "Start the nested fixture"],
                              capture_output=True, text=True, stdin=subprocess.DEVNULL,
                              env=env, timeout=60)
        if done.returncode != 0 or "LEADER_DONE" not in done.stdout:
            raise AssertionError(f"{label}: exit {done.returncode}\n{done.stdout}\n{done.stderr}")
        return fixture, root
    finally:
        fixture.close()


def expect(condition, message):
    if not condition:
        raise AssertionError(message)


def default_depth(binary):
    fixture, root = run(binary, "depth")
    for role, below in (("auditor", "2 levels"), ("lexer", "1 level")):
        request = fixture.first(role)
        expect("Agent" in request["tools"], f"{role} must have the Agent Gadget: {request['tools']}")
        expect(f"nested up to {below} below you" in request["system"],
               f"{role} must be told how deep it may nest: {request['system'][-400:]}")
    tokens = fixture.first("tokens")
    expect("Agent" not in tokens["tools"], f"A depth 3 Bat must not get Agent: {tokens['tools']}")
    expect("You cannot launch Bats: this Bat is at the deepest level Bats may nest "
           "(max_bat_depth 3)" in tokens["system"], "The capped Bat's prompt must say so")
    refused = fixture.result("tokens")
    expect("Agent is not available to this Bat: it is at the maximum Bat depth "
           "(max_bat_depth 3)" in refused, "A great-grandchild needs a clear refusal: " + refused)
    expect(not any(r["role"] == "grains" for r in fixture.requests),
           "No great-grandchild may run")
    expect("TOKENS_DONE" in fixture.result("lexer") and "LEXER_DONE" in fixture.result("auditor"),
           "Each Bat must get its sub-Bat's result")
    # Transcripts nest under the launching Bat's folder.
    transcripts = sorted((root / "state" / "projects").rglob("agent-*.jsonl"))
    nested = [p for p in transcripts if p.parent.name == "subagents" and
              p.parent.parent.name.startswith("agent-")]
    expect(len(transcripts) == 3 and len(nested) == 2, f"Nested layout: {transcripts}")
    deepest = max(transcripts, key=lambda p: len(p.parts))
    meta = json.loads(deepest.with_suffix(".meta.json").read_text())
    parent = deepest.parent.parent.name[len("agent-"):]
    expect(meta.get("parentBatId") == parent and meta.get("batDepth") == 3,
           f"Sub-Bat metadata must name its parent and depth: {meta}")


def depth_one(binary):
    for label, kwargs in (
            ("cli", {"arguments": ["--max-bat-depth", "1"]}),
            ("settings", {"settings": {"max_bat_depth": 1}}),
            ("environment", {"environment": {"BATBOX_MAX_BAT_DEPTH": "0"}})):
        fixture, _ = run(binary, "depth-one-" + label, **kwargs)
        auditor = fixture.first("auditor")
        expect("Agent" not in auditor["tools"],
               f"{label}: max_bat_depth 1 must remove Agent from Bats: {auditor['tools']}")
        expect("You cannot launch Bats" in auditor["system"],
               f"{label}: the Bat's prompt must say it cannot launch Bats")
        expect("Agent is not available to this Bat" in fixture.result("auditor"),
               f"{label}: a refused launch must explain itself")
        expect(not any(r["role"] == "lexer" for r in fixture.requests),
               f"{label}: no sub-Bat may run")
    # The command line beats settings.json.
    fixture, _ = run(binary, "precedence", arguments=["--max-bat-depth", "2"],
                     settings={"max_bat_depth": 1})
    expect("Agent" in fixture.first("auditor")["tools"] and
           "Agent" not in fixture.first("lexer")["tools"],
           "--max-bat-depth 2 must override max_bat_depth 1 in settings.json")


def tree_limits(binary):
    fixture, _ = run(binary, "outstanding", arguments=["--max-subagents", "2"])
    refused = fixture.result("lexer")
    expect("Outstanding subagent limit reached: 2 Bats" in refused,
           "--max-subagents must count the whole tree: " + refused)
    expect(not any(r["role"] == "tokens" for r in fixture.requests),
           "The third Bat in the tree must not run")
    fixture, _ = run(binary, "concurrent", arguments=["--max-concurrent-subagents", "1"],
                     parallel=True)
    results = [r for r in fixture.requests if r["role"] == "auditor" and r["results"]]
    texts = list(results[0]["results"].values())
    refusals = [t for t in texts if "Concurrent subagent limit reached" in t]
    expect(len(texts) == 2 and len(refusals) == 1,
           f"One sub-Bat borrows the parent's slot, the other is refused: {texts}")
    expect(sum(1 for r in fixture.requests if r["role"] == "lexer" and not r["results"]) == 1,
           "Exactly one sub-Bat may run with one concurrent slot")


def main(binary):
    default_depth(binary)
    depth_one(binary)
    tree_limits(binary)
    print("Bats nested to max_bat_depth with clear refusals; max_bat_depth 1 removed Agent; "
          "subagent limits counted the whole Bat tree")


if __name__ == "__main__":
    main(pathlib.Path(sys.argv[1]).resolve(strict=True))

#!/usr/bin/env python3
"""Failing Bash commands must hand the model their output, not their command text.

Runs the real binary in print mode against a fake openai-chat endpoint, so the
Bash Gadget is built from the production default inputs. The fake server asks
for two failing commands and one that outlives its timeout, and records the tool
results it is sent. A command that times out is moved to the background, so its
output has to be in the file the result names.
"""
from http.server import BaseHTTPRequestHandler
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import threading

from loopback_http import LoopbackHTTPServer


FINAL_MARKER = "BASH-ERROR-OUTPUT-DONE"
CALLS = [
    ("call_BASHERR0000001", {
        "command": "printf 'visible-%s\\n' output; printf 'visible-%s\\n' stderr >&2; exit 3",
        "description": "Fail after printing to stdout and stderr"}),
    ("call_BASHERR0000002", {
        "command": "printf 'stderr-only-%s\\n' output >&2; exit 4",
        "description": "Fail after printing only to stderr"}),
    ("call_BASHERR0000003", {
        "command": "printf 'before-%s\\n' timeout; sleep 30",
        "description": "Print, then run past the timeout", "timeout": 1500}),
]


def chunk(delta=None, finish=None, usage=None):
    body = {"id": "chatcmpl-bash-error", "object": "chat.completion.chunk", "model": "served",
            "choices": [] if delta is None and finish is None else
            [{"index": 0, "delta": delta or {}, "finish_reason": finish}]}
    if usage is not None:
        body["usage"] = usage
    return body


def reply(messages):
    usage = {"prompt_tokens": 900, "completion_tokens": 12, "total_tokens": 912}
    answered = sum(1 for message in messages if message.get("role") == "tool")
    if answered >= len(CALLS):
        return [chunk({"role": "assistant", "content": FINAL_MARKER}), chunk(finish="stop"),
                chunk(usage=usage)]
    call_id, arguments = CALLS[answered]
    return [chunk({"role": "assistant", "content": ""}),
            chunk({"tool_calls": [{"index": 0, "id": call_id, "type": "function",
                                   "function": {"name": "Bash", "arguments": ""}}]}),
            chunk({"tool_calls": [{"index": 0, "function": {"arguments": json.dumps(arguments)}}]}),
            chunk(finish="tool_calls"), chunk(usage=usage)]


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def main(binary):
    requests = []
    errors = []
    background_output = {}

    class Handler(BaseHTTPRequestHandler):
        protocol_version = "HTTP/1.1"

        def log_message(self, *_):
            pass

        def do_POST(self):
            try:
                body = json.loads(self.rfile.read(int(self.headers["Content-Length"])))
                requests.append(body)
                messages = body.get("messages", [])
                for message in messages:
                    if message.get("tool_call_id") != CALLS[2][0] or CALLS[2][0] in background_output:
                        continue
                    named = re.search(r"Output is being written to: (\S+)", str(message.get("content")))
                    background_output[CALLS[2][0]] = \
                        Path(named.group(1)).read_text(encoding="utf-8") if named else None
                chunks = reply(messages) if body.get("tools") else \
                    [chunk({"content": "ok"}), chunk(finish="stop")]
                raw = b"".join(b"data: " + json.dumps(item).encode() + b"\n\n" for item in chunks)
                raw += b"data: [DONE]\n\n"
                self.send_response(200)
                self.send_header("Content-Type", "text/event-stream")
                self.send_header("Content-Length", str(len(raw)))
                self.end_headers()
                self.wfile.write(raw)
            except Exception as error:
                errors.append(repr(error))
                self.send_error(500)

    with tempfile.TemporaryDirectory(prefix="batbox-bash-error-output-") as temporary:
        root = Path(temporary)
        config = root / "config"
        work = root / "work"
        config.mkdir()
        work.mkdir()
        server = LoopbackHTTPServer(("127.0.0.1", 0), Handler)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        try:
            (config / "native-client.json").write_text(json.dumps({
                "endpoints": {"fake-chat": {
                    "label": "Fake chat", "base_url": f"http://127.0.0.1:{server.server_port}/v1",
                    "provider": "openai-chat", "preset": "llama.cpp", "auth_token": "smoke-key",
                    "quirks": {"max_tokens_field": "max_tokens"},
                    "context_window": 32000, "max_output_tokens": 4000, "models": ["smoke-model"]}},
                "profile": "fake-chat::smoke-model",
                "state_directory": str(root / "state"), "mcpServers": {}, "thinking": False,
                "auto_title": False,
            }), encoding="utf-8")
            env = {key: value for key, value in os.environ.items() if not key.startswith("BATBOX_")}
            env["BATBOX_CONFIG_DIR"] = str(config)
            env["NO_PROXY"] = env["no_proxy"] = "127.0.0.1,localhost"
            command = [str(binary), "--cwd", str(work), "--no-thinking", "--yolo", "--print",
                       "Run the failing Bash fixture commands."]
            process = subprocess.run(command, env=env, stdin=subprocess.DEVNULL,
                                     capture_output=True, text=True, encoding="utf-8",
                                     errors="replace", timeout=60)
        finally:
            server.shutdown()
            server.server_close()
            thread.join(timeout=5)
        require(not errors, f"Fake endpoint failed: {errors}")
        require(process.returncode == 0,
                f"Batbox exited with {process.returncode}; stderr={process.stderr[-3000:]!r}")
        require(FINAL_MARKER in process.stdout,
                f"Final answer missing from print output: {process.stdout[-3000:]!r}")
        final = next((r for r in reversed(requests)
                      if sum(1 for m in r.get("messages", []) if m.get("role") == "tool") == len(CALLS)),
                     None)
        require(final is not None, f"No request carried all {len(CALLS)} tool results; "
                                   f"saw {len(requests)} requests")
        results = {m.get("tool_call_id"): m.get("content")
                   for m in final["messages"] if m.get("role") == "tool"}

        def content(call_id):
            value = results.get(call_id)
            if isinstance(value, list):
                value = "".join(part.get("text", "") for part in value if isinstance(part, dict))
            require(isinstance(value, str), f"Tool result for {call_id} is missing: {results!r}")
            return value

        both = content(CALLS[0][0])
        require("Exit code 3" in both, f"Exit code missing from the failed command result: {both!r}")
        require("visible-output" in both, f"Stdout missing from the failed command result: {both!r}")
        require("visible-stderr" in both, f"Stderr missing from the failed command result: {both!r}")
        require(CALLS[0][1]["command"] not in both,
                f"Failed command result echoed the command instead of its output: {both!r}")
        stderr_only = content(CALLS[1][0])
        require("Exit code 4" in stderr_only,
                f"Exit code missing from the stderr-only result: {stderr_only!r}")
        require("stderr-only-output" in stderr_only,
                f"Stderr missing from the stderr-only result: {stderr_only!r}")
        require(CALLS[1][1]["command"] not in stderr_only,
                f"Stderr-only result echoed the command instead of its output: {stderr_only!r}")
        timed_out = content(CALLS[2][0])
        require("Command running in background with ID:" in timed_out,
                f"Timed-out command was not moved to the background: {timed_out!r}")
        written = background_output.get(CALLS[2][0])
        require(written is not None and "before-timeout" in written,
                f"Output printed before the timeout is missing from {timed_out!r}: {written!r}")
        print("failed Bash commands hand the model their stdout, stderr and exit code")


if __name__ == "__main__":
    main(Path(sys.argv[1]).resolve(strict=True))

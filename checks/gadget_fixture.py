"""Run the real binary in print mode against a fake openai-chat model.

The fake model calls the Gadgets a smoke lists, one per request, and the run
returns the tool results and tool descriptions the model was sent. Every Gadget
is built from the production default inputs.
"""
from http.server import BaseHTTPRequestHandler
import json
import os
from pathlib import Path
import subprocess
import threading

from loopback_http import LoopbackHTTPServer


FINAL_MARKER = "GADGET-FIXTURE-DONE"


def chunk(delta=None, finish=None, usage=None):
    body = {"id": "chatcmpl-gadget-fixture", "object": "chat.completion.chunk",
            "model": "served",
            "choices": [] if delta is None and finish is None else
            [{"index": 0, "delta": delta or {}, "finish_reason": finish}]}
    if usage is not None:
        body["usage"] = usage
    return body


def text_of(value):
    if isinstance(value, list):
        return "".join(part.get("text", "") for part in value if isinstance(part, dict))
    return value if isinstance(value, str) else json.dumps(value)


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def answer(text):
    usage = {"prompt_tokens": 900, "completion_tokens": 12, "total_tokens": 912}
    return [chunk({"role": "assistant", "content": text}), chunk(finish="stop"), chunk(usage=usage)]


def reply(messages, steps, side_reply):
    """Answer one request: a side conversation (a Bat), the next step, or the end.

    A step is (call_id, gadget_name, arguments); arguments may be a function of
    the messages so far, for a call that needs an earlier result.
    """
    if side_reply:
        side = side_reply(messages)
        if side is not None:
            return answer(side)
    answered = sum(1 for message in messages if message.get("role") == "tool")
    if answered >= len(steps):
        return answer(FINAL_MARKER)
    call_id, name, arguments = steps[answered]
    if callable(arguments):
        arguments = arguments(messages)
    usage = {"prompt_tokens": 900, "completion_tokens": 12, "total_tokens": 912}
    return [chunk({"role": "assistant", "content": ""}),
            chunk({"tool_calls": [{"index": 0, "id": call_id, "type": "function",
                                   "function": {"name": name, "arguments": ""}}]}),
            chunk({"tool_calls": [{"index": 0, "function": {"arguments": json.dumps(arguments)}}]}),
            chunk(finish="tool_calls"), chunk(usage=usage)]


def run(binary, root, work, name, steps, arguments, side_reply=None, timeout=120,
        client=None, environment=None, requests=None, completed=None):
    """Run one print-mode Flight; return ({call_id: result text}, {gadget: description}).

    client adds keys to native-client.json; environment adds variables; a
    requests list receives every request body the fake model was sent; a
    completed list receives the finished process.
    """
    requests = [] if requests is None else requests
    errors = []

    class Handler(BaseHTTPRequestHandler):
        protocol_version = "HTTP/1.1"

        def log_message(self, *_):
            pass

        def do_POST(self):
            try:
                body = json.loads(self.rfile.read(int(self.headers["Content-Length"])))
                requests.append(body)
                chunks = reply(body.get("messages", []), steps, side_reply) if body.get("tools") \
                    else [chunk({"content": "ok"}), chunk(finish="stop")]
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

    config = Path(root) / (name + "-config")
    config.mkdir()
    server = LoopbackHTTPServer(("127.0.0.1", 0), Handler)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    try:
        (config / "native-client.json").write_text(json.dumps({
            "endpoints": {"fake-chat": {
                "label": "Fake chat", "base_url": f"http://127.0.0.1:{server.server_port}/v1",
                "provider": "openai-chat", "preset": "llama.cpp", "auth_token": "smoke-key",
                "quirks": {"max_tokens_field": "max_tokens"},
                "context_window": 64000, "max_output_tokens": 4000, "models": ["smoke-model"]}},
            "profile": "fake-chat::smoke-model",
            "state_directory": str(Path(root) / (name + "-state")), "mcpServers": {},
            "thinking": False, "auto_title": False, **(client or {}),
        }), encoding="utf-8")
        env = {key: value for key, value in os.environ.items() if not key.startswith("BATBOX_")}
        env["BATBOX_CONFIG_DIR"] = str(config)
        env["NO_PROXY"] = env["no_proxy"] = "127.0.0.1,localhost"
        env.update(environment or {})
        command = [str(binary), "--cwd", str(work), "--no-thinking", *arguments,
                   "--print", "Run the Gadget fixture."]
        process = subprocess.run(command, env=env, stdin=subprocess.DEVNULL,
                                 capture_output=True, text=True, encoding="utf-8",
                                 errors="replace", timeout=timeout)
    finally:
        server.shutdown()
        server.server_close()
        thread.join(timeout=5)
    if completed is not None:
        completed.append(process)
    require(not errors, f"Fake endpoint failed: {errors}")
    require(process.returncode == 0,
            f"Batbox exited with {process.returncode}; stderr={process.stderr[-3000:]!r}")
    require(FINAL_MARKER in process.stdout,
            f"Final answer missing from print output: {process.stdout[-3000:]!r}")

    def main_conversation(body):
        return not (side_reply and side_reply(body.get("messages", [])) is not None)

    final = next((body for body in reversed(requests) if main_conversation(body) and
                  sum(1 for m in body.get("messages", []) if m.get("role") == "tool") == len(steps)),
                 None)
    require(final is not None, f"No request carried all {len(steps)} tool results")
    results = {m.get("tool_call_id"): text_of(m.get("content"))
               for m in final["messages"] if m.get("role") == "tool"}
    for call_id, _, _ in steps:
        require(isinstance(results.get(call_id), str), f"Tool result for {call_id} is missing")
    tools = {tool.get("function", {}).get("name"): tool.get("function", {}).get("description", "")
             for tool in final.get("tools", [])}
    return results, tools

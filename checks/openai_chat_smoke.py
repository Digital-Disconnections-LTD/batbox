#!/usr/bin/env python3
"""Terminal smoke: a two-turn tool call through an openai-chat endpoint."""
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


def chunk(delta=None, finish=None, usage=None):
    body = {"id": "chatcmpl-smoke", "object": "chat.completion.chunk", "model": "served",
            "choices": [] if delta is None and finish is None else
            [{"index": 0, "delta": delta or {}, "finish_reason": finish}]}
    if usage is not None:
        body["usage"] = usage
    return body


def main(binary):
    requests = []
    headers = []

    class Handler(http.server.BaseHTTPRequestHandler):
        protocol_version = "HTTP/1.1"

        def log_message(self, *_):
            pass

        def do_POST(self):
            body = json.loads(self.rfile.read(int(self.headers["Content-Length"])))
            requests.append(body)
            headers.append({k.lower(): v for k, v in self.headers.items()})
            messages = body.get("messages", [])
            usage = {"prompt_tokens": 900, "completion_tokens": 12, "total_tokens": 912}
            if any(message.get("role") == "tool" for message in messages):
                chunks = [chunk({"role": "assistant", "content": "The command "}),
                          chunk({"content": "printed SMOKE-DONE hi."}), chunk(finish="stop"),
                          chunk(usage=usage)]
            elif body.get("tools"):
                chunks = [chunk({"role": "assistant", "content": ""}),
                          chunk({"tool_calls": [{"index": 0, "id": "call_SMOKE0000001", "type": "function",
                                                 "function": {"name": "Bash", "arguments": ""}}]}),
                          chunk({"tool_calls": [{"index": 0, "function": {"arguments": "{\"command\":"}}]}),
                          chunk({"tool_calls": [{"index": 0, "function": {
                              "arguments": "\"echo hi\",\"description\":\"Print hi\"}"}}]}),
                          chunk(finish="tool_calls"), chunk(usage=usage)]
            else:
                chunks = [chunk({"content": "ok"}), chunk(finish="stop"), chunk(usage=usage)]
            raw = b"".join(b"data: " + json.dumps(item).encode() + b"\n\n" for item in chunks)
            raw += b"data: [DONE]\n\n"
            self.send_response(200)
            self.send_header("Content-Type", "text/event-stream")
            self.send_header("Content-Length", str(len(raw)))
            self.end_headers()
            self.wfile.write(raw)

    with tempfile.TemporaryDirectory(prefix="batbox-openai-chat-") as temporary:
        root = Path(temporary)
        config = root / "config"
        config.mkdir()
        server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        (config / "native-client.json").write_text(json.dumps({
            "endpoints": {"fake-chat": {
                "label": "Fake chat", "base_url": f"http://127.0.0.1:{server.server_port}/v1",
                "provider": "openai-chat", "preset": "llama.cpp", "auth_token": "smoke-key",
                "quirks": {"max_tokens_field": "max_tokens"},
                "context_window": 32000, "max_output_tokens": 4000, "models": ["smoke-model"]}},
            "profile": "fake-chat::smoke-model",
            "state_directory": str(root / "state"), "mcpServers": {}, "fullscreen": True,
            "auto_title": False,
        }))
        pid, master = pty.fork()
        if pid == 0:
            fcntl.ioctl(0, termios.TIOCSWINSZ, struct.pack("HHHH", 30, 110, 0, 0))
            env = {key: value for key, value in os.environ.items() if not key.startswith("BATBOX_")}
            env.update(TERM="xterm-256color", BATBOX_CONFIG_DIR=str(config))
            os.execve(str(binary), [str(binary), "--cwd", str(root), "--yolo"], env)
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

        def wait_for(condition, label):
            deadline = time.monotonic() + 25
            while not condition() and status is None and time.monotonic() < deadline:
                drain(0.05)
            if not condition():
                raise RuntimeError(f"Timed out waiting for {label}; exit={status}; "
                                   f"requests={len(requests)}; terminal_tail={output[-3000:]!r}")

        def send(command):
            os.write(master, command.encode() + b"\r")
            drain(0.1)

        try:
            wait_for(lambda: b"\x1b[?2004h" in output and b"smoke-model" in output, "terminal startup")
            send("Run echo hi with Bash")
            wait_for(lambda: any(any(m.get("role") == "tool" for m in r.get("messages", []))
                                 for r in requests), "tool result request")
            wait_for(lambda: b"SMOKE-DONE" in output, "final answer")
            send("/exit")
            wait_for(lambda: status is not None, "clean exit")
            if os.waitstatus_to_exitcode(status) != 0:
                raise RuntimeError(f"Batbox exited with {status}")
            first = next(r for r in requests if r.get("tools"))
            final = next(r for r in requests if any(m.get("role") == "tool" for m in r.get("messages", [])))
            index = requests.index(first)
            if headers[index].get("authorization") != "Bearer smoke-key":
                raise RuntimeError("openai-chat request lacked the bearer token")
            if first.get("model") != "smoke-model" or first.get("stream") is not True:
                raise RuntimeError(f"Unexpected request head: model={first.get('model')} stream={first.get('stream')}")
            if first.get("max_tokens") != 4000 or "max_completion_tokens" in first:
                raise RuntimeError("max_tokens quirk was not applied")
            if first["messages"][0].get("role") != "system":
                raise RuntimeError("System prompt did not become a system message")
            names = [tool["function"]["name"] for tool in first["tools"]]
            if "Bash" not in names or any(tool.get("type") != "function" for tool in first["tools"]):
                raise RuntimeError(f"Tools were not translated to functions: {names[:8]}")
            calls = [m for m in final["messages"] if m.get("tool_calls")]
            results = [m for m in final["messages"] if m.get("role") == "tool"]
            if not calls or calls[-1]["tool_calls"][0]["id"] != "call_SMOKE0000001":
                raise RuntimeError("Assistant tool call was not replayed with its id")
            if json.loads(calls[-1]["tool_calls"][0]["function"]["arguments"]).get("command") != "echo hi":
                raise RuntimeError("Tool arguments were not replayed")
            if results[-1].get("tool_call_id") != "call_SMOKE0000001" or "hi" not in results[-1]["content"]:
                raise RuntimeError(f"Tool result did not reach the server: {results[-1]!r}")
            print("openai-chat two-turn tool call terminal smoke passed")
        finally:
            if status is None:
                os.kill(pid, signal.SIGKILL)
                os.waitpid(pid, 0)
            os.close(master)
            server.shutdown()
            server.server_close()
            thread.join(timeout=5)


if __name__ == "__main__":
    main(Path(sys.argv[1]).resolve(strict=True))

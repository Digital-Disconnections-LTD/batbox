#!/usr/bin/env python3
"""Run Batbox with no Scrapling and no SearXNG and call both web Gadgets.

A local Messages fixture asks for WebFetch, then WebSearch. Startup must not
need either helper, both calls must come back as clear tool errors naming
`batbox setup web`, and the run must finish well inside its timeout.
"""
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import threading
import time

FINAL = "WEB_UNAVAILABLE_FIXTURE_DONE"


def closed_port():
    with socket.socket() as probe:
        probe.bind(("127.0.0.1", 0))
        return probe.getsockname()[1]


def events(model, index, tool=None, text=None):
    block = ({"type": "tool_use", "id": tool[0], "name": tool[1], "input": {}}
             if tool else {"type": "text", "text": ""})
    delta = ({"type": "input_json_delta", "partial_json": json.dumps(tool[2])}
             if tool else {"type": "text_delta", "text": text})
    rows = [
        {"type": "message_start", "message": {"id": f"web-{index}", "type": "message",
         "role": "assistant", "model": model, "content": [], "stop_reason": None,
         "usage": {"input_tokens": 10, "output_tokens": 0}}},
        {"type": "content_block_start", "index": 0, "content_block": block},
        {"type": "content_block_delta", "index": 0, "delta": delta},
        {"type": "content_block_stop", "index": 0},
        {"type": "message_delta", "delta": {"stop_reason": "tool_use" if tool else "end_turn"},
         "usage": {"output_tokens": 5}},
        {"type": "message_stop"},
    ]
    return "".join(f"event: {row['type']}\ndata: {json.dumps(row)}\n\n" for row in rows).encode()


def tool_results(body):
    return {block.get("tool_use_id"): block for message in body.get("messages", [])
            for block in (message.get("content") if isinstance(message.get("content"), list) else [])
            if isinstance(block, dict) and block.get("type") == "tool_result"}


def main():
    if len(sys.argv) != 2:
        sys.exit("usage: indigenous_web_unavailable_smoke.py BATBOX_BINARY")
    binary = Path(sys.argv[1]).resolve(strict=True)
    requests, errors = [], []

    class Handler(BaseHTTPRequestHandler):
        def log_message(self, *_):
            pass

        def do_POST(self):
            try:
                body = json.loads(self.rfile.read(int(self.headers["Content-Length"])))
                requests.append(body)
                results = tool_results(body)
                if "fetch" not in results:
                    tool = ("fetch", "WebFetch", {"url": "https://example.org/page",
                                                  "prompt": "Summarize the page"})
                    response = events(body["model"], len(requests), tool=tool)
                elif "search" not in results:
                    tool = ("search", "WebSearch", {"query": "batbox web helpers"})
                    response = events(body["model"], len(requests), tool=tool)
                else:
                    response = events(body["model"], len(requests), text=FINAL)
                self.send_response(200)
                self.send_header("Content-Type", "text/event-stream")
                self.send_header("Content-Length", str(len(response)))
                self.end_headers()
                self.wfile.write(response)
            except Exception as error:
                errors.append(repr(error))
                self.send_error(500)

    server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    try:
        with tempfile.TemporaryDirectory(prefix="batbox-web-unavailable-") as temporary:
            root = Path(temporary)
            (root / "project").mkdir()
            config = root / "config"
            config.mkdir()
            (config / "native-client.json").write_text(json.dumps({
                "model": "web-fixture", "base_url": f"http://127.0.0.1:{server.server_port}",
                "searxng_url": f"http://127.0.0.1:{closed_port()}/search",
                "scrapling_command": str(root / "missing" / "scrapling"),
                "state_directory": str(root / "state"), "mcpServers": {}, "thinking": False,
                "context_window": 70000, "max_output_tokens": 1000, "compact_at": 60000}))
            env = {key: value for key, value in os.environ.items() if not key.startswith("BATBOX_")}
            env.update(BATBOX_CONFIG_DIR=str(config), HOME=str(root),
                       NO_PROXY="127.0.0.1,localhost", no_proxy="127.0.0.1,localhost")
            started = time.monotonic()
            result = subprocess.run([str(binary), "--cwd", str(root / "project"), "--no-thinking",
                                     "--print", "Fetch a page, then search."], env=env,
                                    stdin=subprocess.DEVNULL, text=True, capture_output=True,
                                    timeout=60)
            elapsed = time.monotonic() - started
    finally:
        server.shutdown()
        server.server_close()
    final = tool_results(requests[-1]) if requests else {}
    problems = list(errors)
    if result.returncode != 0 or FINAL not in result.stdout:
        problems.append(f"exit {result.returncode}: {result.stdout}{result.stderr}")
    for name, expected in (("fetch", "not installed"), ("search", "not reachable")):
        block = final.get(name)
        text = json.dumps(block)
        if not block or not block.get("is_error") or expected not in text or \
                "batbox setup web" not in text:
            problems.append(f"{name} result is not a clear tool error: {text}")
    described = {tool.get("name"): tool.get("description", "")
                 for tool in (requests[0].get("tools", []) if requests else [])}
    if "batbox setup web" not in described.get("WebFetch", ""):
        problems.append("WebFetch description does not tell the model it is unavailable")
    if "Unavailable" in described.get("WebSearch", ""):
        problems.append("WebSearch was described as unavailable with searxng_url set")
    if elapsed > 30:
        problems.append(f"run took {elapsed:.1f}s")
    if problems:
        sys.exit("\n".join(problems))
    print(f"web Gadgets without helpers fail clearly ({elapsed:.1f}s, {len(requests)} requests)")


if __name__ == "__main__":
    main()

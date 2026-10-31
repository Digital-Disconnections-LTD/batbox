#!/usr/bin/env python3
"""Exercise one owner-loop query through two local HTTP/SSE model turns."""
import argparse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import subprocess
import threading


class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, *_args):
        pass

    def do_POST(self):
        if self.path != "/v1/messages":
            self.send_error(404)
            return
        body = json.loads(self.rfile.read(int(self.headers["Content-Length"])))
        with self.server.lock:
            self.server.requests.append(body)
            turn = len(self.server.requests)
        events = [
            ("message_start", {"type": "message_start", "message": {
                "type": "message", "id": f"message-{turn}", "model": "batbox-model",
                "role": "assistant", "content": [], "stop_reason": None,
                "usage": {"input_tokens": 5, "output_tokens": 0}}}),
            ("content_block_start", {"type": "content_block_start", "index": 0,
                "content_block": {"type": "tool_use", "id": "tool-1",
                    "name": self.server.gadget_name, "input": {}}
                if turn == 1 else {"type": "text", "text": ""}}),
            ("content_block_delta", {"type": "content_block_delta", "index": 0,
                "delta": {"type": "input_json_delta", "partial_json":
                          json.dumps({"command": "printf 'hello from bash\\n'"})
                          if self.server.gadget_name == "Bash" else "{}"}
                if turn == 1 else {"type": "text_delta", "text": "The file says hello."}}),
            ("content_block_stop", {"type": "content_block_stop", "index": 0}),
            ("message_delta", {"type": "message_delta", "delta": {
                "stop_reason": "tool_use" if turn == 1 else "end_turn"},
                "usage": {"output_tokens": 3}}),
            ("message_stop", {"type": "message_stop"}),
        ]
        self.send_response(200)
        self.send_header("Content-Type", "text/event-stream")
        self.send_header("Request-Id", f"request-{turn}")
        self.send_header("Connection", "close")
        self.end_headers()
        for event_name, event in events:
            self.wfile.write(f"event: {event_name}\ndata: {json.dumps(event)}\n\n".encode())
            self.wfile.flush()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--probe", required=True)
    parser.add_argument("--gadget", "--tool", dest="gadget", choices=("Read", "Bash"), default="Read")
    args = parser.parse_args()
    server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    server.lock = threading.Lock()
    server.requests = []
    server.gadget_name = args.gadget
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    try:
        base = f"http://127.0.0.1:{server.server_address[1]}"
        run = subprocess.run([args.probe, base] + (["bash"] if args.gadget == "Bash" else []),
                             capture_output=True, text=True,
                             timeout=30)
        if run.returncode:
            raise AssertionError(f"probe exit {run.returncode}: {run.stderr[:2000]}")
        result = json.loads(run.stdout)
        assert len(server.requests) == 2, (server.requests, result)
        assert any(block.get("type") == "tool_result" for message in
                   server.requests[1]["messages"] for block in
                   message.get("content", [])), server.requests[1]
        assert result["toolCalls"] == 1, result
        assert result["budgetCalls"] == 2, result
        assert result["disposed"] == 1, result
        assert result["costCalls"] == 2, result
        assert result["lastMainRequestId"] == "request-2", result
        assert result["terminal"]["reason"] == "completed", result
        if args.gadget == "Bash":
            assert "hello from bash" in json.dumps(server.requests[1]), server.requests[1]
        assert any(message.get("type") == "assistant" for message in result["emitted"])
        print(f"owner-loop query, two live HTTP/SSE turns, typed budget and {args.gadget} dispatch passed")
    finally:
        server.shutdown()
        server.server_close()
        thread.join()


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Cancel a live SSE response blocked after headers and one partial event."""
import argparse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import subprocess
import threading
import time


class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, *_args):
        pass

    def do_POST(self):
        self.rfile.read(int(self.headers["Content-Length"]))
        self.send_response(200)
        self.send_header("Content-Type", "text/event-stream")
        self.send_header("Connection", "close")
        self.end_headers()
        event = {"type": "message_start", "message": {"type": "message",
                 "id": "blocked", "model": "batbox-model", "role": "assistant",
                 "content": [], "stop_reason": None,
                 "usage": {"input_tokens": 3, "output_tokens": 0}}}
        self.wfile.write(f"event: message_start\ndata: {json.dumps(event)}\n\n".encode())
        self.wfile.flush()
        time.sleep(4)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--probe", required=True)
    args = parser.parse_args()
    server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    try:
        base = f"http://127.0.0.1:{server.server_address[1]}"
        run = subprocess.run([args.probe, base], capture_output=True, text=True,
                             timeout=5)
        if run.returncode:
            raise AssertionError(f"probe exit {run.returncode}: {run.stderr[:1000]}")
        result = json.loads(run.stdout)
        assert result["stopRequested"], result
        assert result["elapsedMs"] < 2500, result
        assert result["emitted"], result
        print("live blocked HTTP/SSE producer cancelled while owner loop stayed responsive")
    finally:
        server.shutdown()
        server.server_close()
        thread.join()


if __name__ == "__main__":
    main()

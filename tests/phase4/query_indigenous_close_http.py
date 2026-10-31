#!/usr/bin/env python3
"""Close a paused native model generator while HTTP/SSE stays blocked."""
import argparse
from http.server import ThreadingHTTPServer
import json
import subprocess
import threading
from query_indigenous_blocked_http import Handler


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
        assert result["first"]["type"] in ("stream_event", "assistant"), result
        assert result["closeMs"] < 2500, result
        print("paused live HTTP/SSE model generator closed before stalled server completed")
    finally:
        server.shutdown()
        server.server_close()
        thread.join()


if __name__ == "__main__":
    main()

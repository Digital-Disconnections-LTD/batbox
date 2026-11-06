#!/usr/bin/env python3
"""Fake OpenAI Chat Completions server for the openai-chat wire client.

Each scenario is a path prefix: the probe uses base_url
http://127.0.0.1:PORT/<scenario> and posts to <scenario>/v1/chat/completions.
"""
import argparse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import subprocess
import threading


def chunk(delta=None, finish=None, usage=None, empty=False):
    body = {"id": "chatcmpl-fake", "object": "chat.completion.chunk", "created": 1,
            "model": "served-name", "choices": []}
    if not empty:
        body["choices"] = [{"index": 0, "delta": delta or {}, "finish_reason": finish}]
    if usage is not None:
        body["usage"] = usage
    return body


def tool(index, call_id=None, name=None, arguments=""):
    call = {"index": index, "function": {"arguments": arguments}}
    if call_id is not None:
        call["id"] = call_id
        call["type"] = "function"
    if name is not None:
        call["function"]["name"] = name
    return {"tool_calls": [call]}


USAGE = {"prompt_tokens": 11, "completion_tokens": 3, "total_tokens": 14}
STREAMS = {
    "text": [chunk({"role": "assistant", "content": ""}), chunk({"content": "Hello"}),
             chunk({"content": " there"}), chunk(finish="stop"), chunk(empty=True, usage=USAGE)],
    "tools": [chunk({"role": "assistant", "content": "Reading."}),
              chunk(tool(0, "call_READ00000001", "Read")),
              chunk(tool(0, arguments='{"file_')), chunk(tool(0, arguments='path":"/etc/hosts"}')),
              chunk(tool(1, "call_LIST00000001", "Bash", '{"comm')),
              chunk(tool(1, arguments='and":"ls"}')),
              chunk(finish="tool_calls"),
              chunk(empty=True, usage={"prompt_tokens": 40, "completion_tokens": 12,
                                       "prompt_tokens_details": {"cached_tokens": 8}})],
    "reasoning": [chunk({"role": "assistant", "reasoning_content": "Thinking"}),
                  chunk({"reasoning_content": " hard"}), chunk({"content": "Done"}),
                  chunk(finish="stop"), chunk(empty=True, usage=USAGE)],
    "usage-inline": [chunk({"content": "Hi"}), chunk(finish="stop", usage=USAGE)],
    "done-no-usage": [chunk({"content": "abcdefgh"}), chunk(finish="stop")],
}
NONSTREAM = {"id": "chatcmpl-plain", "object": "chat.completion", "created": 1, "model": "served-name",
             "choices": [{"index": 0, "finish_reason": "tool_calls", "message": {
                 "role": "assistant", "content": "Plain reply",
                 "tool_calls": [{"id": "call_PLAIN0000001", "type": "function",
                                 "function": {"name": "Read", "arguments": "{\"file_path\":\"/a\"}"}}]}}],
             "usage": {"prompt_tokens": 20, "completion_tokens": 6, "total_tokens": 26}}
MODELS = {"object": "list", "data": [{"id": "fake-model", "object": "model", "owned_by": "vllm",
                                      "max_model_len": 32768}, {"id": "other-model", "object": "model"}]}


class Server(ThreadingHTTPServer):
    daemon_threads = True

    def __init__(self):
        super().__init__(("127.0.0.1", 0), Handler)
        self.requests = []
        self.counts = {}
        self.lock = threading.Lock()


class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, *_):
        pass

    def record(self, body):
        scenario = self.path.strip("/").split("/")[0]
        with self.server.lock:
            self.server.requests.append({"method": self.command, "path": self.path, "scenario": scenario,
                                         "headers": {k.lower(): v for k, v in self.headers.items()},
                                         "body": body})
            self.server.counts[scenario] = self.server.counts.get(scenario, 0) + 1
            return scenario, self.server.counts[scenario]

    def reply(self, status, body, headers=()):
        data = json.dumps(body).encode()
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(data)))
        self.send_header("Connection", "close")
        for name, value in headers:
            self.send_header(name, value)
        self.end_headers()
        self.wfile.write(data)
        self.wfile.flush()

    def stream(self, raw):
        self.send_response(200)
        self.send_header("Content-Type", "text/event-stream")
        self.send_header("x-request-id", "req-fake-1")
        self.send_header("Connection", "close")
        self.end_headers()
        self.wfile.write(raw)
        self.wfile.flush()

    def do_GET(self):
        self.record(None)
        if self.path.endswith("/v1/models"):
            return self.reply(200, MODELS)
        self.reply(404, {"error": {"message": "no such path"}})

    def do_POST(self):
        body = json.loads(self.rfile.read(int(self.headers.get("Content-Length", "0"))) or b"null")
        scenario, count = self.record(body)
        if not self.path.endswith("/v1/chat/completions"):
            return self.reply(404, {"error": {"message": "no such path"}})
        if scenario == "unauthorized":
            return self.reply(401, {"error": {"message": "Invalid API key provided",
                                              "type": "invalid_request_error", "code": "invalid_api_key"}})
        if scenario == "server-error":
            return self.reply(500, {"error": {"message": "internal failure", "type": "server_error"}})
        if scenario == "ratelimit" and count <= 2:
            return self.reply(429, {"error": {"message": "Rate limit reached", "type": "rate_limit_error"}},
                              [("retry-after", "1")])
        if scenario == "nonstream":
            return self.reply(200, NONSTREAM)
        if scenario == "malformed":
            return self.stream(b'data: ' + json.dumps(chunk({"content": "a"})).encode() +
                               b'\n\ndata: {broken\n\n')
        chunks = STREAMS["text" if scenario == "ratelimit" else scenario]
        raw = b"".join(b"data: " + json.dumps(item).encode() + b"\n\n" for item in chunks)
        self.stream(raw + b": keep-alive comment\n\ndata: [DONE]\n\n")


def run(probe, base, scenario, mode, quirks=None):
    command = [probe, f"{base}/{scenario}", mode] + ([json.dumps(quirks)] if quirks else [])
    process = subprocess.run(command, capture_output=True, text=True, timeout=30)
    if process.returncode:
        raise AssertionError(f"{scenario}: exit {process.returncode}: {process.stderr[:800]}")
    return json.loads(process.stdout)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--probe", required=True)
    args = parser.parse_args()
    server = Server()
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    base = f"http://127.0.0.1:{server.server_address[1]}"
    origin = base

    def sent(scenario):
        return [request for request in server.requests if request["scenario"] == scenario]

    try:
        text = run(args.probe, base, "text", "stream")
        assert text["events"] == ["message_start", "content_block_start", "content_block_delta",
                                  "content_block_delta", "content_block_stop", "message_delta",
                                  "message_stop"], text["events"]
        assert text["start"]["message"]["model"] == "fake-model"
        assert text["messages"][0]["content"][0]["text"] == "Hello there"
        assert text["usage"]["input_tokens"] == 11 and text["usage"]["output_tokens"] == 3
        assert text["stopReason"] == "end_turn" and text["headers"]["request-id"] == "req-fake-1"
        request = sent("text")[0]
        assert request["headers"]["authorization"] == "Bearer test-key"
        assert request["headers"]["accept"] == "text/event-stream"
        chat = request["body"]
        assert chat["model"] == "fake-model" and chat["stream"] is True
        assert chat["stream_options"] == {"include_usage": True} and chat["max_tokens"] == 512
        assert chat["messages"][0] == {"role": "system", "content": "You are Batbox."}
        assert chat["messages"][1] == {"role": "user", "content": "Run it"}
        prior = chat["messages"][2]["tool_calls"][0]
        assert prior["function"] == {"name": "Bash", "arguments": "{\"command\":\"pwd\"}"}
        assert chat["messages"][3] == {"role": "tool", "tool_call_id": prior["id"], "content": "/tmp"}
        assert chat["messages"][4] == {"role": "user", "content": "Now read and list"}
        assert [t["function"]["name"] for t in chat["tools"]] == ["Read", "Bash"]
        assert chat["tools"][0]["type"] == "function" and "parameters" in chat["tools"][0]["function"]

        tools = run(args.probe, base, "tools", "stream")
        assert [m["content"][0]["type"] for m in tools["messages"]] == ["text", "tool_use", "tool_use"]
        read, listing = tools["messages"][1]["content"][0], tools["messages"][2]["content"][0]
        assert read["id"] == "call_READ00000001" and read["input"] == {"file_path": "/etc/hosts"}
        assert listing["id"] == "call_LIST00000001" and listing["input"] == {"command": "ls"}
        assert tools["stopReason"] == "tool_use"
        assert tools["usage"]["input_tokens"] == 32 and tools["usage"]["cache_read_input_tokens"] == 8

        reasoning = run(args.probe, base, "reasoning", "stream")
        assert reasoning["messages"][0]["content"][0] == {"type": "thinking", "thinking": "Thinking hard",
                                                         "signature": ""}
        assert reasoning["messages"][1]["content"][0]["text"] == "Done"

        inline = run(args.probe, base, "usage-inline", "stream")
        assert inline["usage"]["output_tokens"] == 3 and inline["messages"][0]["content"][0]["text"] == "Hi"

        missing = run(args.probe, base, "done-no-usage", "stream", {"usage_in_stream": False})
        assert missing["usage"]["output_tokens"] == 2 and missing["usage"]["input_tokens"] > 0
        assert "stream_options" not in sent("done-no-usage")[0]["body"]

        plain = run(args.probe, base, "nonstream", "nonstream", {"max_tokens_field": "max_completion_tokens"})
        assert plain["status"] == 200
        body = plain["body"]
        assert body["type"] == "message" and body["model"] == "fake-model"
        assert body["content"][0] == {"type": "text", "text": "Plain reply"}
        assert body["content"][1]["input"] == {"file_path": "/a"} and body["stop_reason"] == "tool_use"
        assert body["usage"]["input_tokens"] == 20 and body["usage"]["output_tokens"] == 6
        plain_request = sent("nonstream")[0]["body"]
        assert plain_request["stream"] is False and plain_request["max_completion_tokens"] == 512
        assert "max_tokens" not in plain_request and "stream_options" not in plain_request

        unauthorized = run(args.probe, base, "unauthorized", "retry")
        assert unauthorized["attempts"] == 1 and len(sent("unauthorized")) == 1, unauthorized
        assert unauthorized["error"].startswith(f"401 from {origin}: check auth_token"), unauthorized["error"]
        assert "Invalid API key provided" in unauthorized["error"]

        limited = run(args.probe, base, "ratelimit", "retry")
        assert limited["attempts"] == 3 and len(sent("ratelimit")) == 3, limited
        assert limited["messages"][0]["content"][0]["text"] == "Hello there"
        assert 1800 <= limited["elapsedMs"] < 6000, limited["elapsedMs"]

        failed = run(args.probe, base, "server-error", "retry")
        assert failed["attempts"] == 3 and failed["status"] == 500, failed
        assert "internal failure" in failed["error"] and "api_error" in failed["error"]

        broken = run(args.probe, base, "malformed", "stream")
        assert "Malformed stream chunk" in broken["error"], broken

        direct = run(args.probe, base, "unauthorized", "stream")
        assert direct["status"] == 401 and direct["body"]["error"]["type"] == "authentication_error"

        models = run(args.probe, base, "models", "models")
        assert models == {"data": [{"id": "fake-model", "context_window": 32768},
                                   {"id": "other-model"}]}
        listing_request = sent("models")[0]
        assert listing_request["method"] == "GET" and listing_request["path"] == "/models/v1/models"
        assert listing_request["headers"]["authorization"] == "Bearer test-key"
        print("OpenAI Chat fake server: text, parallel tools, reasoning, usage, non-stream, 401, 429, "
              "500, malformed and model list checks passed")
    finally:
        server.shutdown()
        server.server_close()
        thread.join()


if __name__ == "__main__":
    main()

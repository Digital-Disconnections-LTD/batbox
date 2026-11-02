#!/usr/bin/env python3
"""Fake local model servers for LocalDiscovery: Ollama, LM Studio, llama.cpp,
vLLM, an unidentified OpenAI-style server, a server that hangs, and a closed
port. Each listens on a random loopback port; the C++ probe must find the
first five, report their models, and give up on the rest within its timeout.
"""
import argparse
import json
import socket
import subprocess
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

ROUTES = {
    "ollama": {"/api/tags": {"models": [{"name": "qwen3-coder:30b", "model": "qwen3-coder:30b"},
                                        {"name": "gemma4:31b", "model": "gemma4:31b"},
                                        {"name": "llama3.3:70b"}]}},
    "lmstudio": {"/api/v1/models": {"models": [
        {"type": "llm", "key": "openai/gpt-oss-20b", "display_name": "GPT OSS 20B",
         "max_context_length": 131072},
        {"type": "embedding", "key": "text-embedding-nomic"}]}},
    "llama.cpp": {"/v1/models": {"object": "list", "data": [{"id": "Qwen3-8B-Q4_K_M.gguf",
                                                             "object": "model"}]},
                  "/props": {"build_info": "b7190", "model_path": "/models/Qwen3-8B-Q4_K_M.gguf",
                             "default_generation_settings": {"n_ctx": 32768}}},
    "vllm": {"/v1/models": {"object": "list", "data": [
        {"id": "Qwen/Qwen3-Coder-30B-A3B", "owned_by": "vllm", "max_model_len": 65536}]}},
    "generic": {"/v1/models": {"data": [{"id": "mystery-model"}]}},
}


def handler_for(kind):
    class Handler(BaseHTTPRequestHandler):
        def log_message(self, *args):
            pass

        def do_GET(self):
            if kind == "hang":
                time.sleep(3)
            route = ROUTES.get(kind, {}).get(self.path)
            if route is None:
                self.send_response(404)
                self.end_headers()
                return
            body = json.dumps(route).encode()
            self.send_response(200)
            self.send_header("content-type", "application/json")
            self.send_header("content-length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
    return Handler


def serve(kind):
    server = ThreadingHTTPServer(("127.0.0.1", 0), handler_for(kind))
    server.daemon_threads = True
    threading.Thread(target=server.serve_forever, daemon=True).start()
    return server


def closed_port():
    sock = socket.socket()
    sock.bind(("127.0.0.1", 0))
    port = sock.getsockname()[1]
    sock.close()
    return port


def check(condition, message):
    if not condition:
        raise SystemExit("FAIL: " + message)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--probe", required=True)
    arguments = parser.parse_args()
    servers = {kind: serve(kind) for kind in ("ollama", "lmstudio", "llama.cpp", "vllm",
                                              "generic", "hang")}
    port = {kind: server.server_address[1] for kind, server in servers.items()}
    targets = [f"ollama:{port['ollama']}", f"lmstudio:{port['lmstudio']}",
               f"llama.cpp:{port['llama.cpp']}",
               # vLLM identifies itself through owned_by even on another port.
               f"llama.cpp:{port['vllm']}",
               f"llama.cpp:{port['generic']}", f"vllm:{port['hang']}",
               f"ollama:{closed_port()}"]
    result = subprocess.run([arguments.probe, *targets], capture_output=True, text=True,
                            timeout=30)
    check(result.returncode == 0, "probe failed: " + result.stderr)
    report = json.loads(result.stdout)
    found = {server["port"]: server for server in report["servers"]}
    check(len(found) == 5, "expected five servers: " + result.stdout)
    ollama = found[port["ollama"]]
    check(ollama["preset"] == "ollama" and ollama["models"] ==
          ["qwen3-coder:30b", "gemma4:31b", "llama3.3:70b"], "Ollama models")
    check(ollama["summary"] == "ollama with 3 models", "Ollama summary " + ollama["summary"])
    lmstudio = found[port["lmstudio"]]
    check(lmstudio["models"] == ["openai/gpt-oss-20b", "text-embedding-nomic"] and
          lmstudio["contexts"] == {"openai/gpt-oss-20b": 131072}, "LM Studio models")
    llama = found[port["llama.cpp"]]
    check(llama["preset"] == "llama.cpp" and llama["confirmed"] and
          llama["contexts"] == {"Qwen3-8B-Q4_K_M.gguf": 32768}, "llama.cpp identity")
    check(llama["summary"] == f"llama.cpp on :{port['llama.cpp']} with 1 model", llama["summary"])
    vllm = found[port["vllm"]]
    check(vllm["preset"] == "vllm" and vllm["name"] == "vLLM", "vLLM identity")
    check(vllm["contexts"] == {"Qwen/Qwen3-Coder-30B-A3B": 65536}, "vLLM context")
    generic = found[port["generic"]]
    check(not generic["confirmed"] and "unidentified" in generic["summary"], "unidentified server")
    check(port["hang"] not in found, "a hanging server must time out")
    check(report["elapsed_ms"] < 2000, f"probes must run in parallel: {report['elapsed_ms']} ms")
    for server in servers.values():
        server.shutdown()
    print("local discovery found", len(found), "fake servers in", report["elapsed_ms"], "ms")


if __name__ == "__main__":
    main()

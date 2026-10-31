#!/usr/bin/env python3
"""Small stdio language server fixture for the grouped native runtime check."""
import json
import sys


def receive():
    headers = {}
    while True:
        line = sys.stdin.buffer.readline()
        if not line:
            return None
        if line == b"\r\n":
            break
        name, _, value = line.partition(b":")
        headers[name.lower()] = value.strip()
    size = int(headers[b"content-length"])
    body = sys.stdin.buffer.read(size)
    return json.loads(body)


def send(message):
    body = json.dumps(message, ensure_ascii=False).encode("utf-8")
    sys.stdout.buffer.write(f"Content-Length: {len(body)}\r\n\r\n".encode("ascii") + body)
    sys.stdout.buffer.flush()


opened = {}
while (message := receive()) is not None:
    method = message.get("method")
    params = message.get("params") or {}
    if method == "initialize":
        send({"jsonrpc": "2.0", "id": message["id"],
              "result": {"capabilities": {"definitionProvider": True,
                                          "referencesProvider": True,
                                          "hoverProvider": True}}})
    elif method in ("textDocument/didOpen", "textDocument/didChange"):
        document = params["textDocument"]
        uri = document["uri"]
        text = (document.get("text") if method.endswith("didOpen")
                else params["contentChanges"][0]["text"])
        opened[uri] = text
        send({"jsonrpc": "2.0", "method": "textDocument/publishDiagnostics",
              "params": {"uri": uri, "diagnostics": [
                  {"message": "fixture diagnostic", "severity": 2,
                   "range": {"start": {"line": 0, "character": 0},
                             "end": {"line": 0, "character": 1}}}]}})
    elif method == "textDocument/didClose":
        opened.pop(params["textDocument"]["uri"], None)
    elif method == "textDocument/definition":
        uri = params["textDocument"]["uri"]
        send({"jsonrpc": "2.0", "id": message["id"], "result": {
            "uri": uri, "range": {"start": {"line": 0, "character": 4},
                                  "end": {"line": 0, "character": 8}}}})
    elif method == "textDocument/hover":
        send({"jsonrpc": "2.0", "id": message["id"],
              "result": {"contents": {"kind": "markdown", "value": "fixture hover"}}})
    elif method == "shutdown":
        send({"jsonrpc": "2.0", "id": message["id"], "result": None})
    elif method == "exit":
        break
    elif "id" in message:
        send({"jsonrpc": "2.0", "id": message["id"], "result": None})

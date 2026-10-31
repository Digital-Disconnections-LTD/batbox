#!/usr/bin/env python3
"""Exercise native startup, streaming, a real file tool and clean shutdown.

Runs on Linux, macOS and Windows. A local fixture supplies Messages responses;
no inference credentials or external backend are needed.
"""
import argparse
import http.server
import json
import os
from pathlib import Path
import subprocess
import tempfile
import threading


def run(binary, skill_shell=False, mission_board=False, scrapling_fixture=None):
    with tempfile.TemporaryDirectory(prefix="batbox-indigenous-") as temporary:
        root = Path(temporary).resolve() / "workspace-雪"
        root.mkdir()
        source = root / "hello-雪.txt"
        source.write_text("Native tool result: café 雪\n", encoding="utf-8")
        if skill_shell:
            skill = root / ".batbox" / "skills" / "native-fixture" / "SKILL.md"
            skill.parent.mkdir(parents=True)
            skill.write_text("---\nname: native-fixture\ndescription: Native shell expansion fixture\n"
                "allowed-tools: Bash\n---\nRead hello-雪.txt.\nShell data: !`printf NATIVE-SKILL-SHELL`\n",
                encoding="utf-8")
        steps = [
            ("TaskCreate", {"subject": "Build", "description": "Build the package"}),
            ("TaskCreate", {"subject": "Publish", "description": "Publish after build"}),
            ("TaskUpdate", {"taskId": "1", "owner": "rook", "status": "in_progress", "addBlocks": ["2"]}),
            ("TaskGet", {"taskId": "2"}),
            ("TaskUpdate", {"taskId": "1", "status": "completed"}),
            ("TaskList", {}),
        ] if mission_board else [("Read", {"file_path": str(source)})]
        requests = []
        errors = []

        class Handler(http.server.BaseHTTPRequestHandler):
            def log_message(self, *_):
                pass

            def do_POST(self):
                try:
                    request = json.loads(self.rfile.read(int(self.headers["Content-Length"])))
                    requests.append(request)
                    if self.path != "/v1/messages":
                        raise AssertionError("Unexpected Messages URL")
                    results = [block for message in request.get("messages", [])
                               for block in (message.get("content", [])
                                   if isinstance(message.get("content"), list) else [])
                               if block.get("type") == "tool_result"]
                    index = len(requests) - 1
                    final = index >= len(steps)
                    if any(row.get("is_error") for row in results):
                        raise AssertionError("Native tool reported an error: " + json.dumps(results))
                    if skill_shell and len(requests) == 1:
                        poetry = json.dumps(request.get("messages", []), ensure_ascii=False)
                        if "Shell data: NATIVE-SKILL-SHELL" not in poetry:
                            raise AssertionError("First-prompt skill shell expansion missing")
                    if final:
                        if any(row.get("is_error") for row in results):
                            raise AssertionError("Read tool reported an error: " + json.dumps(results))
                        if not mission_board and "café 雪" not in json.dumps(results, ensure_ascii=False):
                            raise AssertionError("Unicode file content missing from tool result")
                    elif not any(gadget.get("name") == steps[index][0] for gadget in request.get("tools", [])):
                        raise AssertionError(steps[index][0] + " missing from native tool catalog")
                    block = ({"type": "text", "text": ""} if final else
                             {"type": "tool_use", "id": "native_" + str(index), "name": steps[index][0], "input": {}})
                    delta = ({"type": "text_delta", "text": "Native platform round trip complete."}
                             if final else {"type": "input_json_delta",
                               "partial_json": json.dumps(steps[index][1])})
                    events = [
                        ("message_start", {"type": "message_start", "message": {
                            "id": "native_" + str(len(requests)), "type": "message", "role": "assistant",
                            "model": request["model"], "content": [], "stop_reason": None,
                            "usage": {"input_tokens": 20, "output_tokens": 0}}}),
                        ("content_block_start", {"type": "content_block_start", "index": 0,
                            "content_block": block}),
                        ("content_block_delta", {"type": "content_block_delta", "index": 0, "delta": delta}),
                        ("content_block_stop", {"type": "content_block_stop", "index": 0}),
                        ("message_delta", {"type": "message_delta", "delta": {
                            "stop_reason": "end_turn" if final else "tool_use", "stop_sequence": None},
                            "usage": {"output_tokens": 10}}),
                        ("message_stop", {"type": "message_stop"}),
                    ]
                    body = "".join("event: " + name + "\ndata: " + json.dumps(data) + "\n\n"
                                   for name, data in events).encode()
                    self.send_response(200)
                    self.send_header("Content-Type", "text/event-stream")
                    self.send_header("Content-Length", str(len(body)))
                    self.end_headers()
                    self.wfile.write(body)
                except Exception as error:
                    errors.append(str(error))
                    self.send_error(500)

        server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        config = root / "config-雪"
        config.mkdir()
        (config / "native-client.json").write_text(json.dumps({
            "model": "fixture", "base_url": "http://127.0.0.1:" + str(server.server_port),
            "context_window": 16000, "max_output_tokens": 2000, "compact_at": 12000,
            "state_directory": str(root / "state-雪"), "mcpServers": {},
            **({"scrapling_command": str(scrapling_fixture)} if scrapling_fixture else {}),
        }), encoding="utf-8")
        env = {key: value for key, value in os.environ.items() if not key.startswith("BATBOX_")}
        env["BATBOX_CONFIG_DIR"] = str(config)
        if mission_board:
            env["BATBOX_ENABLE_TASKS"] = "1"
            env["BATBOX_TASK_LIST_ID"] = "platform-fixture"
        try:
            subprocess.run([str(binary), "--help"], env=env, check=True,
                           stdout=subprocess.DEVNULL, timeout=30)
            result = subprocess.run([str(binary), "--cwd", str(root), "--print",
                "/native-fixture" if skill_shell else "Exercise the native tools."], env=env, capture_output=True,
                text=True, encoding="utf-8", errors="replace", timeout=90)
        finally:
            server.shutdown()
            server.server_close()
            thread.join(timeout=5)
        if errors or result.returncode or "Native platform round trip complete." not in result.stdout:
            raise RuntimeError(json.dumps({"errors": errors, "exit": result.returncode,
                "stdout": result.stdout, "stderr": result.stderr}, ensure_ascii=False))
        if len(requests) != len(steps) + 1:
            raise RuntimeError("Unexpected round trip count: " + str(len(requests)))
        if mission_board:
            board = config / "tasks" / "platform-fixture"
            first = json.loads((board / "1.json").read_text(encoding="utf-8"))
            second = json.loads((board / "2.json").read_text(encoding="utf-8"))
            if first["owner"] != "rook" or first["status"] != "completed" or first["blocks"] != ["2"]:
                raise RuntimeError("Task ownership/status/dependencies not persisted")
            if second["blockedBy"] != ["1"]:
                raise RuntimeError("Reciprocal task dependency missing")
        print(json.dumps({"passed": True, "platform": os.name, "requests": len(requests),
                          "tools": sorted({name for name, _ in steps}), "unicodePaths": True, "skillShell": skill_shell}))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary", type=Path)
    parser.add_argument("--skill-shell", action="store_true")
    parser.add_argument("--mission-board", "--task-board", dest="mission_board", action="store_true")
    parser.add_argument("--scrapling-fixture", type=Path)
    args = parser.parse_args()
    run(args.binary.resolve(strict=True), args.skill_shell, args.mission_board,
        args.scrapling_fixture.resolve(strict=True) if args.scrapling_fixture else None)

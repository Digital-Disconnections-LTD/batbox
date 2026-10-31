#!/usr/bin/env python3
"""Exercise child Bat permission inheritance with local Messages/SearXNG fixtures.

No inference credentials or external services are used. The binary runs with an
isolated configuration and state directory; failures retain fixture artifacts.
"""
import argparse
from dataclasses import dataclass
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path
import re
import shutil
import signal
import subprocess
import tempfile
import threading
from urllib.parse import parse_qs, urlsplit


SEARCH_MARKER = "PERMISSION_FIXTURE_SEARCH_RESULT"
CHILD_SYSTEM = "PERMISSION_FIXTURE_CHILD_SYSTEM"
FINAL_MARKER = "PERMISSION_FIXTURE_COMPLETE"


@dataclass(frozen=True)
class Case:
    name: str
    background: bool
    config_mode: str | None = None
    deny: bool = False
    cli_mode: str | None = None

    @property
    def expect_search(self):
        return not self.deny and self.cli_mode != "default"


CASES = [
    Case(f"{policy}-{kind}", background, config_mode=mode, deny=deny, cli_mode=cli)
    for policy, mode, deny, cli in (
        ("explicit-bypass", "bypassPermissions", False, None),
        ("explicit-deny", "bypassPermissions", True, None),
        ("automatic-print", None, False, None),
        ("restrictive-print", None, False, "default"),
    )
    for kind, background in (("foreground", False), ("background", True))
]


def gadget_results(body):
    return {block.get("tool_use_id"): block
            for message in body.get("messages", [])
            for block in (message.get("content") if isinstance(message.get("content"), list) else [])
            if isinstance(block, dict) and block.get("type") == "tool_result"}


def child_id(result):
    content = result.get("content")
    texts = [content] if isinstance(content, str) else [
        item.get("text", "") for item in content or [] if isinstance(item, dict)]
    for text in texts:
        try:
            data = json.loads(text)
            if isinstance(data, dict) and data.get("agentId"):
                return data["agentId"]
        except (TypeError, ValueError):
            pass
    match = re.search(r"[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}", str(content))
    if not match:
        raise AssertionError("Background launch did not return a child ID: " + str(content))
    return match.group()


def stream_response(model, index, gadget=None, gadget_id=None, text=None):
    block = ({"type": "tool_use", "id": gadget_id, "name": gadget[0], "input": {}}
             if gadget else {"type": "text", "text": ""})
    delta = ({"type": "input_json_delta", "partial_json": json.dumps(gadget[1])}
             if gadget else {"type": "text_delta", "text": text})
    events = [
        {"type": "message_start", "message": {"id": f"permission-{index}",
         "type": "message", "role": "assistant", "model": model, "content": [],
         "stop_reason": None, "usage": {"input_tokens": 20, "output_tokens": 0}}},
        {"type": "content_block_start", "index": 0, "content_block": block},
        {"type": "content_block_delta", "index": 0, "delta": delta},
        {"type": "content_block_stop", "index": 0},
        {"type": "message_delta", "delta": {"stop_reason": "tool_use" if gadget else "end_turn"},
         "usage": {"output_tokens": 10}},
        {"type": "message_stop"},
    ]
    return "".join("event: " + event["type"] + "\ndata: " + json.dumps(event) + "\n\n"
                   for event in events).encode()


def terminate_fixture_process(process, expired):
    """Bound cleanup even if a descendant retains the captured output pipes."""
    warnings = []
    if os.name == "nt":
        try:
            subprocess.run(["taskkill", "/PID", str(process.pid), "/T", "/F"],
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                           timeout=5, check=False)
        except (OSError, subprocess.TimeoutExpired) as error:
            warnings.append("Process-tree cleanup: " + str(error))
    else:
        try:
            os.killpg(process.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
        except OSError as error:
            warnings.append("Process-group cleanup: " + str(error))
    if process.poll() is None:
        try:
            process.kill()
        except OSError as error:
            warnings.append("Parent cleanup: " + str(error))
    try:
        output, error_output = process.communicate(timeout=5)
    except subprocess.TimeoutExpired as drain_error:
        warnings.append("Output pipes did not close within the cleanup deadline")
        # Do not close pipes here: a Windows reader thread may hold their locks.
        # The runner must report the original timeout rather than wait forever.
        output = drain_error.output if drain_error.output is not None else expired.output
        error_output = drain_error.stderr if drain_error.stderr is not None else expired.stderr
    except OSError as error:
        warnings.append("Output cleanup: " + str(error))
        output, error_output = expired.output, expired.stderr
    def text(value):
        return value.decode("utf-8", errors="replace") if isinstance(value, bytes) else value or ""
    return text(output), text(error_output), warnings


def run_case(binary, case, root, timeout, scrapling_fixture):
    root.mkdir()
    work = root / "project"
    worker = work / ".batbox/agents/permission-worker.md"
    worker.parent.mkdir(parents=True)
    worker.write_text("---\nname: permission-worker\ndescription: Permission inheritance fixture\n---\n"
                      + CHILD_SYSTEM + "\n", encoding="utf-8")
    requests, searches, errors = [], [], []
    observations = {"childRequests": 0, "childResult": None}
    lock = threading.Lock()

    class Handler(BaseHTTPRequestHandler):
        def log_message(self, *_):
            pass

        def do_GET(self):
            try:
                url = urlsplit(self.path)
                query = parse_qs(url.query)
                if url.path != "/search" or query.get("format") != ["json"]:
                    raise AssertionError("Unexpected SearXNG request: " + self.path)
                with lock:
                    searches.append(query)
                body = json.dumps({"results": [{"title": SEARCH_MARKER,
                                   "url": "https://example.invalid/permission-fixture"}]}).encode()
                self.send_response(200)
                self.send_header("Content-Type", "application/json")
                self.send_header("Content-Length", str(len(body)))
                self.end_headers()
                self.wfile.write(body)
            except Exception as error:
                errors.append(str(error))
                self.send_error(500)

        def do_POST(self):
            try:
                if self.path != "/v1/messages":
                    raise AssertionError("Unexpected Messages request: " + self.path)
                body = json.loads(self.rfile.read(int(self.headers["Content-Length"])))
                with lock:
                    requests.append(body)
                    index = len(requests)
                if index > 12:
                    raise AssertionError("Fixture exceeded its expected turn count")
                results = gadget_results(body)
                child = CHILD_SYSTEM in json.dumps(body.get("system", []))
                gadget, gadget_id, text = None, None, None
                if child:
                    observations["childRequests"] += 1
                    if "child-search" not in results:
                        gadget = ("WebSearch", {"query": "batbox permission inheritance fixture"})
                        gadget_id = "child-search"
                    else:
                        result = results["child-search"]
                        observations["childResult"] = result
                        result_text = json.dumps(result)
                        if case.expect_search:
                            if result.get("is_error") or SEARCH_MARKER not in result_text:
                                raise AssertionError("Bypass child search failed: " + result_text)
                        elif not result.get("is_error") or not re.search(
                                r"permission|denied|not allowed", result_text, re.I):
                            raise AssertionError("Child search was not denied: " + result_text)
                        text = "CHILD_SEARCH_ALLOWED" if case.expect_search else "CHILD_SEARCH_DENIED"
                elif "launch-child" not in results:
                    gadget = ("Agent", {"name": "permission-worker", "subagent_type": "permission-worker",
                        "description": "Check inherited search permission", "prompt": "Run the fixture WebSearch.",
                        "run_in_background": case.background})
                    gadget_id = "launch-child"
                elif case.background and "wait-child" not in results:
                    if results["launch-child"].get("is_error"):
                        raise AssertionError("Child launch failed: " + json.dumps(results["launch-child"]))
                    gadget = ("TaskOutput", {"task_id": child_id(results["launch-child"]),
                                             "block": True, "timeout": 15000})
                    gadget_id = "wait-child"
                else:
                    if observations["childResult"] is None:
                        raise AssertionError("Parent completed without a child WebSearch result")
                    if any(item.get("is_error") for item in results.values()):
                        raise AssertionError("Parent Gadget failed: " + json.dumps(results))
                    text = FINAL_MARKER
                response = stream_response(body["model"], index, gadget, gadget_id, text)
                self.send_response(200)
                self.send_header("Content-Type", "text/event-stream")
                self.send_header("Content-Length", str(len(response)))
                self.end_headers()
                self.wfile.write(response)
            except Exception as error:
                errors.append(str(error))
                self.send_error(500)

    server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    server.daemon_threads = True
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    config_dir = root / "config"
    config_dir.mkdir()
    # Allow launching/waiting so restrictive cases test the child's WebSearch decision.
    permissions = {"allow": ["Agent", "TaskOutput"]}
    if case.config_mode:
        permissions["defaultMode"] = case.config_mode
    if case.deny:
        permissions["deny"] = ["WebSearch"]
    endpoint = f"http://127.0.0.1:{server.server_port}"
    config = {"model": "permission-fixture", "base_url": endpoint,
              "searxng_url": endpoint + "/search", "state_directory": str(root / "state"),
              "mcpServers": {}, "thinking": False, "permissions": permissions,
              "context_window": 70000, "max_output_tokens": 1000, "compact_at": 60000}
    if scrapling_fixture:
        config["scrapling_command"] = str(scrapling_fixture)
    (config_dir / "native-client.json").write_text(json.dumps(config), encoding="utf-8")
    env = {key: value for key, value in os.environ.items() if not key.startswith("BATBOX_")}
    env["BATBOX_CONFIG_DIR"] = str(config_dir)
    # Proxies must not redirect the fixture's loopback traffic.
    env["NO_PROXY"] = env["no_proxy"] = "127.0.0.1,localhost"
    command = [str(binary), "--cwd", str(work), "--no-thinking", "--print"]
    if case.cli_mode:
        command += ["--permission-mode", case.cli_mode]
    command += ["Run the permission inheritance fixture."]
    output, error_output, timed_out, code = "", "", False, None
    cleanup_warnings = []
    try:
        process = subprocess.Popen(command, env=env, stdin=subprocess.DEVNULL,
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, encoding="utf-8",
            errors="replace", start_new_session=os.name != "nt")
        try:
            output, error_output = process.communicate(timeout=timeout)
        except subprocess.TimeoutExpired as expired:
            timed_out = True
            output, error_output, cleanup_warnings = terminate_fixture_process(process, expired)
        code = process.returncode
    finally:
        server.shutdown()
        server.server_close()
        thread.join(timeout=5)
        (root / "requests.json").write_text(json.dumps(requests, indent=2), encoding="utf-8")
        (root / "observations.json").write_text(json.dumps({**observations, "searches": searches,
            "errors": errors}, indent=2), encoding="utf-8")
        (root / "stdout.txt").write_text(output, encoding="utf-8")
        (root / "stderr.txt").write_text(error_output, encoding="utf-8")
    failure = (timed_out or code != 0 or errors or FINAL_MARKER not in output or
               observations["childRequests"] != 2 or observations["childResult"] is None or
               len(searches) != int(case.expect_search))
    result = {"case": case.name, "passed": not bool(failure), "timedOut": timed_out,
              "exit": code, "modelRequests": len(requests), "searchRequests": len(searches),
              "childRequests": observations["childRequests"], "errors": errors,
              "cleanupWarnings": cleanup_warnings}
    (root / "result.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary", type=Path)
    parser.add_argument("--scrapling-fixture", type=Path)
    parser.add_argument("--case", action="append", choices=[case.name for case in CASES])
    parser.add_argument("--timeout", type=float, default=25)
    parser.add_argument("--keep-artifacts", action="store_true")
    args = parser.parse_args()
    binary = args.binary.resolve(strict=True)
    scrapling = args.scrapling_fixture.resolve(strict=True) if args.scrapling_fixture else None
    if scrapling is None:
        sibling = binary.with_name("batbox-scrapling-fixture" + (".exe" if os.name == "nt" else ""))
        if sibling.is_file():
            scrapling = sibling
    if scrapling is None:
        parser.error("Supply --scrapling-fixture or place batbox-scrapling-fixture beside the binary")
    root = Path(tempfile.mkdtemp(prefix="batbox-permission-check-")).resolve()
    results = []
    for case in CASES:
        if args.case and case.name not in args.case:
            continue
        result = run_case(binary, case, root / case.name, args.timeout, scrapling)
        results.append(result)
        print(json.dumps(result), flush=True)
    passed = all(result["passed"] for result in results)
    if not passed or args.keep_artifacts:
        print(json.dumps({"passed": passed, "artifacts": str(root)}), flush=True)
    else:
        shutil.rmtree(root)
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())

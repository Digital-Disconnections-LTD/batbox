#!/usr/bin/env python3
"""system.md in the configuration folder replaces the system prompt head.

With no file the fake model server receives the built-in head, and `batbox
poetry export` writes poetry/system.md unchanged, which sends the same bytes.
A custom system.md arrives as the head with {{section:NAME}} rendered in
place, other sections appended, comments removed and an unknown placeholder
left as text and logged once. `batbox poetry` and `batbox doctor` report the
source; an empty file falls back to the built-in head; BATBOX_SIMPLE wins.
In the terminal, /poetry shows the source. With the default stable prompt
prefix, edits to system.md and BATBOX.md between two turns leave the second
request's system prompt and first message byte for byte the same, the
BATBOX.md edit arrives at the end of the conversation, and /clear applies the
system.md edit.
"""
import json
import os
import re
import subprocess
import sys
import tempfile
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

from pty_smoke import PtySession

REPOSITORY = Path(__file__).resolve().parent.parent
CUSTOM = ("<!-- smoke: this comment is not sent -->\n"
          "SMOKE_HEAD custom system prompt.\n\n"
          "{{section:env_info_simple}}\n\n"
          "SMOKE_TAIL after the environment. {{section:nope}}\n")


def check(condition, message):
    if not condition:
        raise SystemExit("FAIL: " + message)


def stream(model, text):
    rows = [
        {"type": "message_start", "message": {"id": "system-poetry", "type": "message",
         "role": "assistant", "model": model, "content": [], "stop_reason": None,
         "usage": {"input_tokens": 5, "output_tokens": 0}}},
        {"type": "content_block_start", "index": 0, "content_block": {"type": "text", "text": ""}},
        {"type": "content_block_delta", "index": 0, "delta": {"type": "text_delta", "text": text}},
        {"type": "content_block_stop", "index": 0},
        {"type": "message_delta", "delta": {"stop_reason": "end_turn"}, "usage": {"output_tokens": 3}},
        {"type": "message_stop"},
    ]
    return "".join(f"event: {r['type']}\ndata: {json.dumps(r)}\n\n" for r in rows).encode()


def without_breakpoints(message):
    """The message as the model reads it: cache_control marks only move."""
    def strip(value):
        if isinstance(value, dict):
            return {key: strip(item) for key, item in value.items() if key != "cache_control"}
        if isinstance(value, list):
            return [strip(item) for item in value]
        return value
    return json.dumps(strip(message), sort_keys=True)


def terminal(binary, project, env, system_file, requests):
    session = PtySession([str(binary), "--cwd", str(project)], env, rows=40, columns=120)

    def plain():
        return re.sub(rb"\x1b\[[0-9;?]*[A-Za-z]|\x1b\][^\x07]*\x07", b"",
                      bytes(session.output)).decode(errors="replace")

    def request_for(text):
        return next(r for r in reversed(requests) if text in json.dumps(r.get("messages", [])))

    def turn(text):
        session.send(text.encode() + b"\r")
        session.wait_for(lambda: any(text in json.dumps(r.get("messages", [])) for r in requests),
                         f"the request for {text!r}")
        session.drain(0.5)
        return "\n\n".join(block["text"] for block in request_for(text)["system"])

    try:
        session.wait_for(lambda: b"\x1b[?2004h" in session.output, "terminal startup")
        session.drain(0.3)
        before = len(requests)
        session.send(b"/poetry\r")
        session.wait_for(lambda: "System poetry (system prompt): ~/config/system.md" in plain() and
                         "SMOKE_HEAD custom system prompt." in plain(), "/poetry output")
        check(len(requests) == before, "/poetry contacted the model server")
        first_system = turn("First terminal turn")
        check(first_system.startswith("SMOKE_HEAD custom system prompt."),
              "the terminal turn did not send system.md")
        first_head = without_breakpoints(request_for("First terminal turn")["messages"][0])
        check("As you answer the user's questions" in first_head and "currentDate" in first_head,
              "the first message did not carry the head context")
        system_file.write_text("SMOKE_EDITED head for the second turn.\n")
        (project / "BATBOX.md").write_text("Smoke project rule added mid-Flight.\n")
        check(turn("Second terminal turn") == first_system,
              "an edit between turns changed the system prompt of the next request")
        second = request_for("Second terminal turn")
        check(without_breakpoints(second["messages"][0]) == first_head,
              "an edit between turns changed the first message of the next request")
        check("Smoke project rule added mid-Flight." in json.dumps(second["messages"][1:]),
              "the BATBOX.md edit did not reach the end of the conversation")
        session.send(b"/clear\r")
        session.drain(1.0)
        check(turn("Third terminal turn").startswith("SMOKE_EDITED head for the second turn.\n\n"),
              "the system.md edit did not apply after /clear")
        session.send(b"/exit\r")
        check(session.wait_for_exit() == 0, "the terminal did not exit cleanly")
    finally:
        session.close()


def main(binary):
    requests = []

    class Handler(BaseHTTPRequestHandler):
        def log_message(self, *_):
            pass

        def do_GET(self):
            self.send_error(404)

        def do_POST(self):
            body = json.loads(self.rfile.read(int(self.headers["Content-Length"])))
            requests.append(body)
            response = stream(body.get("model", ""), "POETRY_REPLY")
            self.send_response(200)
            self.send_header("Content-Type", "text/event-stream")
            self.send_header("Content-Length", str(len(response)))
            self.end_headers()
            self.wfile.write(response)

    server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    try:
        with tempfile.TemporaryDirectory(prefix="batbox-system-poetry-") as temporary:
            root = Path(temporary)
            config = root / "config"
            project = root / "project"
            state = root / "state"
            config.mkdir()
            project.mkdir()
            system_file = config / "system.md"
            (config / "native-client.json").write_text(json.dumps({
                "model": "poetry-fixture", "base_url": f"http://127.0.0.1:{server.server_port}",
                "state_directory": str(state), "mcpServers": {}, "auto_title": False,
                "thinking": False}))
            env = {key: value for key, value in os.environ.items()
                   if not key.startswith("BATBOX_") and key not in {"TMUX", "STY", "SSH_CONNECTION"}}
            env.update(TERM="xterm-256color", BATBOX_CONFIG_DIR=str(config), HOME=str(root),
                       NO_PROXY="127.0.0.1,localhost", no_proxy="127.0.0.1,localhost")

            def run(*arguments, extra=None):
                return subprocess.run([str(binary), *arguments], cwd=project,
                                      env={**env, **(extra or {})}, stdin=subprocess.DEVNULL,
                                      capture_output=True, text=True, timeout=90)

            def system_prompt(extra=None):
                before = len(requests)
                result = run("-p", "Say the poetry marker", extra=extra)
                check(result.returncode == 0 and "POETRY_REPLY" in result.stdout,
                      f"-p: exit {result.returncode}: {result.stdout}{result.stderr}")
                turns = requests[before:]
                check(len(turns) == 1, f"expected one request, saw {len(turns)}")
                blocks = turns[0].get("system")
                check(isinstance(blocks, list) and blocks, f"no system blocks: {blocks!r}")
                return "\n\n".join(block["text"] for block in blocks)

            built_in = system_prompt()
            check(built_in.startswith("\nYou are an interactive agent that helps users"),
                  "the built-in head did not lead: " + built_in[:120])

            shown = run("poetry")
            check(shown.returncode == 0 and
                  "System poetry (system prompt): built-in" in shown.stderr and
                  "batbox poetry export" in shown.stderr,
                  "batbox poetry: " + shown.stdout[:200] + shown.stderr)
            before_tools = shown.stdout.split("{{section:using_your_tools}}")[0]
            check(len(before_tools) > 1000 and built_in.startswith(before_tools),
                  "batbox poetry did not print the head that was sent")

            exported = run("poetry", "export")
            check(exported.returncode == 0 and "~/config/system.md" in exported.stdout and
                  system_file.read_bytes() == (REPOSITORY / "poetry" / "system.md").read_bytes(),
                  "export: " + exported.stdout + exported.stderr)
            refused = run("poetry", "export")
            check(refused.returncode == 1 and "--force" in refused.stderr,
                  "export over a file: " + refused.stdout + refused.stderr)
            check(run("poetry", "export", "--force").returncode == 0, "export --force failed")
            check(system_prompt() == built_in,
                  "the exported default did not send the built-in bytes")

            system_file.write_text(CUSTOM)
            custom = system_prompt()
            check(custom.startswith("SMOKE_HEAD custom system prompt.\n\n# Environment\n"),
                  "system.md did not become the head: " + custom[:200])
            tail = custom.find("SMOKE_TAIL after the environment. {{section:nope}}")
            check(tail > custom.find("# Environment") and custom.count("# Environment") == 1,
                  "the environment section was not placed once before the tail")
            check(custom.find("When working with tool results") > tail,
                  "unreferenced sections were not appended after the file")
            check("interactive agent that helps users" not in custom and "<!--" not in custom,
                  "the built-in head or a comment leaked into the override")
            log = (state / "native.log").read_text(errors="replace")
            check(log.count("Unknown placeholder left as text: {{section:nope}}") == 1,
                  "the unknown placeholder was not logged once in native.log")

            shown = run("poetry")
            check("System poetry (system prompt): ~/config/system.md · "
                  f"{len(CUSTOM.encode())} bytes · 1 placeholder" in shown.stderr and
                  "Warning: Unknown placeholder left as text: {{section:nope}}" in shown.stderr and
                  shown.stdout.startswith("SMOKE_HEAD"),
                  "batbox poetry with a file: " + shown.stderr)
            doctor = run("doctor")
            check("System poetry: ~/config/system.md · " in doctor.stdout and
                  "warning: Unknown placeholder left as text: {{section:nope}}" in doctor.stdout,
                  "batbox doctor: " + doctor.stdout[-800:])

            system_file.write_text("<!-- empty -->\n\n")
            shown = run("poetry")
            check("built-in" in shown.stderr and "is empty" in shown.stderr,
                  "an empty file was not reported: " + shown.stderr)
            check(system_prompt() == built_in, "an empty system.md changed the prompt")

            system_file.write_text(CUSTOM)
            terminal(binary, project, env, system_file, requests)

            simple = system_prompt({"BATBOX_SIMPLE": "1"})
            check(simple.startswith("You are Batbox, an interactive coding assistant.") and
                  "SMOKE_HEAD" not in simple,
                  "BATBOX_SIMPLE did not keep precedence: " + simple[:200])
            print("system.md replaced the head on the wire; export, poetry, doctor, /poetry, a "
                  "stable prefix across mid-Flight edits, /clear, empty file "
                  "and BATBOX_SIMPLE behaved")
    finally:
        server.shutdown()
        server.server_close()


if __name__ == "__main__":
    main(Path(sys.argv[1]).resolve(strict=True))
